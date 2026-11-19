/*
 * main.c - Pico355 firmware entry (PicoFirmware). The ONLY file that includes
 * Pico SDK headers: it binds SDK SPI0, GPIO and USB CDC into the hal structs
 * (adxl355_bus_t, acq_hal_t, usb_stream_hal_t) and runs the main loop.
 * Requirements: PICO355-R4, PICO355-R5, PICO355-R6, PICO355-R7.
 * While CFG_SPI_CLOCK_HZ / CFG_SPI_MODE are TODO(TBD) sentinels (CFG_IS_TBD),
 * SPI0 is not initialised and the sensor is not touched; TBD-dependent code is
 * guarded with #if so no sentinel reaches an SDK call.
 */
#include <stdio.h>

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

static size_t cdc_write(void *ctx, const uint8_t *buf, size_t len)
{
    (void)ctx;
    for (size_t i = 0; i < len; i++) {
        putchar_raw(buf[i]);
    }
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

#define PICO355_SPI_DECIDED (!CFG_IS_TBD(CFG_SPI_CLOCK_HZ) && !CFG_IS_TBD(CFG_SPI_MODE))

static bool spi_constants_decided(void)
{
    return PICO355_SPI_DECIDED;
}

static void pins_init(void)
{
    /* DRDY: input, no drive (model: GP22 is read, never driven). */
    gpio_init(CFG_PIN_DRDY_IN);
    gpio_set_dir(CFG_PIN_DRDY_IN, GPIO_IN);

#if PICO355_SPI_DECIDED
    spi_init(PICO355_SPI, (uint)CFG_SPI_CLOCK_HZ);
    spi_set_format(PICO355_SPI, 8, ((uint32_t)CFG_SPI_MODE & 2u) ? SPI_CPOL_1 : SPI_CPOL_0,
                   ((uint32_t)CFG_SPI_MODE & 1u) ? SPI_CPHA_1 : SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(CFG_PIN_SPI0_RX_MISO, GPIO_FUNC_SPI);
    gpio_set_function(CFG_PIN_SPI0_SCK, GPIO_FUNC_SPI);
    gpio_set_function(CFG_PIN_SPI0_TX_MOSI, GPIO_FUNC_SPI);
    gpio_init(CFG_PIN_SPI0_CSN);
    gpio_set_dir(CFG_PIN_SPI0_CSN, GPIO_OUT);
    gpio_put(CFG_PIN_SPI0_CSN, 1);
#endif
}

/* ---- state (file-local, not in headers) ---- */
static adxl355_t g_dev;
static sample_buffer_t g_buf;
static acq_t g_acq;
static usb_stream_t g_usb;
static fsm_t g_fsm;
static adxl355_config_t g_cfg;
static uint8_t g_frame_buf[CFG_USB_FRAME_BUF_BYTES]; /* scratch size TODO(TBD) frame format (config.h) */
static uint8_t g_rx_buf[CFG_USB_RX_BUF_BYTES];       /* scratch size TODO(TBD) frame format (config.h) */

int main(void)
{
    static const adxl355_bus_t bus = { bus_transfer, NULL };
    static const acq_hal_t acq_hal = { drdy_read, NULL };
    static const usb_stream_hal_t usb_hal = { cdc_write, cdc_read, NULL };

    stdio_init_all();
    pins_init();

    adxl355_bus_t bus_bound = bus;
    bus_bound.ctx = PICO355_SPI;

    /* TODO(TBD) state transition table: while CFG_FSM_TRANSITION_TABLE is empty
     * fsm_init_default returns FSM_ERR_TBD and the state stays FSM_STATE_UNSET.
     * The loop below then never enters streaming and makes no sensor call. */
    fsm_rc_t fsm_rc = fsm_init_default(&g_fsm);
    (void)fsm_rc;
    (void)sample_buffer_init(&g_buf);

    /* adxl355 register contract is TODO(TBD) (adxl355_regs.h). Without decided
     * SPI constants the driver is not initialised; with them, an ADXL355_ERR_TBD
     * or any other init failure leaves dev_rc non-OK, and acq_step reports it
     * (ACQ_ERR_TBD / ACQ_ERR_SENSOR). No register value is invented here. */
    adxl355_rc_t dev_rc = ADXL355_ERR_NOT_INIT;
    if (spi_constants_decided()) {
        dev_rc = adxl355_init(&g_dev, &bus_bound);
    }
    (void)dev_rc;
    (void)acq_init(&g_acq, &acq_hal, &g_dev, &g_buf);
    (void)usb_stream_init(&g_usb, &usb_hal, g_frame_buf, sizeof g_frame_buf);

    g_cfg.odr_hz = CFG_DEFAULT_ODR_HZ;
    g_cfg.range = CFG_DEFAULT_RANGE;
    g_cfg.filter = CFG_IS_TBD(CFG_DEFAULT_FILTER) ? ADXL355_FILTER_UNSET : (int16_t)CFG_DEFAULT_FILTER;
    command_ctx_t cmd_ctx = { &g_dev, &g_cfg, &g_fsm };

    for (;;) {
        size_t n = 0;
        if (usb_stream_receive(&g_usb, g_rx_buf, sizeof g_rx_buf, &n) == USB_STREAM_OK && n > 0) {
            protocol_frame_t frame;
            size_t used = 0;
            if (!CFG_IS_TBD(CFG_PROTO_TYPE_COMMAND) &&
                protocol_decode_frame(g_rx_buf, n, &frame, &used) == PROTOCOL_OK &&
                frame.type == (uint8_t)CFG_PROTO_TYPE_COMMAND) {
                protocol_command_t cmd;
                protocol_response_t resp;
                if (command_parse(frame.payload, frame.payload_len, &cmd) == COMMAND_OK) {
                    (void)command_apply(&cmd_ctx, &cmd, &resp);
                    (void)usb_stream_send_response(&g_usb, &resp);
                }
            }
        }
        if (fsm_state(&g_fsm) == FSM_STATE_STREAMING) {
            size_t pushed = 0, sent = 0;
            acq_rc_t arc = acq_step(&g_acq, &pushed);
            if (arc == ACQ_ERR_TBD || arc == ACQ_ERR_SENSOR || arc == ACQ_ERR_OVERFLOW) {
                /* stream error -> fsm (FSM_EV_STREAM_ERROR); moves to FAULT once the table is decided */
                fsm_state_t next = fsm_state(&g_fsm);
                (void)fsm_dispatch(&g_fsm, FSM_EV_STREAM_ERROR, &next);
            }
            (void)usb_stream_send_batch(&g_usb, &g_buf, CFG_SAMPLE_BUFFER_DEPTH, &sent);
        }
        tight_loop_contents();
    }
}
