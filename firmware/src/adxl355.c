/* adxl355.c - ADXL355B SPI driver (PICO355-R4). Register values: adxl355_regs.h only.
 *
 * Check order per adxl355.h: ARG, TBD, NOT_INIT, BUS/ID, OK. While
 * ADXL355_REGS_ARE_TBD is 1 every valid call returns ADXL355_ERR_TBD before any
 * bus transfer and leaves its outputs unchanged (*n_read is set to 0 by
 * adxl355_read_fifo). The register sequences after the TBD gate are TODO(TBD):
 * they are written only once the entries they use are cited in adxl355_regs.h.
 */
#include "adxl355.h"
#include "adxl355_regs.h"
#include "config.h"

static bool range_is_legal(adxl355_range_t r)
{
    return r == ADXL355_RANGE_2G || r == ADXL355_RANGE_4G || r == ADXL355_RANGE_8G;
}

adxl355_rc_t adxl355_init(adxl355_t *dev, const adxl355_bus_t *bus)
{
    if (dev == NULL || bus == NULL || bus->transfer == NULL) return ADXL355_ERR_ARG;
    dev->bus = bus;
    dev->config.odr_hz = CFG_DEFAULT_ODR_HZ;
    dev->config.range = CFG_DEFAULT_RANGE;
    dev->config.filter = CFG_IS_TBD(CFG_DEFAULT_FILTER) ? ADXL355_FILTER_UNSET : (int16_t)CFG_DEFAULT_FILTER;
    dev->initialised = false;
    if (ADXL355_REGS_ARE_TBD) return ADXL355_ERR_TBD; /* TODO(TBD) reset + ID check: needs cited RESET, DEVID_*, PARTID entries */
    return ADXL355_ERR_TBD; /* TODO(TBD) reset and ID check body, after the entries above are cited */
}

adxl355_rc_t adxl355_configure(adxl355_t *dev, const adxl355_config_t *cfg)
{
    if (dev == NULL || cfg == NULL) return ADXL355_ERR_ARG;
    if (!range_is_legal(cfg->range) || cfg->odr_hz == 0) return ADXL355_ERR_ARG;
    if (ADXL355_REGS_ARE_TBD) return ADXL355_ERR_TBD; /* TODO(TBD) range, ODR/LPF and HPF register sequence not cited */
    if (!dev->initialised) return ADXL355_ERR_NOT_INIT;
    return ADXL355_ERR_TBD; /* TODO(TBD) write sequence; dev->config = *cfg only on OK */
}

adxl355_rc_t adxl355_set_measuring(adxl355_t *dev, bool on)
{
    (void)on;
    if (dev == NULL) return ADXL355_ERR_ARG;
    if (ADXL355_REGS_ARE_TBD) return ADXL355_ERR_TBD; /* TODO(TBD) POWER_CTL standby bit not cited */
    if (!dev->initialised) return ADXL355_ERR_NOT_INIT;
    return ADXL355_ERR_TBD; /* TODO(TBD) POWER_CTL write sequence */
}

adxl355_rc_t adxl355_read_sample(adxl355_t *dev, adxl355_sample_t *out)
{
    if (dev == NULL || out == NULL) return ADXL355_ERR_ARG;
    if (ADXL355_REGS_ARE_TBD) return ADXL355_ERR_TBD; /* TODO(TBD) XDATA3 layout and data format not cited */
    if (!dev->initialised) return ADXL355_ERR_NOT_INIT;
    return ADXL355_ERR_TBD; /* TODO(TBD) data-register burst read */
}

adxl355_rc_t adxl355_read_fifo(adxl355_t *dev, adxl355_sample_t *out, size_t max, size_t *n_read)
{
    if (dev == NULL || out == NULL || n_read == NULL) return ADXL355_ERR_ARG;
    *n_read = 0;
    (void)max;
    if (ADXL355_REGS_ARE_TBD) return ADXL355_ERR_TBD; /* TODO(TBD) FIFO port and entry count not cited */
    if (!dev->initialised) return ADXL355_ERR_NOT_INIT;
    return ADXL355_ERR_TBD; /* TODO(TBD) FIFO burst read */
}

adxl355_rc_t adxl355_status(adxl355_t *dev, uint8_t *status)
{
    if (dev == NULL || status == NULL) return ADXL355_ERR_ARG;
    if (ADXL355_REGS_ARE_TBD) return ADXL355_ERR_TBD; /* TODO(TBD) STATUS register not cited */
    if (!dev->initialised) return ADXL355_ERR_NOT_INIT;
    return ADXL355_ERR_TBD; /* TODO(TBD) STATUS read */
}
