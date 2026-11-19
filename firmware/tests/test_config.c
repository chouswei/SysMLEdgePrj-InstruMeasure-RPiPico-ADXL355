/*
 * test_config.c - settled link constants are not TBD (PICO355-R4, PICO355-R5, PICO355-R6).
 * Each settled item carries its label in config.h: "cited" (no-OS pin
 * 4557b1f58f8a233bd116dbb02d8d5c447d3699b6) or "design decision (2026-11-19, owner go t37u)" /
 * "design choice". This test only asserts the items are set and consistent; it does not
 * repeat their numbers. Items that stay TBD (hardware LDO, cable pinout, log format,
 * default filter, overflow policy) keep their TBD sentinel here.
 */
#include <stdio.h>

#include "config.h"
#include "fsm.h"
#include "adxl355.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

int main(void)
{
    /* C8: SPI settings and post-reset delay (cited to no-OS pin, projects/eval-adxl355-pmdz and adxl355.c) */
    CHECK(!CFG_IS_TBD(CFG_SPI_CLOCK_HZ));
    CHECK(!CFG_IS_TBD(CFG_SPI_MODE));
    CHECK(CFG_SPI_MODE <= 3u);
    CHECK(!CFG_IS_TBD(CFG_SPI_BIT_ORDER_MSB_FIRST) && CFG_SPI_BIT_ORDER_MSB_FIRST == 1);
    CHECK(!CFG_IS_TBD(CFG_ADXL355_RESET_DELAY_MS));

    /* pins stay the model's: GP16/17/18/19 and DRDY GP22 (not the no-OS example's GP20) */
    CHECK(CFG_PIN_SPI0_RX_MISO == 16 && CFG_PIN_SPI0_CSN == 17 && CFG_PIN_SPI0_SCK == 18 &&
          CFG_PIN_SPI0_TX_MOSI == 19 && CFG_PIN_DRDY_IN == 22 && CFG_SPI_INSTANCE_INDEX == 0);

    /* frame format: every name the codec reads is decided */
    CHECK(!CFG_IS_TBD(CFG_PROTO_SYNC) && !CFG_IS_TBD(CFG_PROTO_HEADER_LEN) &&
          !CFG_IS_TBD(CFG_PROTO_LENGTH_FIELD_BYTES) && !CFG_IS_TBD(CFG_PROTO_CHECKSUM_KIND) &&
          !CFG_IS_TBD(CFG_PROTO_MAX_PAYLOAD) && !CFG_IS_TBD(CFG_PROTO_TYPE_SAMPLE_BATCH) &&
          !CFG_IS_TBD(CFG_PROTO_TYPE_COMMAND) && !CFG_IS_TBD(CFG_PROTO_TYPE_RESPONSE));
    CHECK(!CFG_IS_TBD(CFG_PROTO_SYNC_OFFSET) && !CFG_IS_TBD(CFG_PROTO_TYPE_OFFSET) &&
          !CFG_IS_TBD(CFG_PROTO_LENGTH_OFFSET) && !CFG_IS_TBD(CFG_PROTO_LENGTH_MSB_FIRST) &&
          !CFG_IS_TBD(CFG_PROTO_PAD_BYTE) && !CFG_IS_TBD(CFG_PROTO_CHECKSUM_NONE));
    CHECK(!CFG_IS_TBD(CFG_PROTO_CMD_PAYLOAD_LEN) && !CFG_IS_TBD(CFG_PROTO_CMD_CODE_OFFSET) &&
          !CFG_IS_TBD(CFG_PROTO_CMD_VALUE_OFFSET) && !CFG_IS_TBD(CFG_PROTO_CMD_VALUE_BYTES) &&
          !CFG_IS_TBD(CFG_PROTO_RESP_PAYLOAD_LEN) && !CFG_IS_TBD(CFG_PROTO_RESP_CODE_OFFSET) &&
          !CFG_IS_TBD(CFG_PROTO_RESP_CMD_CODE_OFFSET) && !CFG_IS_TBD(CFG_PROTO_RESP_VALUE_OFFSET) &&
          !CFG_IS_TBD(CFG_PROTO_RESP_VALUE_BYTES) && !CFG_IS_TBD(CFG_PROTO_VALUE_MSB_FIRST) &&
          !CFG_IS_TBD(CFG_PROTO_SAMPLE_AXIS_BYTES) && !CFG_IS_TBD(CFG_PROTO_SAMPLE_MSB_FIRST));
    CHECK(!CFG_IS_TBD(CFG_PROTO_CMD_SET_ODR) && !CFG_IS_TBD(CFG_PROTO_CMD_SET_RANGE) &&
          !CFG_IS_TBD(CFG_PROTO_CMD_SET_FILTER) && !CFG_IS_TBD(CFG_PROTO_RESP_ACK) &&
          !CFG_IS_TBD(CFG_PROTO_RESP_ERROR) && !CFG_IS_TBD(CFG_PROTO_RESP_STATUS));

    /* streaming scratch and buffer: sized from the frame format, not placeholders */
    CHECK(!CFG_SAMPLE_BUFFER_DEPTH_IS_TBD && !CFG_USB_BUF_BYTES_IS_TBD);
    CHECK(CFG_USB_FRAME_BUF_BYTES >= CFG_PROTO_HEADER_LEN + CFG_PROTO_MAX_PAYLOAD + CFG_PROTO_CHECKSUM_LEN_BYTES);
    CHECK(CFG_SAMPLE_BUFFER_DEPTH * 3u * CFG_PROTO_SAMPLE_AXIS_BYTES <= CFG_PROTO_MAX_PAYLOAD);
    CHECK(CFG_USB_RX_BUF_BYTES >= CFG_PROTO_HEADER_LEN + CFG_PROTO_CMD_PAYLOAD_LEN + CFG_PROTO_CHECKSUM_LEN_BYTES);

    /* default range is needed to stream (configure() writes ODR 4000 only with a legal range): set */
    CHECK(CFG_DEFAULT_RANGE != ADXL355_RANGE_UNSET);
    CHECK(CFG_DEFAULT_ODR_HZ == 4000u);          /* PICO355-R1 */

    /* fsm boot defaults are set (design decision): the stream can start */
    CHECK(CFG_FSM_INITIAL_STATE != FSM_STATE_UNSET && CFG_FSM_UNKNOWN_EVENT_POLICY != FSM_POLICY_UNSET);

    /* still TBD on purpose */
    CHECK(!CFG_IS_TBD(CFG_FIFO_WATERMARK));   /* settled: design decision (owner t39u), range checked in test_fifo.c */
    CHECK(!CFG_IS_TBD(CFG_FIFO_TRIGGER));
    CHECK(CFG_SPI_CLOCK_HZ == 1000000u);      /* the SPI clock stays at the cited 1 MHz (no raise) */
    CHECK(CFG_IS_TBD(CFG_DEFAULT_FILTER));
    CHECK(CFG_IS_TBD(CFG_SAMPLE_BUFFER_OVERFLOW_POLICY));

    if (g_fail) { printf("test_config: %d check(s) failed\n", g_fail); return 1; }
    printf("test_config: ok\n");
    return 0;
}
