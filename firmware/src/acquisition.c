/* acquisition.c - DRDY-gated acquisition (PICO355-R4). */
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
    /* static: 64 x 12 B would take 768 B of the 2 KB default RP2040 stack; one acq_t runs at a time */
    static adxl355_sample_t batch[CFG_SAMPLE_BUFFER_DEPTH];
    size_t n_read = 0;
    size_t i;
    bool overflow = false;
    adxl355_rc_t rc;

    if (acq == NULL || pushed == NULL) return ACQ_ERR_ARG;
    *pushed = 0;

    /* DRDY low: nothing to read, no bus traffic (PICO355-R4). */
    if (!acq->hal->drdy_read(acq->hal->ctx)) return ACQ_OK;

    /* Burst-read the FIFO into a local batch sized to the buffer depth. */
    rc = adxl355_read_fifo(acq->dev, batch, CFG_SAMPLE_BUFFER_DEPTH, &n_read);
    if (rc == ADXL355_ERR_TBD) return ACQ_ERR_TBD;   /* TBD rule 3: propagate, buffer untouched */
    if (rc != ADXL355_OK || n_read > CFG_SAMPLE_BUFFER_DEPTH) return ACQ_ERR_SENSOR;

    /* After a drain, read STATUS once so a FIFO_OVR is counted (adxl355_status; design decision (owner t39u)).
     * Below the watermark nothing was drained and no STATUS read is made. */
    if (n_read > 0) {
        uint8_t status;
        rc = adxl355_status(acq->dev, &status);
        if (rc == ADXL355_ERR_TBD) return ACQ_ERR_TBD;
        if (rc != ADXL355_OK) return ACQ_ERR_SENSOR;
    }

    for (i = 0; i < n_read; i++) {
        sample_buffer_rc_t brc = sample_buffer_push(acq->buf, &batch[i]);
        if (brc == SAMPLE_BUFFER_OVERFLOW) {
            overflow = true;
        } else if (brc == SAMPLE_BUFFER_OK) {
            (*pushed)++;
        } else {
            return ACQ_ERR_SENSOR;
        }
    }
    return overflow ? ACQ_ERR_OVERFLOW : ACQ_OK;
}
