/* acquisition.c - DRDY-gated acquisition stubs (PICO355-R4). */
#include "acquisition.h"

acq_rc_t acq_init(acq_t *acq, const acq_hal_t *hal, adxl355_t *dev, sample_buffer_t *buf)
{
    if (acq == NULL || hal == NULL || hal->drdy_read == NULL || dev == NULL || buf == NULL) return ACQ_ERR_ARG;
    acq->hal = hal;
    acq->dev = dev;
    acq->buf = buf;
    return ACQ_OK;
}

acq_rc_t acq_step(acq_t *acq, size_t *pushed)
{
    if (acq == NULL || pushed == NULL) return ACQ_ERR_ARG;
    *pushed = 0;
    return ACQ_ERR_NOT_IMPLEMENTED;
}
