/* adxl355.c - ADXL355B SPI driver stubs (PICO355-R4). Register values: adxl355_regs.h only. */
#include "adxl355.h"
#include "adxl355_regs.h"
#include "config.h"

adxl355_rc_t adxl355_init(adxl355_t *dev, const adxl355_bus_t *bus)
{
    if (dev == NULL || bus == NULL || bus->transfer == NULL) return ADXL355_ERR_ARG;
    dev->bus = bus;
    dev->config.odr_hz = CFG_DEFAULT_ODR_HZ;
    dev->config.range = CFG_DEFAULT_RANGE;
    dev->config.filter = CFG_IS_TBD(CFG_DEFAULT_FILTER) ? ADXL355_FILTER_UNSET : (int16_t)CFG_DEFAULT_FILTER;
    dev->initialised = false;
    return ADXL355_ERR_NOT_IMPLEMENTED;
}

adxl355_rc_t adxl355_configure(adxl355_t *dev, const adxl355_config_t *cfg)
{
    if (dev == NULL || cfg == NULL) return ADXL355_ERR_ARG;
    return ADXL355_ERR_NOT_IMPLEMENTED;
}

adxl355_rc_t adxl355_set_measuring(adxl355_t *dev, bool on)
{
    (void)on;
    if (dev == NULL) return ADXL355_ERR_ARG;
    return ADXL355_ERR_NOT_IMPLEMENTED;
}

adxl355_rc_t adxl355_read_sample(adxl355_t *dev, adxl355_sample_t *out)
{
    if (dev == NULL || out == NULL) return ADXL355_ERR_ARG;
    return ADXL355_ERR_NOT_IMPLEMENTED;
}

adxl355_rc_t adxl355_read_fifo(adxl355_t *dev, adxl355_sample_t *out, size_t max, size_t *n_read)
{
    if (dev == NULL || out == NULL || n_read == NULL) return ADXL355_ERR_ARG;
    (void)max;
    *n_read = 0;
    return ADXL355_ERR_NOT_IMPLEMENTED;
}

adxl355_rc_t adxl355_status(adxl355_t *dev, uint8_t *status)
{
    if (dev == NULL || status == NULL) return ADXL355_ERR_ARG;
    return ADXL355_ERR_NOT_IMPLEMENTED;
}
