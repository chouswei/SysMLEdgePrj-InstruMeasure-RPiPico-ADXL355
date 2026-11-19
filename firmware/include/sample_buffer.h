/*
 * sample_buffer.h - ring buffer of raw X, Y, Z samples (model part sampleBuffer : SampleBuffer).
 *
 * Requirement: PICO355-R5 (buffer samples between acquisition and USB streaming).
 * Capacity CFG_SAMPLE_BUFFER_DEPTH and overflow policy are TODO(TBD) in config.h.
 * The placeholder depth is 1 (CFG_SAMPLE_BUFFER_DEPTH_IS_TBD), so the buffer is
 * full after one push and overflow tests are trivial until the depth is decided.
 * While CFG_IS_TBD(CFG_SAMPLE_BUFFER_OVERFLOW_POLICY): a push on a full buffer
 * keeps the stored samples unchanged (the new one is not stored), increments the
 * overflow counter and returns SAMPLE_BUFFER_OVERFLOW. This is the not-configured
 * behaviour, not a decided drop-newest policy.
 * Header deps: stdint, stdbool, stddef, config.h, adxl355.h.
 */
#ifndef PICO355_SAMPLE_BUFFER_H
#define PICO355_SAMPLE_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "adxl355.h"
#include "config.h"

/** Return codes of the sample_buffer module. */
typedef enum {
    SAMPLE_BUFFER_OK = 0,
    SAMPLE_BUFFER_ERR_ARG,
    SAMPLE_BUFFER_EMPTY,             /**< pop on an empty buffer */
    SAMPLE_BUFFER_OVERFLOW,          /**< push on a full buffer; overflow counter incremented */
    SAMPLE_BUFFER_ERR_NOT_IMPLEMENTED
} sample_buffer_rc_t;

/** Ring buffer; caller owns storage. Fields are private to sample_buffer.c. */
typedef struct {
    adxl355_sample_t slots[CFG_SAMPLE_BUFFER_DEPTH];
    size_t head;
    size_t tail;
    size_t count;
    uint32_t overflows;
} sample_buffer_t;

/** Empty the buffer and clear the overflow counter. */
sample_buffer_rc_t sample_buffer_init(sample_buffer_t *buf);

/** Append one sample; on full see the overflow rule above (policy TODO(TBD)). */
sample_buffer_rc_t sample_buffer_push(sample_buffer_t *buf, const adxl355_sample_t *s);

/** Remove the oldest sample into *out. */
sample_buffer_rc_t sample_buffer_pop(sample_buffer_t *buf, adxl355_sample_t *out);

/** Number of samples held (0 for a NULL buffer). */
size_t sample_buffer_count(const sample_buffer_t *buf);

/** Number of overflow events since init (0 for a NULL buffer). */
uint32_t sample_buffer_overflows(const sample_buffer_t *buf);

#endif /* PICO355_SAMPLE_BUFFER_H */
