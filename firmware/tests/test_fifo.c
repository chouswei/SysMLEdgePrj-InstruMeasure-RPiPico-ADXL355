/*
 * test_fifo.c - FIFO drain tests (PICO355-R4), written before the code (red first).
 * C1 marker alignment, C2 partial-set discard, C3 empty marker, C4 overflow counting and the
 * status response, C5 capacity, C6 watermark/trigger set, C8 drain length at the bus-hook level.
 * Marker rule (no-OS pin 4557b1f58f8a233bd116dbb02d8d5c447d3699b6 adxl355.c:724-725): third byte
 * bit 0 set = X entry, bit 1 set = empty. Capacity: README.rst:92 (96 locations = 32 XYZ sets).
 * Host only: a mock bus; no DMA, no hardware.
 */
#include <stdio.h>
#include <string.h>

#include "adxl355.h"
#include "adxl355_regs.h"
#include "command.h"
#include "config.h"
#include "protocol.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

#define FIFO_CMD ((uint8_t)(((ADXL355_REG_FIFO_DATA) << ADXL355_SPI_ADDR_SHIFT) | ADXL355_SPI_READ_BIT))

/* ---- mock sensor: register file, FIFO byte stream, hook-level and transfer-level counters ---- */
static uint8_t g_reg[64];
static uint8_t g_stream[400];     /* bytes the FIFO port returns */
static size_t g_stream_len;
static int g_xfer_calls;          /* transfer() calls */
static int g_xfer_fifo_calls;     /* transfer() calls that targeted FIFO_DATA */
static size_t g_xfer_fifo_len;
static int g_xfer_fifo_tx_all_cmd;/* every tx byte of that transfer equals the FIFO_DATA read byte */
static int g_hook_calls;          /* fifo_read() calls */
static uint8_t g_hook_tx;
static size_t g_hook_len;
static int g_wr;                  /* write transfers */
static uint8_t g_wr_reg, g_wr_val;

static void mock_reset(void)
{
    memset(g_reg, 0, sizeof g_reg);
    memset(g_stream, 0, sizeof g_stream);
    g_stream_len = 0; g_xfer_calls = 0; g_xfer_fifo_calls = 0; g_xfer_fifo_len = 0;
    g_xfer_fifo_tx_all_cmd = 0; g_hook_calls = 0; g_hook_tx = 0; g_hook_len = 0; g_wr = 0;
    g_reg[ADXL355_REG_DEVID_AD] = ADXL355_DEVID_AD_EXPECTED;
    g_reg[ADXL355_REG_DEVID_MST] = ADXL355_DEVID_MST_EXPECTED;
    g_reg[ADXL355_REG_PARTID] = ADXL355_PARTID_EXPECTED;
}

static int mock_transfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    uint8_t reg = (uint8_t)(tx[0] >> ADXL355_SPI_ADDR_SHIFT);
    size_t i;
    (void)ctx;
    g_xfer_calls++;
    if ((tx[0] & ADXL355_SPI_READ_BIT) != 0) {
        if (reg == ADXL355_REG_FIFO_DATA) {
            g_xfer_fifo_calls++;
            g_xfer_fifo_len = len;
            g_xfer_fifo_tx_all_cmd = 1;
            for (i = 0; i < len; i++) if (tx[i] != FIFO_CMD) g_xfer_fifo_tx_all_cmd = 0;
        }
        for (i = 1; i < len; i++) {
            if (rx == NULL) break;
            if (reg == ADXL355_REG_FIFO_DATA) rx[i] = (i - 1 < g_stream_len) ? g_stream[i - 1] : 0;
            else rx[i] = g_reg[reg + i - 1];
        }
    } else {
        g_wr++; g_wr_reg = reg; g_wr_val = tx[1];
        g_reg[reg] = tx[1];
    }
    return 0;
}

/* hook-level mock of the one-CS-low-window FIFO read: rx[0] is the dummy byte, rx[1..] the stream */
static int mock_fifo_read(void *ctx, uint8_t tx_byte, uint8_t *rx, size_t len)
{
    size_t i;
    (void)ctx;
    g_hook_calls++; g_hook_tx = tx_byte; g_hook_len = len;
    rx[0] = 0xEE;                                   /* dummy, must never reach a sample */
    for (i = 1; i < len; i++) rx[i] = (i - 1 < g_stream_len) ? g_stream[i - 1] : 0;
    return 0;
}

static void mock_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

static const adxl355_bus_t g_bus = { mock_transfer, NULL, mock_delay };
static const adxl355_fifo_hal_t g_hal = { mock_fifo_read, NULL };

static void put_entry(size_t k, int32_t v, bool x_marker, bool empty)
{
    uint32_t raw = ((uint32_t)v & 0xFFFFFu) << ADXL355_SAMPLE_SHIFT;
    g_stream[3 * k + 0] = (uint8_t)(raw >> 16);
    g_stream[3 * k + 1] = (uint8_t)(raw >> 8);
    g_stream[3 * k + 2] = (uint8_t)((raw & 0xF0u) | (x_marker ? ADXL355_FIFO_X_MARKER : 0) | (empty ? ADXL355_FIFO_EMPTY : 0));
}

/* put a full XYZ set (values 3s, 3s+1, 3s+2 offset by base) at entries k..k+2 */
static void put_set(size_t k, int32_t base)
{
    put_entry(k, base, true, false);
    put_entry(k + 1, base + 1, false, false);
    put_entry(k + 2, base + 2, false, false);
}

/* hook true: the DMA hook is bound after init (init clears it); false: no hook, transfer() fallback */
static void init_dev(adxl355_t *dev, bool hook)
{
    mock_reset();
    CHECK(adxl355_init(dev, &g_bus) == ADXL355_OK);
    CHECK(dev->fifo_hal == NULL);
    if (hook) CHECK(adxl355_set_fifo_hal(dev, &g_hal) == ADXL355_OK);
}

/* ---- C1 marker alignment ---- */
static void c1_alignment(void)
{
    adxl355_sample_t out[8];
    size_t n = 99;
    /* stream starts mid-set: Y, Z of an older set, then two whole sets */
    put_entry(0, 11, false, false);
    put_entry(1, 12, false, false);
    put_set(2, 100);
    put_set(5, 200);
    CHECK(adxl355_fifo_parse(g_stream, 8, out, 8, &n) == ADXL355_OK);
    CHECK(n == 2);
    CHECK(out[0].x == 100 && out[0].y == 101 && out[0].z == 102);
    CHECK(out[1].x == 200 && out[1].y == 201 && out[1].z == 202);
    /* starts exactly on an X: nothing skipped */
    mock_reset();
    put_set(0, -5); put_set(3, 7);
    CHECK(adxl355_fifo_parse(g_stream, 6, out, 8, &n) == ADXL355_OK && n == 2 && out[0].x == -5 && out[1].z == 9);
    /* max limits the output */
    CHECK(adxl355_fifo_parse(g_stream, 6, out, 1, &n) == ADXL355_OK && n == 1 && out[0].x == -5);
}

/* ---- C2 partial set discard ---- */
static void c2_partial(void)
{
    adxl355_sample_t out[8];
    size_t n = 99, total;
    mock_reset();
    /* ends after X, Y */
    put_set(0, 10); put_entry(3, 20, true, false); put_entry(4, 21, false, false);
    CHECK(adxl355_fifo_parse(g_stream, 5, out, 8, &n) == ADXL355_OK && n == 1 && out[0].x == 10);
    /* ends after X only */
    CHECK(adxl355_fifo_parse(g_stream, 4, out, 8, &n) == ADXL355_OK && n == 1);
    /* lone leading Y and Z before the first X, then one set and a trailing X,Y */
    mock_reset();
    put_entry(0, 1, false, false); put_entry(1, 2, false, false);
    put_set(2, 30); put_entry(5, 40, true, false); put_entry(6, 41, false, false);
    CHECK(adxl355_fifo_parse(g_stream, 7, out, 8, &n) == ADXL355_OK && n == 1 && out[0].x == 30 && out[0].z == 32);
    /* an X that follows a truncated set restarts the set (the truncated X is dropped, not merged) */
    mock_reset();
    put_entry(0, 50, true, false); put_entry(1, 51, false, false);   /* X, Y then a new X: Z lost (overflow) */
    put_set(2, 60);
    CHECK(adxl355_fifo_parse(g_stream, 5, out, 8, &n) == ADXL355_OK && n == 1 && out[0].x == 60 && out[0].y == 61);
    /* count of whole sets = floor(aligned entries / 3) for every length 0..32 */
    mock_reset();
    { size_t k; for (k = 0; k < 32; k++) put_set(3 * k, (int32_t)(3 * k)); }
    for (total = 0; total <= 96; total++) {
        adxl355_sample_t big[40];
        CHECK(adxl355_fifo_parse(g_stream, total, big, 40, &n) == ADXL355_OK && n == total / 3);
    }
}

/* ---- C3 empty marker ---- */
static void c3_empty(void)
{
    adxl355_sample_t out[4];
    adxl355_t dev;
    size_t n = 99, k;
    out[0].x = 777; out[0].y = 777; out[0].z = 777;
    mock_reset();
    for (k = 0; k < 6; k++) put_entry(k, 5, (k % 3) == 0, true);   /* every entry empty */
    CHECK(adxl355_fifo_parse(g_stream, 6, out, 4, &n) == ADXL355_OK && n == 0);
    CHECK(out[0].x == 777 && out[0].y == 777 && out[0].z == 777);  /* nothing stale reused or written */
    /* an empty X is not a set start; the next real set is emitted */
    mock_reset();
    put_entry(0, 5, true, true); put_entry(1, 6, false, false); put_entry(2, 7, false, false);
    put_set(3, 80);
    CHECK(adxl355_fifo_parse(g_stream, 6, out, 4, &n) == ADXL355_OK && n == 1 && out[0].x == 80);
    /* FIFO_ENTRIES = 0: no DMA/FIFO transfer, no sample, no error */
    init_dev(&dev, true);
    g_reg[ADXL355_REG_FIFO_ENTRIES] = 0;
    n = 99;
    CHECK(adxl355_read_fifo(&dev, out, 4, &n) == ADXL355_OK && n == 0);
    CHECK(g_hook_calls == 0 && g_xfer_fifo_calls == 0);
}

/* ---- C4 overflow counting ---- */
static void c4_overflow(void)
{
    adxl355_t dev;
    uint8_t st = 0;
    uint32_t i;
    init_dev(&dev, false);
    CHECK(dev.fifo_overflows == 0u);
    g_reg[ADXL355_REG_STATUS] = ADXL355_STATUS_DATA_RDY;
    CHECK(adxl355_status(&dev, &st) == ADXL355_OK && dev.fifo_overflows == 0u);   /* clear: unchanged */
    g_reg[ADXL355_REG_STATUS] = ADXL355_STATUS_FIFO_FULL;
    CHECK(adxl355_status(&dev, &st) == ADXL355_OK && dev.fifo_overflows == 0u);   /* FIFO_FULL is not OVR */
    g_reg[ADXL355_REG_STATUS] = ADXL355_STATUS_FIFO_OVR;
    for (i = 1; i <= 5; i++) {
        CHECK(adxl355_status(&dev, &st) == ADXL355_OK);
        CHECK(dev.fifo_overflows == i);                                          /* N observations = N */
    }
    g_reg[ADXL355_REG_STATUS] = 0;
    CHECK(adxl355_status(&dev, &st) == ADXL355_OK && dev.fifo_overflows == 5u);
    /* counter is 32 bit and saturates, it does not wrap */
    CHECK(sizeof dev.fifo_overflows == 4u);
    dev.fifo_overflows = 0xFFFFFFFFu;
    g_reg[ADXL355_REG_STATUS] = ADXL355_STATUS_FIFO_OVR;
    CHECK(adxl355_status(&dev, &st) == ADXL355_OK && dev.fifo_overflows == 0xFFFFFFFFu);
    /* re-init clears the count */
    CHECK(adxl355_init(&dev, &g_bus) == ADXL355_OK && dev.fifo_overflows == 0u);
}

/* status response: a GET_STATUS command returns RESP_STATUS carrying the count, streaming is not interrupted */
static void c4_status_response(void)
{
    adxl355_t dev;
    adxl355_config_t cfg;
    fsm_t fsm;
    command_ctx_t ctx;
    protocol_command_t cmd = { (uint8_t)CFG_PROTO_CMD_GET_STATUS, 0u };
    protocol_response_t resp, back;
    protocol_frame_t f;
    uint8_t buf[64];
    size_t len = 0, used = 0;
    fsm_state_t st;

    init_dev(&dev, false);
    dev.fifo_overflows = 0x01020304u;
    cfg = dev.config;
    CHECK(fsm_init_default(&fsm) == FSM_OK);
    (void)fsm_dispatch(&fsm, FSM_EV_CONFIG_COMMAND, &st);
    (void)fsm_dispatch(&fsm, FSM_EV_CONFIG_DONE, &st);
    CHECK(fsm_state(&fsm) == FSM_STATE_STREAMING);
    ctx.dev = &dev; ctx.config = &cfg; ctx.fsm = &fsm;
    g_xfer_calls = 0;
    CHECK(command_apply(&ctx, &cmd, &resp) == COMMAND_OK);
    CHECK(resp.code == (uint8_t)CFG_PROTO_RESP_STATUS && resp.cmd_code == (uint8_t)CFG_PROTO_CMD_GET_STATUS);
    CHECK(resp.value == 0x01020304u);
    CHECK(fsm_state(&fsm) == FSM_STATE_STREAMING);   /* a status query does not leave STREAMING */
    CHECK(g_xfer_calls == 0);                         /* no sensor traffic */
    /* the response survives the wire format: value is the little-endian u32 at the response value field */
    CHECK(protocol_encode_response(&resp, buf, sizeof buf, &len) == PROTOCOL_OK);
    CHECK(protocol_decode_frame(buf, len, &f, &used) == PROTOCOL_OK && f.type == (uint8_t)CFG_PROTO_TYPE_RESPONSE);
    CHECK(protocol_decode_response(f.payload, f.payload_len, &back) == PROTOCOL_OK);
    CHECK(back.code == (uint8_t)CFG_PROTO_RESP_STATUS && back.value == 0x01020304u);
    CHECK(f.payload[CFG_PROTO_RESP_VALUE_OFFSET] == 0x04 && f.payload[CFG_PROTO_RESP_VALUE_OFFSET + 3] == 0x01);
}

/* ---- C5 capacity ---- */
static void c5_capacity(void)
{
    adxl355_t dev;
    adxl355_sample_t out[40];
    size_t n = 0, k;
    CHECK(!ADXL355_IS_TBD(ADXL355_FIFO_CAPACITY));
    CHECK(ADXL355_FIFO_CAPACITY == 96);
    CHECK(ADXL355_FIFO_CAPACITY / 3 == 32);
    /* a FIFO_ENTRIES reading above capacity never makes the drain longer than 96 entries */
    init_dev(&dev, true);
    g_reg[ADXL355_REG_FIFO_ENTRIES] = 0xFF;
    for (k = 0; k < 32; k++) put_set(3 * k, (int32_t)(3 * k));
    g_stream_len = 3 * 96;
    CHECK(adxl355_read_fifo(&dev, out, 40, &n) == ADXL355_OK);
    CHECK(g_hook_calls == 1 && g_hook_len <= 1u + 3u * ADXL355_FIFO_CAPACITY);
    CHECK(n <= ADXL355_FIFO_CAPACITY / 3);
}

/* ---- C6 design decisions are set (value not named here) ---- */
static void c6_decisions(void)
{
    CHECK(!CFG_IS_TBD(CFG_FIFO_WATERMARK));
    CHECK(CFG_FIFO_WATERMARK >= 1u && CFG_FIFO_WATERMARK <= ADXL355_FIFO_SAMPLES_MAX);
    CHECK(!CFG_IS_TBD(CFG_FIFO_TRIGGER));
    CHECK(CFG_FIFO_TRIGGER == CFG_FIFO_TRIGGER_POLL_ENTRIES || CFG_FIFO_TRIGGER == CFG_FIFO_TRIGGER_DRDY ||
          CFG_FIFO_TRIGGER == CFG_FIFO_TRIGGER_FIFO_FULL);
    CHECK(CFG_SPI_CLOCK_HZ == 1000000u);   /* C7: the clock is not raised */
}

/* watermark write and gate */
static void c6_watermark(void)
{
    adxl355_t dev;
    adxl355_sample_t out[40];
    size_t n = 99, k;
    init_dev(&dev, true);
    g_wr = 0;   /* init wrote RESET */
    CHECK(adxl355_set_fifo_watermark(&dev, (uint8_t)(ADXL355_FIFO_SAMPLES_MAX + 1u)) == ADXL355_ERR_ARG);
    CHECK(g_wr == 0);
    CHECK(adxl355_set_fifo_watermark(&dev, 48) == ADXL355_OK);
    CHECK(g_wr == 1 && g_wr_reg == ADXL355_REG_FIFO_SAMPLES && g_wr_val == 48);
    for (k = 0; k < 32; k++) put_set(3 * k, (int32_t)(3 * k));
    g_stream_len = 3 * 96;
    /* below the watermark: only the entry count is read, nothing drained */
    g_reg[ADXL355_REG_FIFO_ENTRIES] = 45; g_hook_calls = 0; n = 99;
    CHECK(adxl355_read_fifo(&dev, out, 40, &n) == ADXL355_OK && n == 0 && g_hook_calls == 0);
    /* at the watermark: drained */
    g_reg[ADXL355_REG_FIFO_ENTRIES] = 48;
    CHECK(adxl355_read_fifo(&dev, out, 40, &n) == ADXL355_OK && n == 16 && g_hook_calls == 1);
}

/* ---- C8 drain length and TX byte at the bus-hook level ---- */
static void c8_length(void)
{
    adxl355_t dev;
    adxl355_sample_t out[40];
    size_t n = 0, k;
    uint8_t entries;
    static const uint8_t cases[] = { 3, 9, 48, 96 };
    size_t ci;
    for (ci = 0; ci < sizeof cases; ci++) {
        entries = cases[ci];
        init_dev(&dev, true);
        for (k = 0; k < 32; k++) put_set(3 * k, (int32_t)(3 * k));
        g_stream_len = 3u * 96u;
        g_reg[ADXL355_REG_FIFO_ENTRIES] = entries;
        g_xfer_calls = 0;
        CHECK(adxl355_read_fifo(&dev, out, 40, &n) == ADXL355_OK);
        CHECK(g_xfer_calls == 1);                    /* FIFO_ENTRIES read first; the drain is NOT a transfer() */
        CHECK(g_hook_calls == 1);                    /* one CS-low window = one hook call */
        CHECK(g_hook_len == 1u + 3u * (size_t)entries);   /* one leading dummy + FIFO_ENTRIES x 3 */
        CHECK(g_hook_tx == FIFO_CMD && FIFO_CMD == 0x23); /* (0x11 << 1) | READ_BIT */
        CHECK(n == (size_t)entries / 3u);
        CHECK(out[0].x == 0);                        /* the dummy byte (0xEE) never leaks into a sample */
    }
    /* FIFO_ENTRIES not a multiple of 3: whole sets only, the tail stays in the FIFO (no-OS C:714) */
    init_dev(&dev, true);
    for (k = 0; k < 32; k++) put_set(3 * k, (int32_t)(3 * k));
    g_stream_len = 3u * 96u;
    g_reg[ADXL355_REG_FIFO_ENTRIES] = 7;
    CHECK(adxl355_read_fifo(&dev, out, 40, &n) == ADXL355_OK && n == 2 && g_hook_len == 1u + 18u);
    /* output space smaller than the FIFO: drain only what fits, no loss */
    g_reg[ADXL355_REG_FIFO_ENTRIES] = 48;
    CHECK(adxl355_read_fifo(&dev, out, 4, &n) == ADXL355_OK && n == 4 && g_hook_len == 1u + 36u);
    /* non-DMA fallback: no hook, one transfer(), tx is the FIFO_DATA read byte repeated, same length */
    init_dev(&dev, false);
    for (k = 0; k < 32; k++) put_set(3 * k, (int32_t)(3 * k));
    g_stream_len = 3u * 96u;
    g_reg[ADXL355_REG_FIFO_ENTRIES] = 48;
    CHECK(adxl355_read_fifo(&dev, out, 40, &n) == ADXL355_OK && n == 16);
    CHECK(g_xfer_fifo_calls == 1 && g_xfer_fifo_len == 1u + 144u && g_xfer_fifo_tx_all_cmd == 1);
    CHECK(out[15].x == 45 && out[15].z == 47);
}

int main(void)
{
    c1_alignment();
    c2_partial();
    c3_empty();
    c4_overflow();
    c4_status_response();
    c5_capacity();
    c6_decisions();
    c6_watermark();
    c8_length();
    if (g_fail) { printf("test_fifo: %d check(s) failed\n", g_fail); return 1; }
    printf("test_fifo: ok\n");
    return 0;
}
