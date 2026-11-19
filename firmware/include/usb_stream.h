/*
 * usb_stream.h - USB CDC streamer (model part usbCdcStreamer : UsbCdcStreamer).
 *
 * Requirement: PICO355-R5 (stream buffered raw X, Y, Z samples to the PC on
 * sampleStream). Also carries responses for PICO355-R6. Bytes go out through
 * usb_stream_hal_t, which main.c binds to SDK USB CDC. Frame layout belongs to
 * protocol.h only.
 * Header deps: stdint, stdbool, stddef, protocol.h, sample_buffer.h.
 */
#ifndef PICO355_USB_STREAM_H
#define PICO355_USB_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol.h"
#include "sample_buffer.h"

/** Return codes of the usb_stream module. */
typedef enum {
    USB_STREAM_OK = 0,
    USB_STREAM_ERR_ARG,
    USB_STREAM_ERR_ENCODE,       /**< protocol encode failed */
    USB_STREAM_ERR_WRITE,        /**< hal wrote fewer bytes than asked */
    USB_STREAM_ERR_NOT_IMPLEMENTED
} usb_stream_rc_t;

/**
 * Hardware abstraction. write() sends up to len bytes and returns the count sent;
 * read() copies up to cap received bytes into buf and returns the count (0 = none).
 */
typedef struct {
    size_t (*write)(void *ctx, const uint8_t *buf, size_t len);
    size_t (*read)(void *ctx, uint8_t *buf, size_t cap);
    void *ctx;
} usb_stream_hal_t;

/** Streamer instance; frame_buf is caller-owned scratch for one encoded frame. */
typedef struct {
    const usb_stream_hal_t *hal;
    uint8_t *frame_buf;
    size_t frame_cap;
} usb_stream_t;

/** Bind hal and scratch buffer. */
usb_stream_rc_t usb_stream_init(usb_stream_t *us, const usb_stream_hal_t *hal,
                                uint8_t *frame_buf, size_t frame_cap);

/** Pop up to max_samples from buf and send them as one sample-batch frame; *sent = samples sent. */
usb_stream_rc_t usb_stream_send_batch(usb_stream_t *us, sample_buffer_t *buf,
                                      size_t max_samples, size_t *sent);

/** Encode and send one response frame. */
usb_stream_rc_t usb_stream_send_response(usb_stream_t *us, const protocol_response_t *resp);

/** Read received bytes (commandChannel) into buf; *n = bytes read. */
usb_stream_rc_t usb_stream_receive(usb_stream_t *us, uint8_t *buf, size_t cap, size_t *n);

#endif /* PICO355_USB_STREAM_H */
