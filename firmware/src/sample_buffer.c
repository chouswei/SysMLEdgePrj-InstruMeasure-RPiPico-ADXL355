/* sample_buffer.c - raw sample ring buffer (PICO355-R5). Policy per sample_buffer.h. */
#include "sample_buffer.h"

sample_buffer_rc_t sample_buffer_init(sample_buffer_t *buf)
{
    if (buf == NULL) return SAMPLE_BUFFER_ERR_ARG;
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    buf->overflows = 0;
    return SAMPLE_BUFFER_OK;
}

sample_buffer_rc_t sample_buffer_push(sample_buffer_t *buf, const adxl355_sample_t *s)
{
    if (buf == NULL || s == NULL) return SAMPLE_BUFFER_ERR_ARG;
    if (buf->count >= CFG_SAMPLE_BUFFER_DEPTH) {
        /* Full: policy TODO(TBD) in config.h; not-configured behaviour per sample_buffer.h
         * keeps the stored samples unchanged and does not store the new one. */
        buf->overflows++;
        return SAMPLE_BUFFER_OVERFLOW;
    }
    buf->slots[buf->head] = *s;
    buf->head = (buf->head + 1) % CFG_SAMPLE_BUFFER_DEPTH;
    buf->count++;
    return SAMPLE_BUFFER_OK;
}

sample_buffer_rc_t sample_buffer_pop(sample_buffer_t *buf, adxl355_sample_t *out)
{
    if (buf == NULL || out == NULL) return SAMPLE_BUFFER_ERR_ARG;
    if (buf->count == 0) return SAMPLE_BUFFER_EMPTY;
    *out = buf->slots[buf->tail];
    buf->tail = (buf->tail + 1) % CFG_SAMPLE_BUFFER_DEPTH;
    buf->count--;
    return SAMPLE_BUFFER_OK;
}

size_t sample_buffer_count(const sample_buffer_t *buf)
{
    return buf == NULL ? 0u : buf->count;
}

uint32_t sample_buffer_overflows(const sample_buffer_t *buf)
{
    return buf == NULL ? 0u : buf->overflows;
}
