/*
 * test_command.c - host test for firmware/src/command.c (PICO355-R6, PICO355-R2).
 * Two builds (CMakeLists.txt): command (command set TBD) and command_inject
 * (-D test data for CFG_PROTO_*, never the real command set: mechanics only).
 * command.h: while a CFG_PROTO_CMD_* / CFG_PROTO_RESP_* value is TBD, command_parse and
 * command_apply return COMMAND_ERR_TBD, call no adxl355 or fsm function, and command_apply
 * zeroes *resp. A lower-module TBD (adxl355 / protocol) propagates as COMMAND_ERR_TBD
 * (docs/protocol.md TBD rule 3).
 */
#include <stdio.h>
#include <string.h>

#include "command.h"
#include "adxl355_regs.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

#define ANY_TBD (CFG_IS_TBD(CFG_PROTO_SYNC) || CFG_IS_TBD(CFG_PROTO_HEADER_LEN) || \
                 CFG_IS_TBD(CFG_PROTO_LENGTH_FIELD_BYTES) || CFG_IS_TBD(CFG_PROTO_CHECKSUM_KIND) || \
                 CFG_IS_TBD(CFG_PROTO_MAX_PAYLOAD) || CFG_IS_TBD(CFG_PROTO_TYPE_SAMPLE_BATCH) || \
                 CFG_IS_TBD(CFG_PROTO_TYPE_COMMAND) || CFG_IS_TBD(CFG_PROTO_TYPE_RESPONSE) || \
                 CFG_IS_TBD(CFG_PROTO_CMD_SET_ODR) || CFG_IS_TBD(CFG_PROTO_CMD_SET_RANGE) || \
                 CFG_IS_TBD(CFG_PROTO_CMD_SET_FILTER) || CFG_IS_TBD(CFG_PROTO_RESP_ACK) || \
                 CFG_IS_TBD(CFG_PROTO_RESP_ERROR) || CFG_IS_TBD(CFG_PROTO_RESP_STATUS))

#ifdef PF_REQUIRE_INJECT
#if ANY_TBD
#error "command_inject target must define every CFG_PROTO_* as test data"
#endif
#endif

static int g_calls;
static int mock_transfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    (void)ctx; (void)tx; (void)rx; (void)len;
    g_calls++;
    return 0;
}
static const adxl355_bus_t g_bus = { mock_transfer, NULL };

/* test data: a made-up table, not the design's transition table */
static const fsm_transition_t g_tab[] = {
    { FSM_STATE_IDLE, FSM_EV_CONFIG_COMMAND, FSM_STATE_CONFIGURING },
    { FSM_STATE_CONFIGURING, FSM_EV_CONFIG_DONE, FSM_STATE_IDLE }
};

typedef struct { adxl355_t dev; adxl355_config_t cfg; fsm_t fsm; command_ctx_t ctx; } rig_t;

static void rig_init(rig_t *r)
{
    memset(r, 0, sizeof *r);
    r->dev.bus = &g_bus;
    r->cfg.odr_hz = 1234u;           /* test data */
    r->cfg.range = ADXL355_RANGE_4G;
    r->cfg.filter = 3;
    CHECK(fsm_init(&r->fsm, g_tab, 2, FSM_STATE_IDLE, FSM_POLICY_IGNORE) == FSM_OK);
    r->ctx.dev = &r->dev; r->ctx.config = &r->cfg; r->ctx.fsm = &r->fsm;
    g_calls = 0;
}

static void args(void)
{
    rig_t r;
    protocol_command_t cmd = {1, 2}, out;
    protocol_response_t resp;
    uint8_t pay[4] = {0, 0, 0, 0};

    rig_init(&r);
    CHECK(command_parse(NULL, sizeof pay, &out) == COMMAND_ERR_ARG);
    CHECK(command_parse(pay, sizeof pay, NULL) == COMMAND_ERR_ARG);
    CHECK(command_apply(NULL, &cmd, &resp) == COMMAND_ERR_ARG);
    CHECK(command_apply(&r.ctx, NULL, &resp) == COMMAND_ERR_ARG);
    CHECK(command_apply(&r.ctx, &cmd, NULL) == COMMAND_ERR_ARG);
    CHECK(g_calls == 0);
}

#if ANY_TBD
static void tbd_state(void)
{
    rig_t r;
    protocol_command_t cmd = {1, 2}, out = {9, 9};
    protocol_response_t resp;
    uint8_t pay[4] = {1, 2, 3, 4};

    rig_init(&r);
    CHECK(command_parse(pay, sizeof pay, &out) == COMMAND_ERR_TBD);
    CHECK(out.code == 9 && out.value == 9);

    memset(&resp, 0xAA, sizeof resp);
    CHECK(command_apply(&r.ctx, &cmd, &resp) == COMMAND_ERR_TBD);
    CHECK(resp.code == 0 && resp.cmd_code == 0 && resp.value == 0);
    CHECK(g_calls == 0);                                   /* no adxl355 call */
    CHECK(fsm_state(&r.fsm) == FSM_STATE_IDLE);            /* no fsm call */
    CHECK(r.cfg.odr_hz == 1234u && r.cfg.range == ADXL355_RANGE_4G && r.cfg.filter == 3);
}
#else
static void mechanics(void)
{
    rig_t r;
    uint8_t buf[256];
    size_t len = 0, used = 0;
    protocol_command_t cmd = {(uint8_t)CFG_PROTO_CMD_SET_ODR, 500u}, back = {0, 0};
    protocol_command_t unknown = {0, 0};
    protocol_frame_t f;
    protocol_response_t resp;
    uint8_t known[] = { (uint8_t)CFG_PROTO_CMD_SET_ODR, (uint8_t)CFG_PROTO_CMD_SET_RANGE, (uint8_t)CFG_PROTO_CMD_SET_FILTER };
    size_t i, k;

    rig_init(&r);
    /* parse: payload of an encoded command parses back to the same command */
    CHECK(protocol_encode_command(&cmd, buf, sizeof buf, &len) == PROTOCOL_OK);
    CHECK(protocol_decode_frame(buf, len, &f, &used) == PROTOCOL_OK);
    CHECK(command_parse(f.payload, f.payload_len, &back) == COMMAND_OK);
    CHECK(back.code == cmd.code && back.value == cmd.value);
    CHECK(command_parse(buf, 0, &back) != COMMAND_OK);

    /* an unused code is COMMAND_ERR_UNKNOWN */
    unknown.value = 0;
    for (k = 0; k < 256; k++) {
        int hit = 0;
        for (i = 0; i < sizeof known; i++) if (known[i] == k) hit = 1;
        if (!hit) { unknown.code = (uint8_t)k; break; }
    }
    CHECK(command_apply(&r.ctx, &unknown, &resp) == COMMAND_ERR_UNKNOWN);

#if ADXL355_REGS_ARE_TBD
    /* driver registers still TBD: the sensor's TBD propagates, config unchanged, no bus traffic */
    CHECK(command_apply(&r.ctx, &cmd, &resp) == COMMAND_ERR_TBD);
    CHECK(r.cfg.odr_hz == 1234u && r.cfg.range == ADXL355_RANGE_4G && r.cfg.filter == 3);
    CHECK(g_calls == 0);
#endif
}
#endif

int main(void)
{
    args();
#if ANY_TBD
    tbd_state();
#else
    mechanics();
#endif
    if (g_fail) { printf("test_command: %d check(s) failed\n", g_fail); return 1; }
    printf("test_command: ok\n");
    return 0;
}
