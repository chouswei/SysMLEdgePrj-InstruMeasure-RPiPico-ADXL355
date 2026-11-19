/*
 * main.c - Pico355 firmware entry (PicoFirmware). The ONLY file that includes
 * Pico SDK headers: it binds SDK SPI0, GPIO and USB CDC into the hal structs
 * (adxl355_bus_t, acq_hal_t, usb_stream_hal_t) and runs the main loop.
 * Requirements: PICO355-R4, PICO355-R5, PICO355-R6, PICO355-R7.
 * Boot: SPI0 up (GP16/17/18/19, CFG_SPI_CLOCK_HZ, CFG_SPI_MODE, MSB first; cited no-OS pin
 * 4557b1f58f8a233bd116dbb02d8d5c447d3699b6 projects/eval-adxl355-pmdz), sensor reset with the
 * cited 1 ms delay, configure CFG_DEFAULT_ODR_HZ / CFG_DEFAULT_RANGE, start measuring, then
 * stream sample-batch frames over USB CDC; command frames from the PC are answered.
 * Not run on hardware by the author: see the PR (host round trip only).
 */
#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "acquisition.h"
#include "adxl355.h"
#include "command.h"
#include "config.h"
#include "fsm.h"
#include "protocol.h"
#include "sample_buffer.h"
#include "usb_stream.h"

/* ---- hal bindings ---- */

static int bus_transfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    spi_inst_t *spi = (spi_inst_t *)ctx;
    int n;
    gpio_put(CFG_PIN_SPI0_CSN, 0);
    if (rx != NULL) {
        n = spi_write_read_blocking(spi, tx, rx, len);
    } else {
        n = spi_write_blocking(spi, tx, len);
    }
    gpio_put(CFG_PIN_SPI0_CSN, 1);
    return (n == (int)len) ? 0 : -1;
}

static bool drdy_read(void *ctx)
{
    (void)ctx;
    return gpio_get(CFG_PIN_DRDY_IN); /* GP22 is an input only; never driven */
}

static void bus_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    sleep_ms(ms);
}

/* Raw bytes (no CR/LF translation) to the USB CDC stdio; the frame leaves in one flush. */
static size_t cdc_write(void *ctx, const uint8_t *buf, size_t len)
{
    (void)ctx;
    (void)stdio_put_string((const char *)buf, (int)len, false, false);
    stdio_flush();
    return len;
}

static size_t cdc_read(void *ctx, uint8_t *buf, size_t cap)
{
    (void)ctx;
    size_t n = 0;
    while (n < cap) {
        int c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) break;
        buf[n++] = (uint8_t)c;
    }
    return n;
}

/* SPI bus from the model (SPI0, PICO355-R4) via CFG_SPI_INSTANCE_INDEX. */
#define PICO355_SPI SPI_INSTANCE(CFG_SPI_INSTANCE_INDEX)

#if CFG_IS_TBD(CFG_SPI_CLOCK_HZ) || CFG_IS_TBD(CFG_SPI_MODE)
#error "SPI clock and mode must be set in config.h (cited: no-OS pin projects/eval-adxl355-pmdz)"
#endif

static void pins_init(void)
{
    /* DRDY: input, no drive (model: GP22 is read, never driven). */
    gpio_init(CFG_PIN_DRDY_IN);
    gpio_set_dir(CFG_PIN_DRDY_IN, GPIO_IN);

    spi_init(PICO355_SPI, (uint)CFG_SPI_CLOCK_HZ);
    spi_set_format(PICO355_SPI, 8, ((uint32_t)CFG_SPI_MODE & 2u) ? SPI_CPOL_1 : SPI_CPOL_0,
                   ((uint32_t)CFG_SPI_MODE & 1u) ? SPI_CPHA_1 : SPI_CPHA_0,
                   CFG_SPI_BIT_ORDER_MSB_FIRST ? SPI_MSB_FIRST : SPI_LSB_FIRST);
    gpio_set_function(CFG_PIN_SPI0_RX_MISO, GPIO_FUNC_SPI);
    gpio_set_function(CFG_PIN_SPI0_SCK, GPIO_FUNC_SPI);
    gpio_set_function(CFG_PIN_SPI0_TX_MOSI, GPIO_FUNC_SPI);
    gpio_init(CFG_PIN_SPI0_CSN);
    gpio_set_dir(CFG_PIN_SPI0_CSN, GPIO_OUT);
    gpio_put(CFG_PIN_SPI0_CSN, 1);
}

/* ---- state (file-local, not in headers) ---- */
static adxl355_t g_dev;
static sample_buffer_t g_buf;
static acq_t g_acq;
static usb_stream_t g_usb;
static fsm_t g_fsm;
static adxl355_config_t g_cfg;
static uint8_t g_frame_buf[CFG_USB_FRAME_BUF_BYTES]; /* one whole frame (config.h) */
static uint8_t g_rx_buf[CFG_USB_RX_BUF_BYTES];       /* receive accumulator (config.h) */
static size_t g_rx_len;

/* Run the received bytes through the frame decoder: resync on garbage (drop `used`), act on
 * command frames, keep a partial frame until the rest arrives. */
static void rx_poll(command_ctx_t *cmd_ctx)
{
    size_t n = 0;
    if (usb_stream_receive(&g_usb, g_rx_buf + g_rx_len, sizeof g_rx_buf - g_rx_len, &n) == USB_STREAM_OK)
        g_rx_len += n;
    while (g_rx_len > 0) {
        protocol_frame_t frame;
        size_t used = 0;
        protocol_rc_t rc = protocol_decode_frame(g_rx_buf, g_rx_len, &frame, &used);
        if (rc == PROTOCOL_ERR_SHORT) {
            if (g_rx_len < sizeof g_rx_buf) break;   /* wait for the rest of the frame */
            used = 1;                                 /* full buffer that is no frame: drop a byte */
        } else if (rc == PROTOCOL_OK && frame.type == (uint8_t)CFG_PROTO_TYPE_COMMAND) {
            protocol_command_t cmd;
            protocol_response_t resp;
            if (command_parse(frame.payload, frame.payload_len, &cmd) == COMMAND_OK) {
                (void)command_apply(cmd_ctx, &cmd, &resp);
                (void)usb_stream_send_response(&g_usb, &resp);
            }
        } else if (rc != PROTOCOL_OK && used == 0) {
            used = 1;
        }
        memmove(g_rx_buf, g_rx_buf + used, g_rx_len - used);
        g_rx_len -= used;
    }
}

int main(void)
{
    static const adxl355_bus_t bus = { bus_transfer, NULL, bus_delay_ms };
    static const acq_hal_t acq_hal = { drdy_read, NULL };
    static const usb_stream_hal_t usb_hal = { cdc_write, cdc_read, NULL };

    stdio_init_all();
    pins_init();

    adxl355_bus_t bus_bound = bus;
    bus_bound.ctx = PICO355_SPI;

    (void)fsm_init_default(&g_fsm);   /* IDLE: config.h transition table (design decision) */
    (void)sample_buffer_init(&g_buf);

    g_cfg.odr_hz = CFG_DEFAULT_ODR_HZ;
    g_cfg.range = CFG_DEFAULT_RANGE;                /* design choice, config.h */
    g_cfg.filter = CFG_IS_TBD(CFG_DEFAULT_FILTER) ? ADXL355_FILTER_UNSET : (int16_t)CFG_DEFAULT_FILTER;

    /* Sensor bring-up: soft reset + 1 ms delay + ID check (adxl355_init), configure, start measuring.
     * A failure leaves the fsm in IDLE: nothing streams, commands are answered with an error. */
    adxl355_rc_t dev_rc = adxl355_init(&g_dev, &bus_bound);
    (void)acq_init(&g_acq, &acq_hal, &g_dev, &g_buf);
    (void)usb_stream_init(&g_usb, &usb_hal, g_frame_buf, sizeof g_frame_buf);
    if (dev_rc == ADXL355_OK) {
        fsm_state_t st;
        (void)fsm_dispatch(&g_fsm, FSM_EV_CONFIG_COMMAND, &st);
        if (adxl355_configure(&g_dev, &g_cfg) == ADXL355_OK && adxl355_set_measuring(&g_dev, true) == ADXL355_OK)
            (void)fsm_dispatch(&g_fsm, FSM_EV_CONFIG_DONE, &st);
        else
            (void)fsm_dispatch(&g_fsm, FSM_EV_CONFIG_FAILED, &st);
    }

    command_ctx_t cmd_ctx = { &g_dev, &g_cfg, &g_fsm };

    for (;;) {
        rx_poll(&cmd_ctx);
        if (fsm_state(&g_fsm) == FSM_STATE_STREAMING) {
            size_t pushed = 0, sent = 0;
            acq_rc_t arc = acq_step(&g_acq, &pushed);
            /* ACQ_ERR_OVERFLOW is not a fault: the buffer is full, the newest samples are dropped and counted
             * (sample_buffer_overflows; policy CFG_SAMPLE_BUFFER_OVERFLOW_POLICY stays TBD). */
            if (arc == ACQ_ERR_TBD || arc == ACQ_ERR_SENSOR) {
                fsm_state_t next = fsm_state(&g_fsm);
                (void)fsm_dispatch(&g_fsm, FSM_EV_STREAM_ERROR, &next);
            }
            (void)usb_stream_send_batch(&g_usb, &g_buf, CFG_SAMPLE_BUFFER_DEPTH, &sent);
        }
        tight_loop_contents();
    }
}
