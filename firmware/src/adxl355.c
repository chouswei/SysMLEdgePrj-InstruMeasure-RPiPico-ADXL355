/* adxl355.c - ADXL355B SPI driver (PICO355-R4). Register values: adxl355_regs.h only.
 *
 * Check order per adxl355.h: ARG, TBD, NOT_INIT, BUS/ID, OK. An operation returns
 * ADXL355_ERR_TBD, before any bus transfer, while a register entry it needs is
 * still ADXL355_TBD (leaves its outputs unchanged; *n_read is set to 0 by
 * adxl355_read_fifo).
 *
 * Sequences follow the Analog Devices no-OS driver, commit
 * 4557b1f58f8a233bd116dbb02d8d5c447d3699b6, drivers/accel/adxl355/adxl355.c
 * (cited as no-OS C:<line>): command byte C:46/C:81, ID check C:137-160, soft
 * reset C:270-300, set_op_mode C:228-240, set_range C:335-355, set_odr_lpf
 * C:366-410, set_hpf_corner C:417-460, get_raw_xyz C:512-545, get_sts_reg
 * C:641-655, FIFO entries C:664-668, FIFO_SAMPLES write C:678-692, FIFO read C:705-736
 * (whole sets only C:714, X marker and empty tests C:724-725), sample conversion C:891-925.
 * The FIFO drain itself (one CS-low window, TX byte repeated, RX with a leading dummy) follows
 * pico-examples spi/spi_dma/spi_dma.c:74-85 and is bound to SPI0 DMA in main.c through
 * adxl355_bus_t.fifo_read; without that hook one transfer() carries the same bytes.
 *
 * The 1 ms post-reset delay is taken from no-OS C:297-298 (no_os_mdelay(1)) through the
 * bus delay_ms hook, CFG_ADXL355_RESET_DELAY_MS, after NVM_BUSY clears and before the ID
 * reads. Not taken from no-OS: the shadow-register compare after reset (no-OS C:300-308);
 * the driver keeps no shadow-register baseline, the ID check below stands in for it.
 */
#include "adxl355.h"
#include "adxl355_regs.h"
#include "config.h"

#define T(x) ADXL355_IS_TBD(x)

/* Entries each operation needs (constant expressions). */
#define IO_TBD (T(ADXL355_SPI_ADDR_SHIFT) || T(ADXL355_SPI_READ_BIT) || T(ADXL355_SPI_WRITE_BIT))
#define INIT_TBD (IO_TBD || T(ADXL355_REG_RESET) || T(ADXL355_RESET_CODE) || \
    T(ADXL355_REG_STATUS) || T(ADXL355_STATUS_NVM_BUSY) || \
    T(ADXL355_REG_DEVID_AD) || T(ADXL355_REG_DEVID_MST) || T(ADXL355_REG_PARTID) || \
    T(ADXL355_DEVID_AD_EXPECTED) || T(ADXL355_DEVID_MST_EXPECTED) || T(ADXL355_PARTID_EXPECTED))
#define CONFIGURE_TBD (IO_TBD || T(ADXL355_REG_POWER_CTL) || T(ADXL355_POWER_CTL_STANDBY) || \
    T(ADXL355_REG_RANGE) || T(ADXL355_RANGE_MASK) || T(ADXL355_RANGE_CODE_2G) || \
    T(ADXL355_RANGE_CODE_4G) || T(ADXL355_RANGE_CODE_8G) || \
    T(ADXL355_REG_FILTER) || T(ADXL355_FILTER_ODR_LPF_MASK) || T(ADXL355_FILTER_HPF_MASK) || \
    T(ADXL355_FILTER_HPF_SHIFT) || T(ADXL355_HPF_CODE_MAX) || \
    T(ADXL355_ODR_CODE_4000HZ) || T(ADXL355_ODR_CODE_2000HZ) || T(ADXL355_ODR_CODE_1000HZ) || \
    T(ADXL355_ODR_CODE_500HZ) || T(ADXL355_ODR_CODE_250HZ) || T(ADXL355_ODR_CODE_125HZ))
#define MEASURING_TBD (IO_TBD || T(ADXL355_REG_POWER_CTL) || T(ADXL355_POWER_CTL_STANDBY) || \
    T(ADXL355_POWER_CTL_MEASURE_VAL))
#define SAMPLE_TBD (IO_TBD || T(ADXL355_REG_XDATA3) || T(ADXL355_REG_YDATA3) || \
    T(ADXL355_REG_ZDATA3) || T(ADXL355_SAMPLE_SHIFT))
#define PARSE_TBD (T(ADXL355_FIFO_X_MARKER) || T(ADXL355_FIFO_EMPTY) || T(ADXL355_SAMPLE_SHIFT))
#define FIFO_TBD (IO_TBD || PARSE_TBD || T(ADXL355_REG_FIFO_ENTRIES) || T(ADXL355_REG_FIFO_DATA) || \
    T(ADXL355_FIFO_CAPACITY))
#define WATERMARK_TBD (IO_TBD || T(ADXL355_REG_FIFO_SAMPLES) || T(ADXL355_FIFO_SAMPLES_MAX))
#define STATUS_TBD (IO_TBD || T(ADXL355_REG_STATUS) || T(ADXL355_STATUS_FIFO_OVR))

/* One FIFO sample is three 3-byte entries (X, Y, Z): no-OS C:716-735. */
#define FIFO_SAMPLE_BYTES 9u
/* Largest FIFO read: ADXL355_FIFO_SAMPLES_MAX (0x60) entries of 3 bytes, no-OS H:57 / H:279 (comm_buff 289). */
#define FIFO_MAX_BYTES 288u
#define XFER_MAX (1u + FIFO_MAX_BYTES)

static bool range_is_legal(adxl355_range_t r)
{
    return r == ADXL355_RANGE_2G || r == ADXL355_RANGE_4G || r == ADXL355_RANGE_8G;
}

/* Supported integer ODR values; codes in odr_code(). Fractional ODRs (no-OS H:162-166) are not exposed. */
static const uint32_t k_odr_hz[] = { 4000u, 2000u, 1000u, 500u, 250u, 125u };

static bool odr_is_legal(uint32_t hz)
{
    size_t i;
    for (i = 0; i < sizeof k_odr_hz / sizeof k_odr_hz[0]; i++) if (k_odr_hz[i] == hz) return true;
    return false;
}

static uint8_t odr_code(uint32_t hz)
{
    switch (hz) {
    case 4000u: return (uint8_t)ADXL355_ODR_CODE_4000HZ;
    case 2000u: return (uint8_t)ADXL355_ODR_CODE_2000HZ;
    case 1000u: return (uint8_t)ADXL355_ODR_CODE_1000HZ;
    case 500u:  return (uint8_t)ADXL355_ODR_CODE_500HZ;
    case 250u:  return (uint8_t)ADXL355_ODR_CODE_250HZ;
    default:    return (uint8_t)ADXL355_ODR_CODE_125HZ;
    }
}

static uint8_t range_code(adxl355_range_t r)
{
    switch (r) {
    case ADXL355_RANGE_2G: return (uint8_t)ADXL355_RANGE_CODE_2G;
    case ADXL355_RANGE_4G: return (uint8_t)ADXL355_RANGE_CODE_4G;
    default:               return (uint8_t)ADXL355_RANGE_CODE_8G;
    }
}

/* Read n bytes starting at reg: command byte (reg << shift) | read bit, then n clocked bytes (no-OS C:40-60). */
static adxl355_rc_t reg_read(const adxl355_bus_t *bus, uint8_t reg, uint8_t *out, size_t n)
{
    uint8_t tx[XFER_MAX] = {0};
    uint8_t rx[XFER_MAX] = {0};
    size_t i;
    if (n > FIFO_MAX_BYTES) return ADXL355_ERR_ARG;
    tx[0] = (uint8_t)((uint8_t)(reg << ((unsigned)ADXL355_SPI_ADDR_SHIFT & 7u)) | (uint8_t)ADXL355_SPI_READ_BIT);
    if (bus->transfer(bus->ctx, tx, rx, n + 1u) != 0) return ADXL355_ERR_BUS;
    for (i = 0; i < n; i++) out[i] = rx[i + 1u];
    return ADXL355_OK;
}

/* Write one byte to reg: command byte (reg << shift) | write bit, then the value (no-OS C:72-95). */
static adxl355_rc_t reg_write(const adxl355_bus_t *bus, uint8_t reg, uint8_t value)
{
    uint8_t tx[2];
    tx[0] = (uint8_t)((uint8_t)(reg << ((unsigned)ADXL355_SPI_ADDR_SHIFT & 7u)) | (uint8_t)ADXL355_SPI_WRITE_BIT);
    tx[1] = value;
    return bus->transfer(bus->ctx, tx, NULL, sizeof tx) != 0 ? ADXL355_ERR_BUS : ADXL355_OK;
}

/* Read, clear mask, set bits, write back (no-OS set_range C:341-352, set_odr_lpf C:395-402). */
static adxl355_rc_t reg_rmw(const adxl355_bus_t *bus, uint8_t reg, uint8_t mask, uint8_t bits)
{
    uint8_t v;
    adxl355_rc_t rc = reg_read(bus, reg, &v, 1u);
    if (rc != ADXL355_OK) return rc;
    v = (uint8_t)((v & (uint8_t)~mask) | (bits & mask));
    return reg_write(bus, reg, v);
}

/* 24-bit big-endian word, bits [23:4] are the 20-bit two's complement sample (no-OS C:896-902, C:921). */
static int32_t sample_from_bytes(const uint8_t *b)
{
    uint32_t raw = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[2];
    uint32_t sign = 1u << (ADXL355_SAMPLE_BITS - 1);
    raw >>= ((unsigned)ADXL355_SAMPLE_SHIFT & 31u); /* masked: a TBD sentinel never reaches here (gated) but must compile */
    return (int32_t)(raw ^ sign) - (int32_t)sign;
}

adxl355_rc_t adxl355_init(adxl355_t *dev, const adxl355_bus_t *bus)
{
    uint8_t v;
    uint8_t status;
    unsigned retries;
    adxl355_rc_t rc;

    if (dev == NULL || bus == NULL || bus->transfer == NULL || bus->delay_ms == NULL) return ADXL355_ERR_ARG;
    dev->bus = bus;
    dev->fifo_hal = NULL;
    dev->config.odr_hz = CFG_DEFAULT_ODR_HZ;
    dev->config.range = CFG_DEFAULT_RANGE;
    dev->config.filter = CFG_IS_TBD(CFG_DEFAULT_FILTER) ? ADXL355_FILTER_UNSET : (int16_t)CFG_DEFAULT_FILTER;
    dev->initialised = false;
    dev->fifo_watermark = 0u;
    dev->fifo_overflows = 0u;
    if (INIT_TBD) return ADXL355_ERR_TBD;

    /* Soft reset: write the reset code, then poll STATUS until NVM_BUSY clears, 255 tries (no-OS C:270-295). */
    rc = reg_write(bus, (uint8_t)ADXL355_REG_RESET, (uint8_t)ADXL355_RESET_CODE);
    if (rc != ADXL355_OK) return rc;
    retries = 255u;
    do {
        rc = reg_read(bus, (uint8_t)ADXL355_REG_STATUS, &status, 1u);
        if (rc != ADXL355_OK) return rc;
        if ((status & (uint8_t)ADXL355_STATUS_NVM_BUSY) == 0) break;
    } while (--retries != 0u);
    if (retries == 0u) return ADXL355_ERR_BUS;

    /* Delay between the soft reset and the next register read (cited: no-OS C:297-298). */
    bus->delay_ms(bus->ctx, (uint32_t)CFG_ADXL355_RESET_DELAY_MS);

    /* ID check: DEVID_AD, DEVID_MST, PARTID against their reset values (no-OS C:137-160). */
    rc = reg_read(bus, (uint8_t)ADXL355_REG_DEVID_AD, &v, 1u);
    if (rc != ADXL355_OK) return rc;
    if (v != (uint8_t)ADXL355_DEVID_AD_EXPECTED) return ADXL355_ERR_ID;
    rc = reg_read(bus, (uint8_t)ADXL355_REG_DEVID_MST, &v, 1u);
    if (rc != ADXL355_OK) return rc;
    if (v != (uint8_t)ADXL355_DEVID_MST_EXPECTED) return ADXL355_ERR_ID;
    rc = reg_read(bus, (uint8_t)ADXL355_REG_PARTID, &v, 1u);
    if (rc != ADXL355_OK) return rc;
    if (v != (uint8_t)ADXL355_PARTID_EXPECTED) return ADXL355_ERR_ID;

    dev->initialised = true;
    return ADXL355_OK;
}

adxl355_rc_t adxl355_configure(adxl355_t *dev, const adxl355_config_t *cfg)
{
    uint8_t prev;
    adxl355_rc_t rc;
    bool was_measuring;

    if (dev == NULL || cfg == NULL) return ADXL355_ERR_ARG;
    if (!range_is_legal(cfg->range) || !odr_is_legal(cfg->odr_hz)) return ADXL355_ERR_ARG;
    if (cfg->filter != ADXL355_FILTER_UNSET && (uint64_t)(int64_t)cfg->filter > ADXL355_HPF_CODE_MAX)
        return ADXL355_ERR_ARG;
    if (CONFIGURE_TBD) return ADXL355_ERR_TBD;
    if (!dev->initialised) return ADXL355_ERR_NOT_INIT;

    /* ODR/LPF and HPF are changed in standby only (no-OS C:366-380, C:417-433): read POWER_CTL,
     * enter standby if measuring, write RANGE, FILTER, then restore POWER_CTL. */
    rc = reg_read(dev->bus, (uint8_t)ADXL355_REG_POWER_CTL, &prev, 1u);
    if (rc != ADXL355_OK) return rc;
    was_measuring = (prev & (uint8_t)ADXL355_POWER_CTL_STANDBY) == 0;
    if (was_measuring) {
        rc = reg_write(dev->bus, (uint8_t)ADXL355_REG_POWER_CTL, (uint8_t)ADXL355_POWER_CTL_STANDBY);
        if (rc != ADXL355_OK) return rc;
    }
    rc = reg_rmw(dev->bus, (uint8_t)ADXL355_REG_RANGE, (uint8_t)ADXL355_RANGE_MASK, range_code(cfg->range));
    if (rc != ADXL355_OK) return rc;
    rc = reg_rmw(dev->bus, (uint8_t)ADXL355_REG_FILTER, (uint8_t)ADXL355_FILTER_ODR_LPF_MASK, odr_code(cfg->odr_hz));
    if (rc != ADXL355_OK) return rc;
    if (cfg->filter != ADXL355_FILTER_UNSET) {
        rc = reg_rmw(dev->bus, (uint8_t)ADXL355_REG_FILTER, (uint8_t)ADXL355_FILTER_HPF_MASK,
                     (uint8_t)((uint8_t)cfg->filter << ((unsigned)ADXL355_FILTER_HPF_SHIFT & 7u)));
        if (rc != ADXL355_OK) return rc;
    }
    if (was_measuring) {
        rc = reg_write(dev->bus, (uint8_t)ADXL355_REG_POWER_CTL, prev);
        if (rc != ADXL355_OK) return rc;
    }
    dev->config = *cfg;
    return ADXL355_OK;
}

adxl355_rc_t adxl355_set_measuring(adxl355_t *dev, bool on)
{
    if (dev == NULL) return ADXL355_ERR_ARG;
    if (MEASURING_TBD) return ADXL355_ERR_TBD;
    if (!dev->initialised) return ADXL355_ERR_NOT_INIT;
    /* POWER_CTL is written whole with an op mode: measurement = MEAS_TEMP_ON_DRDY_ON, stop = STDBY_TEMP_ON_DRDY_ON (no-OS H:130-131, C:228-240). */
    return reg_write(dev->bus, (uint8_t)ADXL355_REG_POWER_CTL,
                     on ? (uint8_t)ADXL355_POWER_CTL_MEASURE_VAL : (uint8_t)ADXL355_POWER_CTL_STANDBY);
}

adxl355_rc_t adxl355_read_sample(adxl355_t *dev, adxl355_sample_t *out)
{
    uint8_t b[3];
    adxl355_sample_t s;
    adxl355_rc_t rc;

    if (dev == NULL || out == NULL) return ADXL355_ERR_ARG;
    if (SAMPLE_TBD) return ADXL355_ERR_TBD;
    if (!dev->initialised) return ADXL355_ERR_NOT_INIT;
    /* One 3-byte read per axis, as no-OS get_raw_xyz (C:512-545). */
    rc = reg_read(dev->bus, (uint8_t)ADXL355_REG_XDATA3, b, 3u);
    if (rc != ADXL355_OK) return rc;
    s.x = sample_from_bytes(b);
    rc = reg_read(dev->bus, (uint8_t)ADXL355_REG_YDATA3, b, 3u);
    if (rc != ADXL355_OK) return rc;
    s.y = sample_from_bytes(b);
    rc = reg_read(dev->bus, (uint8_t)ADXL355_REG_ZDATA3, b, 3u);
    if (rc != ADXL355_OK) return rc;
    s.z = sample_from_bytes(b);
    *out = s;
    return ADXL355_OK;
}

/* FIFO entry helpers: third byte bit 0 = X marker, bit 1 = empty (no-OS C:724-725). */
static bool entry_is_x(const uint8_t *e)
{
    return (e[2] & (uint8_t)ADXL355_FIFO_X_MARKER) != 0 && (e[2] & (uint8_t)ADXL355_FIFO_EMPTY) == 0;
}
static bool entry_is_plain(const uint8_t *e)
{
    return (e[2] & (uint8_t)ADXL355_FIFO_X_MARKER) == 0 && (e[2] & (uint8_t)ADXL355_FIFO_EMPTY) == 0;
}

adxl355_rc_t adxl355_fifo_parse(const uint8_t *raw, size_t n_entries, adxl355_sample_t *out, size_t max, size_t *n_out)
{
    size_t i = 0;
    size_t cnt = 0;

    if (raw == NULL || out == NULL || n_out == NULL) return ADXL355_ERR_ARG;
    *n_out = 0;
    if (PARSE_TBD) return ADXL355_ERR_TBD;
    /* Align to an X marker; a set needs X, then a Y and a Z entry that are neither X nor empty.
     * Otherwise the X is a partial set (FIFO overwritten, or the stream ends): drop it, rescan from the next entry. */
    while (i + 3u <= n_entries && cnt < max) {
        const uint8_t *e = &raw[i * 3u];
        if (!entry_is_x(e) || !entry_is_plain(e + 3) || !entry_is_plain(e + 6)) {
            i++;
            continue;
        }
        out[cnt].x = sample_from_bytes(e);
        out[cnt].y = sample_from_bytes(e + 3);
        out[cnt].z = sample_from_bytes(e + 6);
        cnt++;
        i += 3u;
    }
    *n_out = cnt;
    return ADXL355_OK;
}

adxl355_rc_t adxl355_read_fifo(adxl355_t *dev, adxl355_sample_t *out, size_t max, size_t *n_read)
{
    uint8_t entries_reg;
    uint8_t rx[XFER_MAX];
    size_t entries, sets, len;
    uint8_t cmd;
    adxl355_rc_t rc;

    if (dev == NULL || out == NULL || n_read == NULL) return ADXL355_ERR_ARG;
    *n_read = 0;
    if (FIFO_TBD) return ADXL355_ERR_TBD;
    if (!dev->initialised) return ADXL355_ERR_NOT_INIT;

    /* FIFO_ENTRIES first (no-OS C:664-668); capped at the cited capacity (README.rst:92, 96 entries). */
    rc = reg_read(dev->bus, (uint8_t)ADXL355_REG_FIFO_ENTRIES, &entries_reg, 1u);
    if (rc != ADXL355_OK) return rc;
    entries = entries_reg;
    if (entries > (size_t)ADXL355_FIFO_CAPACITY) entries = (size_t)ADXL355_FIFO_CAPACITY;
    /* Trigger (design decision (owner t39u)): drain only at or above the watermark. */
    if (dev->fifo_watermark != 0u && entries < dev->fifo_watermark) return ADXL355_OK;

    /* Whole XYZ sets only: round down to a multiple of 3 entries (no-OS C:714), the tail stays in the FIFO;
     * never more sets than the caller can store, so nothing drained is lost. */
    sets = entries / 3u;
    if (sets > max) sets = max;
    if (sets == 0u) return ADXL355_OK;
    entries = sets * 3u;

    /* One CS-low window: 1 leading dummy byte + entries x 3 bytes, TX = FIFO_DATA read byte repeated. */
    len = 1u + entries * 3u;
    cmd = (uint8_t)((uint8_t)((uint8_t)ADXL355_REG_FIFO_DATA << ((unsigned)ADXL355_SPI_ADDR_SHIFT & 7u)) |
                    (uint8_t)ADXL355_SPI_READ_BIT);
    if (dev->fifo_hal != NULL && dev->fifo_hal->read != NULL) {
        if (dev->fifo_hal->read(dev->fifo_hal->ctx, cmd, rx, len) != 0) return ADXL355_ERR_BUS;
    } else {
        uint8_t tx[XFER_MAX];
        size_t i;
        for (i = 0; i < len; i++) tx[i] = cmd;
        if (dev->bus->transfer(dev->bus->ctx, tx, rx, len) != 0) return ADXL355_ERR_BUS;
    }
    return adxl355_fifo_parse(&rx[1], entries, out, max, n_read);
}

adxl355_rc_t adxl355_set_fifo_hal(adxl355_t *dev, const adxl355_fifo_hal_t *hal)
{
    if (dev == NULL) return ADXL355_ERR_ARG;
    dev->fifo_hal = hal;
    return ADXL355_OK;
}

/* a function, not a constant expression: with a TBD sentinel the comparison below is gated out but must compile */
static size_t fifo_samples_max(void) { return (size_t)ADXL355_FIFO_SAMPLES_MAX; }

adxl355_rc_t adxl355_set_fifo_watermark(adxl355_t *dev, uint8_t entries)
{
    if (dev == NULL) return ADXL355_ERR_ARG;
    if (WATERMARK_TBD) return ADXL355_ERR_TBD;
    if ((size_t)entries > fifo_samples_max()) return ADXL355_ERR_ARG;   /* no-OS C:682 */
    if (!dev->initialised) return ADXL355_ERR_NOT_INIT;
    if (reg_write(dev->bus, (uint8_t)ADXL355_REG_FIFO_SAMPLES, entries) != ADXL355_OK) return ADXL355_ERR_BUS;
    dev->fifo_watermark = entries;
    return ADXL355_OK;
}

adxl355_rc_t adxl355_status(adxl355_t *dev, uint8_t *status)
{
    uint8_t v;
    adxl355_rc_t rc;

    if (dev == NULL || status == NULL) return ADXL355_ERR_ARG;
    if (STATUS_TBD) return ADXL355_ERR_TBD;
    if (!dev->initialised) return ADXL355_ERR_NOT_INIT;
    rc = reg_read(dev->bus, (uint8_t)ADXL355_REG_STATUS, &v, 1u);
    if (rc != ADXL355_OK) return rc;
    /* FIFO_OVR (no-OS H:222) is counted once per STATUS read that shows it; a STATUS read clears the
     * overflow interrupt (no-OS README.rst:127). 32 bit, saturating. Design decision (owner t39u). */
    if ((v & (uint8_t)ADXL355_STATUS_FIFO_OVR) != 0 && dev->fifo_overflows != UINT32_MAX) dev->fifo_overflows++;
    *status = v;
    return ADXL355_OK;
}
