/* usb_stream.c - USB CDC streamer (PICO355-R5 sample batches, PICO355-R6 responses).
 * Frame layout: protocol.c only (protocol_encode_* / protocol_decode_*). No SDK or tusb
 * include here; bytes leave through usb_stream_hal_t.
 *
 * TBD rule (docs/protocol.md): a PROTOCOL_ERR_TBD from the encoder becomes
 * USB_STREAM_ERR_TBD; then nothing is written and no sample is removed from the buffer
 * (*sent = 0). Samples are popped into a local copy before encoding; if encoding fails
 * they are pushed back in their original order so the buffer is unchanged.
 */
#include "usb_stream.h"

usb_stream_rc_t usb_stream_init(usb_stream_t *us, const usb_stream_hal_t *hal,
                                uint8_t *frame_buf, size_t frame_cap)
{
    if (us == NULL || hal == NULL || hal->write == NULL || hal->read == NULL || frame_buf == NULL)
        return USB_STREAM_ERR_ARG;
    us->hal = hal;
    us->frame_buf = frame_buf;
    us->frame_cap = frame_cap;
    return USB_STREAM_OK;
}

/* Map a protocol encoder result to the streamer result (TBD rule above). */
static usb_stream_rc_t map_encode_rc(protocol_rc_t rc)
{
    if (rc == PROTOCOL_OK) return USB_STREAM_OK;
    if (rc == PROTOCOL_ERR_TBD) return USB_STREAM_ERR_TBD;
    return USB_STREAM_ERR_ENCODE;
}

/* Send len encoded bytes in one hal call; a short write is USB_STREAM_ERR_WRITE. */
static usb_stream_rc_t send_frame(usb_stream_t *us, size_t len)
{
    size_t wrote = us->hal->write(us->hal->ctx, us->frame_buf, len);
    return (wrote == len) ? USB_STREAM_OK : USB_STREAM_ERR_WRITE;
}

usb_stream_rc_t usb_stream_send_batch(usb_stream_t *us, sample_buffer_t *buf,
                                      size_t max_samples, size_t *sent)
{
    /* static: 64 x 12 B would take 768 B of the 2 KB default RP2040 stack; one streamer sends at a time */
    static adxl355_sample_t batch[CFG_SAMPLE_BUFFER_DEPTH]; /* buffer never holds more than its depth */
    size_t n = 0, len = 0, i;
    protocol_rc_t prc;
    usb_stream_rc_t rc;

    if (us == NULL || buf == NULL || sent == NULL) return USB_STREAM_ERR_ARG;
    *sent = 0;
    if (max_samples == 0) return USB_STREAM_OK;

    /* Take up to max_samples (bounded by depth) out of the buffer, oldest first. */
    while (n < max_samples && n < CFG_SAMPLE_BUFFER_DEPTH &&
           sample_buffer_pop(buf, &batch[n]) == SAMPLE_BUFFER_OK)
        n++;
    if (n == 0) return USB_STREAM_OK;

    prc = protocol_encode_sample_batch(batch, n, us->frame_buf, us->frame_cap, &len);
    rc = map_encode_rc(prc);
    if (rc != USB_STREAM_OK) {
        /* Undo: push back in original order so the buffer is unchanged. */
        for (i = 0; i < n; i++)
            (void)sample_buffer_push(buf, &batch[i]);
        return rc;
    }

    rc = send_frame(us, len);
    if (rc == USB_STREAM_OK) *sent = n;
    return rc;
}

usb_stream_rc_t usb_stream_send_response(usb_stream_t *us, const protocol_response_t *resp)
{
    size_t len = 0;
    usb_stream_rc_t rc;

    if (us == NULL || resp == NULL) return USB_STREAM_ERR_ARG;
    rc = map_encode_rc(protocol_encode_response(resp, us->frame_buf, us->frame_cap, &len));
    if (rc != USB_STREAM_OK) return rc;
    return send_frame(us, len);
}

usb_stream_rc_t usb_stream_receive(usb_stream_t *us, uint8_t *buf, size_t cap, size_t *n)
{
    size_t got;

    if (us == NULL || buf == NULL || n == NULL) return USB_STREAM_ERR_ARG;
    got = us->hal->read(us->hal->ctx, buf, cap);
    *n = (got > cap) ? cap : got;
    return USB_STREAM_OK;
}
