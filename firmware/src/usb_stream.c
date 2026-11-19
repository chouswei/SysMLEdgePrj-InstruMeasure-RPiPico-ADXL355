/* usb_stream.c - USB CDC streamer stubs (PICO355-R5). Frame layout: protocol.c only. */
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

usb_stream_rc_t usb_stream_send_batch(usb_stream_t *us, sample_buffer_t *buf,
                                      size_t max_samples, size_t *sent)
{
    if (us == NULL || buf == NULL || sent == NULL) return USB_STREAM_ERR_ARG;
    (void)max_samples;
    *sent = 0;
    return USB_STREAM_ERR_NOT_IMPLEMENTED;
}

usb_stream_rc_t usb_stream_send_response(usb_stream_t *us, const protocol_response_t *resp)
{
    if (us == NULL || resp == NULL) return USB_STREAM_ERR_ARG;
    return USB_STREAM_ERR_NOT_IMPLEMENTED;
}

usb_stream_rc_t usb_stream_receive(usb_stream_t *us, uint8_t *buf, size_t cap, size_t *n)
{
    if (us == NULL || buf == NULL || n == NULL) return USB_STREAM_ERR_ARG;
    (void)cap;
    *n = 0;
    return USB_STREAM_ERR_NOT_IMPLEMENTED;
}
