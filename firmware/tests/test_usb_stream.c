/*
 * test_usb_stream.c - host test for firmware/src/usb_stream.c on the REAL config.h frame layout
 * (PICO355-R5, PICO355-R6). Mock hal records written bytes and serves scripted receive bytes.
 */
#include <stdio.h>
#include <string.h>

#include "usb_stream.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)


static uint8_t g_written[1024];
static size_t g_written_len;
static int g_write_calls;
static size_t g_short_by;            /* hal reports this many fewer bytes than asked */
static const uint8_t *g_rx_data;
static size_t g_rx_len;

static size_t mock_write(void *ctx, const uint8_t *buf, size_t len)
{
    (void)ctx;
    g_write_calls++;
    if (g_written_len + len <= sizeof g_written) {
        memcpy(g_written + g_written_len, buf, len);
        g_written_len += len;
    }
    return len > g_short_by ? len - g_short_by : 0;
}
static size_t mock_read(void *ctx, uint8_t *buf, size_t cap)
{
    size_t n = g_rx_len < cap ? g_rx_len : cap;
    (void)ctx;
    if (n) memcpy(buf, g_rx_data, n);
    g_rx_data += n; g_rx_len -= n;
    return n;
}
static const usb_stream_hal_t g_hal = { mock_write, mock_read, NULL };

static void reset_mock(void)
{
    g_written_len = 0; g_write_calls = 0; g_short_by = 0; g_rx_data = NULL; g_rx_len = 0;
}

static void args(void)
{
    usb_stream_t us;
    uint8_t frame[64], rx[8];
    usb_stream_hal_t nowrite = { NULL, mock_read, NULL }, noread = { mock_write, NULL, NULL };
    sample_buffer_t buf;
    protocol_response_t resp = {0, 0, 0};
    size_t n = 5, sent = 5;

    reset_mock();
    CHECK(sample_buffer_init(&buf) == SAMPLE_BUFFER_OK);
    CHECK(usb_stream_init(NULL, &g_hal, frame, sizeof frame) == USB_STREAM_ERR_ARG);
    CHECK(usb_stream_init(&us, NULL, frame, sizeof frame) == USB_STREAM_ERR_ARG);
    CHECK(usb_stream_init(&us, &nowrite, frame, sizeof frame) == USB_STREAM_ERR_ARG);
    CHECK(usb_stream_init(&us, &noread, frame, sizeof frame) == USB_STREAM_ERR_ARG);
    CHECK(usb_stream_init(&us, &g_hal, NULL, sizeof frame) == USB_STREAM_ERR_ARG);
    CHECK(usb_stream_init(&us, &g_hal, frame, sizeof frame) == USB_STREAM_OK);
    CHECK(usb_stream_send_batch(NULL, &buf, 1, &sent) == USB_STREAM_ERR_ARG);
    CHECK(usb_stream_send_batch(&us, NULL, 1, &sent) == USB_STREAM_ERR_ARG);
    CHECK(usb_stream_send_batch(&us, &buf, 1, NULL) == USB_STREAM_ERR_ARG);
    CHECK(usb_stream_send_response(NULL, &resp) == USB_STREAM_ERR_ARG);
    CHECK(usb_stream_send_response(&us, NULL) == USB_STREAM_ERR_ARG);
    CHECK(usb_stream_receive(NULL, rx, sizeof rx, &n) == USB_STREAM_ERR_ARG);
    CHECK(usb_stream_receive(&us, NULL, sizeof rx, &n) == USB_STREAM_ERR_ARG);
    CHECK(usb_stream_receive(&us, rx, sizeof rx, NULL) == USB_STREAM_ERR_ARG);
    CHECK(g_write_calls == 0);
}

static void receive_passthrough(void)
{
    usb_stream_t us;
    uint8_t frame[64], rx[8];
    static const uint8_t data[3] = {0x01, 0x02, 0x03};
    size_t n = 99;

    reset_mock();
    CHECK(usb_stream_init(&us, &g_hal, frame, sizeof frame) == USB_STREAM_OK);
    CHECK(usb_stream_receive(&us, rx, sizeof rx, &n) == USB_STREAM_OK);
    CHECK(n == 0);
    g_rx_data = data; g_rx_len = sizeof data;
    CHECK(usb_stream_receive(&us, rx, sizeof rx, &n) == USB_STREAM_OK);
    CHECK(n == sizeof data && memcmp(rx, data, sizeof data) == 0);
}

static void mechanics(void)
{
    usb_stream_t us;
    uint8_t frame[256];
    sample_buffer_t buf;
    adxl355_sample_t s = {1, -2, 3};
    protocol_response_t resp = {(uint8_t)CFG_PROTO_RESP_ACK, (uint8_t)CFG_PROTO_CMD_SET_ODR, 1u};
    protocol_frame_t f;
    size_t sent = 99, used = 0;
    uint8_t tiny[1];
    usb_stream_t us_tiny;

    /* batch: popped samples leave the buffer, one frame of the batch type reaches the hal */
    reset_mock();
    CHECK(usb_stream_init(&us, &g_hal, frame, sizeof frame) == USB_STREAM_OK);
    CHECK(sample_buffer_init(&buf) == SAMPLE_BUFFER_OK);
    CHECK(sample_buffer_push(&buf, &s) == SAMPLE_BUFFER_OK);
    CHECK(usb_stream_send_batch(&us, &buf, 1, &sent) == USB_STREAM_OK);
    CHECK(sent == 1);
    CHECK(sample_buffer_count(&buf) == 0);
    CHECK(g_write_calls >= 1 && g_written_len > 0);
    CHECK(protocol_decode_frame(g_written, g_written_len, &f, &used) == PROTOCOL_OK);
    CHECK(f.type == (uint8_t)CFG_PROTO_TYPE_SAMPLE_BATCH && used == g_written_len);

    /* response frame */
    reset_mock();
    CHECK(usb_stream_send_response(&us, &resp) == USB_STREAM_OK);
    CHECK(protocol_decode_frame(g_written, g_written_len, &f, &used) == PROTOCOL_OK);
    CHECK(f.type == (uint8_t)CFG_PROTO_TYPE_RESPONSE && used == g_written_len);

    /* short write from the hal */
    reset_mock();
    g_short_by = 1;
    CHECK(usb_stream_send_response(&us, &resp) == USB_STREAM_ERR_WRITE);

    /* scratch too small for the encoder */
    reset_mock();
    CHECK(usb_stream_init(&us_tiny, &g_hal, tiny, sizeof tiny) == USB_STREAM_OK);
    CHECK(usb_stream_send_response(&us_tiny, &resp) == USB_STREAM_ERR_ENCODE);
    CHECK(g_write_calls == 0);
}

/* a full buffer of CFG_SAMPLE_BUFFER_DEPTH samples leaves as ONE frame, at the golden bytes for 2 samples */
static void depth_batch_and_golden(void)
{
    usb_stream_t us;
    uint8_t frame[CFG_USB_FRAME_BUF_BYTES];
    sample_buffer_t buf;
    adxl355_sample_t a = {1, -2, 3}, b = {-524288, 524287, 0};
    protocol_response_t ack = {(uint8_t)CFG_PROTO_RESP_ACK, (uint8_t)CFG_PROTO_CMD_SET_ODR, 0u};
    protocol_frame_t f;
    size_t sent = 0, used = 0, i;
    static const uint8_t g_ack[] = {0xa5,0x03,0x06,0x00,0x01,0x01,0x00,0x00,0x00,0x00,0x0e,0xbc};

    reset_mock();
    CHECK(usb_stream_init(&us, &g_hal, frame, sizeof frame) == USB_STREAM_OK);
    CHECK(sample_buffer_init(&buf) == SAMPLE_BUFFER_OK);
    for (i = 0; i < CFG_SAMPLE_BUFFER_DEPTH; i++) CHECK(sample_buffer_push(&buf, (i % 2) ? &b : &a) == SAMPLE_BUFFER_OK);
    CHECK(usb_stream_send_batch(&us, &buf, CFG_SAMPLE_BUFFER_DEPTH, &sent) == USB_STREAM_OK);
    CHECK(sent == CFG_SAMPLE_BUFFER_DEPTH && g_write_calls == 1);
    CHECK(protocol_decode_frame(g_written, g_written_len, &f, &used) == PROTOCOL_OK);
    CHECK(used == g_written_len && f.payload_len == CFG_SAMPLE_BUFFER_DEPTH * 9u);

    reset_mock();
    CHECK(usb_stream_send_response(&us, &ack) == USB_STREAM_OK);
    CHECK(g_written_len == sizeof g_ack && memcmp(g_written, g_ack, sizeof g_ack) == 0);
}

int main(void)
{
    args();
    receive_passthrough();
    mechanics();
    depth_batch_and_golden();
    if (g_fail) { printf("test_usb_stream: %d check(s) failed\n", g_fail); return 1; }
    printf("test_usb_stream: ok\n");
    return 0;
}
