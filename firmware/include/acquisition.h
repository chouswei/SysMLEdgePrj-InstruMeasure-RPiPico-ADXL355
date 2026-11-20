/*
 * acquisition.h - DRDY-gated acquisition (model part drdyAcquisition : DrdyAcquisition).
 *
 * Requirement: PICO355-R4 (read samples over SPI0 only when DRDY on GP22 signals
 * new data; INT1/INT2 unconnected). DRDY edge and polling vs interrupt: TODO(TBD).
 * FIFO drain: on a DRDY high the driver reads FIFO_ENTRIES and drains at or above the watermark
 * (CFG_FIFO_WATERMARK, CFG_FIFO_TRIGGER, design decision (owner t39u)); after a drain STATUS is read once
 * so FIFO_OVR is counted in the driver. TBD rule: an ADXL355_ERR_TBD
 * from the driver is returned as ACQ_ERR_TBD with *pushed = 0 and the buffer
 * untouched; DRDY low returns ACQ_OK with *pushed = 0. main.c binds acq_hal_t to the
 * SDK GPIO read of CFG_PIN_DRDY_IN (input only, never driven).
 * Header deps: stdint, stdbool, stddef, adxl355.h, sample_buffer.h.
 */
#ifndef PICO355_ACQUISITION_H
#define PICO355_ACQUISITION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "adxl355.h"
#include "sample_buffer.h"

/** Return codes of the acquisition module. */
typedef enum {
    ACQ_OK = 0,              /**< step ran; *pushed may be 0 when DRDY was low */
    ACQ_ERR_ARG,
    ACQ_ERR_SENSOR,          /**< adxl355 call failed (other than ADXL355_ERR_TBD) */
    ACQ_ERR_TBD,             /**< adxl355 returned ADXL355_ERR_TBD (registers or config still TODO(TBD)) */
    ACQ_ERR_OVERFLOW,        /**< sample buffer overflowed during this step */
    ACQ_ERR_NOT_IMPLEMENTED
} acq_rc_t;

/** Hardware abstraction: drdy_read() returns the DRDY level (true = high). */
typedef struct {
    bool (*drdy_read)(void *ctx);
    void *ctx;
} acq_hal_t;

/** Acquisition instance; caller owns storage. */
typedef struct {
    const acq_hal_t *hal;
    adxl355_t *dev;
    sample_buffer_t *buf;
} acq_t;

/** Bind hal, sensor and buffer. */
acq_rc_t acq_init(acq_t *acq, const acq_hal_t *hal, adxl355_t *dev, sample_buffer_t *buf);

/** One acquisition step (model action acquireOnDrdy): if DRDY, read samples and push them; *pushed = count. */
acq_rc_t acq_step(acq_t *acq, size_t *pushed);

#endif /* PICO355_ACQUISITION_H */
