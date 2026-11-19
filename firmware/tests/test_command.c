/*
 * test_command.c - host test for firmware/src/command.c on the REAL config.h command set
 * (PICO355-R6, PICO355-R2). The mock sensor is an initialised adxl355_t over a bus that
 * accepts every transfer; the fsm table is test data, not the design's table.
 */
#include <stdio.h>
#include <string.h>

#include "command.h"
#include "adxl355_regs.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)


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
    { FSM_STATE_CONFIGURING, FSM_EV_CONFIG_DONE, FSM_STATE_IDLE },
    { FSM_STATE_CONFIGURING, FSM_EV_CONFIG_FAILED, FSM_STATE_IDLE }
};

typedef struct { adxl355_t dev; adxl355_config_t cfg; fsm_t fsm; command_ctx_t ctx; } rig_t;

static void rig_init(rig_t *r)
{
    memset(r, 0, sizeof *r);
    r->dev.bus = &g_bus;
    r->dev.initialised = true;       /* mock bus accepts everything */
    r->cfg.odr_hz = 4000u;           /* valid ODR */
    r->cfg.range = ADXL355_RANGE_4G;
    r->cfg.filter = 3;
    CHECK(fsm_init(&r->fsm, g_tab, 3, FSM_STATE_IDLE, FSM_POLICY_IGNORE) == FSM_OK);
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

}

/* C7: every command of the set is applied, answered with ACK (code + answered cmd) and stored */
static void apply_set(void)
{
    rig_t r;
    protocol_response_t resp;
    protocol_command_t odr = {(uint8_t)CFG_PROTO_CMD_SET_ODR, 2000u};
    protocol_command_t rng = {(uint8_t)CFG_PROTO_CMD_SET_RANGE, (uint32_t)ADXL355_RANGE_8G};
    protocol_command_t flt = {(uint8_t)CFG_PROTO_CMD_SET_FILTER, 2u};

    rig_init(&r);
    CHECK(command_apply(&r.ctx, &odr, &resp) == COMMAND_OK);
    CHECK(resp.code == (uint8_t)CFG_PROTO_RESP_ACK && resp.cmd_code == odr.code && r.cfg.odr_hz == 2000u);
    CHECK(command_apply(&r.ctx, &rng, &resp) == COMMAND_OK);
    CHECK(resp.code == (uint8_t)CFG_PROTO_RESP_ACK && resp.cmd_code == rng.code && r.cfg.range == ADXL355_RANGE_8G);
    CHECK(command_apply(&r.ctx, &flt, &resp) == COMMAND_OK);
    CHECK(resp.code == (uint8_t)CFG_PROTO_RESP_ACK && resp.cmd_code == flt.code && r.cfg.filter == 2);
    CHECK(fsm_state(&r.fsm) == FSM_STATE_IDLE);
}

/* a refused command is answered with RESP_ERROR and leaves the stored config alone */
static void apply_refused(void)
{
    rig_t r;
    protocol_response_t resp;
    protocol_command_t odr0 = {(uint8_t)CFG_PROTO_CMD_SET_ODR, 0u};
    protocol_command_t odr_bad = {(uint8_t)CFG_PROTO_CMD_SET_ODR, 1234u};   /* not a supported ODR */
    protocol_command_t rng_bad = {(uint8_t)CFG_PROTO_CMD_SET_RANGE, 99u};
    protocol_command_t flt_bad = {(uint8_t)CFG_PROTO_CMD_SET_FILTER, 7u};   /* above HPF code max */
    const protocol_command_t *bad[] = {&odr0, &odr_bad, &rng_bad, &flt_bad};
    size_t i;

    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        rig_init(&r);
        CHECK(command_apply(&r.ctx, bad[i], &resp) != COMMAND_OK);
        CHECK(resp.code == (uint8_t)CFG_PROTO_RESP_ERROR && resp.cmd_code == bad[i]->code);
        CHECK(r.cfg.odr_hz == 4000u && r.cfg.range == ADXL355_RANGE_4G && r.cfg.filter == 3);
    }
}

int main(void)
{
    args();
    mechanics();
    apply_set();
    apply_refused();
    if (g_fail) { printf("test_command: %d check(s) failed\n", g_fail); return 1; }
    printf("test_command: ok\n");
    return 0;
}
