/*
 * adxl355.h - SPI driver for the ADXL355B (model part spiDriver : SpiDriver).
 *
 * Requirements: PICO355-R4 (SPI0 reads gated by DRDY), PICO355-R2/PICO355-R6
 * (apply ODR, range, filter). Register knowledge lives only in adxl355_regs.h.
 * The bus is injected as adxl355_bus_t (main.c binds it to SDK SPI0 + CSN GP17),
 * so this module builds and tests on the host.
 *
 * Register values come from adxl355_regs.h, cited to the Analog Devices no-OS driver
 * (path + commit SHA in that header). Contract for every public function, checked in
 * this order:
 *   1. ADXL355_ERR_ARG: a NULL pointer, bus->transfer or bus->delay_ms NULL, or a value out of
 *      range (range not one of ADXL355_RANGE_2G/4G/8G, odr_hz not one of
 *      4000 2000 1000 500 250 125, filter not ADXL355_FILTER_UNSET or 0..ADXL355_HPF_CODE_MAX).
 *   2. ADXL355_ERR_TBD: a register entry (ADXL355_IS_TBD) the operation needs is
 *      still a sentinel (per operation, see the *_TBD lists in adxl355.c). No bus
 *      transfer is made; *out / *status are left unchanged; *n_read is set to 0.
 *      adxl355_init still binds dev->bus and loads dev->config with the defaults
 *      (CFG_DEFAULT_ODR_HZ, CFG_DEFAULT_RANGE, filter ADXL355_FILTER_UNSET while
 *      CFG_DEFAULT_FILTER is TBD) and leaves dev->initialised false.
 *   3. ADXL355_ERR_NOT_INIT: dev->initialised is false (any call but init).
 *   4. ADXL355_ERR_BUS: transfer() returned non-zero (or NVM_BUSY never cleared);
 *      ADXL355_ERR_ID: a DEVID_AD, DEVID_MST or PARTID value differs.
 *   5. ADXL355_OK.
 * With the header fully cited ADXL355_REGS_ARE_TBD is 0 and these sequences run
 * (init: reset, wait NVM_BUSY, ID check; configure: standby, RANGE, ODR/LPF, HPF,
 * restore; set_measuring; read_sample: X, Y, Z reads; read_fifo: entry count and
 * one burst; status). The TBD paths are tested against generated all-TBD and
 * partial register headers (test_adxl355_tbd.c, test_adxl355_partial.c).
 * ADXL355_ERR_NOT_IMPLEMENTED is returned only by the A1 stub bodies.
 * adxl355_set_fifo_watermark and adxl355_fifo_parse follow the same order (ARG, TBD, NOT_INIT where a dev is used).
 *
 * Header deps: stdint, stdbool, stddef.
 */
#ifndef PICO355_ADXL355_H
#define PICO355_ADXL355_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Return codes of the adxl355 module. */
typedef enum {
    ADXL355_OK = 0,
    ADXL355_ERR_ARG,             /**< NULL pointer or value out of range */
    ADXL355_ERR_BUS,             /**< bus transfer callback reported failure */
    ADXL355_ERR_ID,              /**< device ID read back did not match */
    ADXL355_ERR_TBD,             /**< needed register entry is still TODO(TBD) in adxl355_regs.h; no transfer made */
    ADXL355_ERR_NOT_INIT,        /**< adxl355_init has not succeeded on this dev */
    ADXL355_ERR_NOT_IMPLEMENTED  /**< stub body only */
} adxl355_rc_t;

/** Measurement range, mirrors model enum AccelRange; UNSET = TBD default. */
typedef enum {
    ADXL355_RANGE_UNSET = 0,
    ADXL355_RANGE_2G,
    ADXL355_RANGE_4G,
    ADXL355_RANGE_8G
} adxl355_range_t;

/** One raw X, Y, Z sample (model item AccelSample: xRaw, yRaw, zRaw), sign-extended 20-bit. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
} adxl355_sample_t;

/** Sensor configuration (model item ConfigCommand attributes). */
typedef struct {
    uint32_t odr_hz;         /**< output data rate; default CFG_DEFAULT_ODR_HZ (PICO355-R1) */
    adxl355_range_t range;   /**< measurement range */
    int16_t filter;          /**< high-pass corner code 0..ADXL355_HPF_CODE_MAX (no-OS enum adxl355_hpf_corner, H:145-153); ADXL355_FILTER_UNSET = HPF left unchanged, default TODO(TBD) (filter settings exposed) */
} adxl355_config_t;

/** adxl355_config_t.filter sentinel "not configured" (no legal code is negative). */
#define ADXL355_FILTER_UNSET (-1)

/**
 * Bus operations. transfer() asserts CSN, clocks len bytes out of tx and into rx
 * (rx may be NULL), releases CSN. Returns 0 on success, non-zero on failure.
delay_ms() blocks for at least ms milliseconds; adxl355_init needs it for the
 * post-soft-reset delay (CFG_ADXL355_RESET_DELAY_MS, cited: no-OS adxl355.c
 * C:297-298 no_os_mdelay(1)); adxl355_init returns ADXL355_ERR_ARG if it is NULL.
 */
typedef struct {
    int (*transfer)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);
    void *ctx;
    void (*delay_ms)(void *ctx, uint32_t ms);
} adxl355_bus_t;

/**
 * Optional DMA FIFO drain, bound with adxl355_set_fifo_hal after adxl355_init (kept apart from
 * adxl355_bus_t so a three-member bus initialiser stays valid). read() runs ONE CSN-low window of
 * len bytes: every MOSI byte is tx_byte (the TX DMA channel repeats one byte, read_increment false;
 * pico-examples spi_dma.c:74) and every MISO byte goes to rx[0..len) (RX DMA, write_increment true;
 * both channels started together with dma_start_channel_mask, spi_dma.c:85). rx[0] is the dummy byte
 * clocked while the command byte goes out; the FIFO data is rx[1..len). Returns 0 on success.
 * With no hal, adxl355_read_fifo falls back to one bus->transfer() with the same tx bytes and
 * length, so the host tests need no DMA.
 */
typedef struct {
    int (*read)(void *ctx, uint8_t tx_byte, uint8_t *rx, size_t len);
    void *ctx;
} adxl355_fifo_hal_t;

/** Driver instance; caller owns storage. */
typedef struct {
    const adxl355_bus_t *bus;
    const adxl355_fifo_hal_t *fifo_hal;  /**< DMA drain hook or NULL (transfer() fallback); NULL after adxl355_init */
    adxl355_config_t config;
    bool initialised;
    uint8_t fifo_watermark;    /**< entries; adxl355_read_fifo drains only at >= this; 0 = no gate (set by adxl355_set_fifo_watermark) */
    uint32_t fifo_overflows;   /**< STATUS reads that showed FIFO_OVR since init; 32 bit, saturates at UINT32_MAX (design decision (owner t39u)) */
} adxl355_t;

/** Bind the bus, load default config, reset the part and check its IDs (contract above). */
adxl355_rc_t adxl355_init(adxl355_t *dev, const adxl355_bus_t *bus);

/** Write ODR, range and filter (model action configureSensor); on OK dev->config = *cfg. */
adxl355_rc_t adxl355_configure(adxl355_t *dev, const adxl355_config_t *cfg);

/** Start (true) or stop (false) measurement. */
adxl355_rc_t adxl355_set_measuring(adxl355_t *dev, bool on);

/** Read one sample from the data registers. */
adxl355_rc_t adxl355_read_sample(adxl355_t *dev, adxl355_sample_t *out);

/**
 * Drain the FIFO (design decision (owner t39u) for the trigger and watermark, see docs/protocol.md):
 * read FIFO_ENTRIES first (no-OS C:664-668); below dev->fifo_watermark, or below one XYZ set, return
 * OK with *n_read = 0 and no drain. Otherwise whole sets only (entries rounded down to a multiple of 3,
 * no-OS C:714; the tail stays in the FIFO), at most ADXL355_FIFO_CAPACITY entries and at most max sets;
 * one CS-low window of 1 + entries x 3 bytes through fifo_hal->read (DMA) or the transfer() fallback;
 * then adxl355_fifo_parse on rx[1..]. *n_read gets the number of samples stored in out.
 */
adxl355_rc_t adxl355_read_fifo(adxl355_t *dev, adxl355_sample_t *out, size_t max, size_t *n_read);

/**
 * Parse raw FIFO bytes (3 bytes per entry, no dummy) into whole XYZ samples (no-OS C:723-731):
 * an entry whose third byte has bit 0 (X marker) set and bit 1 (empty) clear starts a set; the
 * next two entries are its Y and Z and must be neither an X marker nor empty, else the set is
 * partial and dropped and scanning goes on. Entries before the first X marker, empty entries and
 * a trailing partial set are discarded. *n_out = sets stored (at most max). ADXL355_ERR_TBD if
 * a marker entry is TBD (*n_out = 0); out is untouched where no sample is stored.
 */
adxl355_rc_t adxl355_fifo_parse(const uint8_t *raw, size_t n_entries, adxl355_sample_t *out, size_t max, size_t *n_out);

/** Bind (or, with NULL, clear) the DMA FIFO drain hook; call after adxl355_init. */
adxl355_rc_t adxl355_set_fifo_hal(adxl355_t *dev, const adxl355_fifo_hal_t *hal);

/** Write the FIFO_SAMPLES watermark (no-OS C:678-692); entries > ADXL355_FIFO_SAMPLES_MAX is ADXL355_ERR_ARG. */
adxl355_rc_t adxl355_set_fifo_watermark(adxl355_t *dev, uint8_t entries);

/** Read the raw STATUS register into *status; a set FIFO_OVR bit adds 1 to dev->fifo_overflows (saturating). */
adxl355_rc_t adxl355_status(adxl355_t *dev, uint8_t *status);

#endif /* PICO355_ADXL355_H */
