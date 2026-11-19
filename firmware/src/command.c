/* command.c - command handler (PICO355-R6, PICO355-R2). Command set TODO(TBD) (config.h CFG_PROTO_CMD_* / CFG_PROTO_RESP_*).
 * Order in each function (command.h, docs/protocol.md TBD rule 2): arguments, then the TBD gate,
 * then the work. While a needed CFG_PROTO_* value is TBD, no adxl355 or fsm function is called.
 * Lower-module TBD codes propagate as COMMAND_ERR_TBD (TBD rule 3).
 */
#include "command.h"

/* Command codes this module dispatches on (TODO(TBD) command set; macros only, never literals). */
static bool cmd_codes_tbd(void)
{
    return CFG_IS_TBD(CFG_PROTO_CMD_SET_ODR) || CFG_IS_TBD(CFG_PROTO_CMD_SET_RANGE) ||
           CFG_IS_TBD(CFG_PROTO_CMD_SET_FILTER);
}

static bool cmd_code_known(uint8_t code)
{
    return code == (uint8_t)CFG_PROTO_CMD_SET_ODR ||
           code == (uint8_t)CFG_PROTO_CMD_SET_RANGE ||
           code == (uint8_t)CFG_PROTO_CMD_SET_FILTER;
}

static void resp_set(protocol_response_t *resp, uint8_t code, uint8_t cmd_code, uint32_t value)
{
    resp->code = code;
    resp->cmd_code = cmd_code;
    resp->value = value;
}

/*
 * Map a command onto a copy of the sensor config. TODO(TBD) value encoding: the protocol
 * does not yet fix how protocol_command_t.value is laid out for each command. The mapping
 * below (ODR in Hz, range as the adxl355_range_t member, filter as the int16 code) is the
 * single place to change once the command set is decided.
 */
static command_rc_t cmd_to_config(const protocol_command_t *cmd, adxl355_config_t *next)
{
    if (cmd->code == (uint8_t)CFG_PROTO_CMD_SET_ODR) {
        if (cmd->value == 0u) return COMMAND_ERR_ARG;   /* 0 Hz is not an output rate */
        next->odr_hz = cmd->value;
        return COMMAND_OK;
    }
    if (cmd->code == (uint8_t)CFG_PROTO_CMD_SET_RANGE) {
        if (cmd->value != (uint32_t)ADXL355_RANGE_2G && cmd->value != (uint32_t)ADXL355_RANGE_4G &&
            cmd->value != (uint32_t)ADXL355_RANGE_8G) {
            return COMMAND_ERR_ARG;
        }
        next->range = (adxl355_range_t)cmd->value;
        return COMMAND_OK;
    }
    if (cmd->code == (uint8_t)CFG_PROTO_CMD_SET_FILTER) {
        if (cmd->value > (uint32_t)INT16_MAX) return COMMAND_ERR_ARG;  /* TODO(TBD) filter code range */
        next->filter = (int16_t)cmd->value;
        return COMMAND_OK;
    }
    return COMMAND_ERR_UNKNOWN;
}

command_rc_t command_parse(const uint8_t *payload, size_t len, protocol_command_t *cmd)
{
    protocol_command_t tmp;
    protocol_rc_t rc;

    if (payload == NULL || cmd == NULL) return COMMAND_ERR_ARG;
    if (cmd_codes_tbd()) return COMMAND_ERR_TBD;  /* *cmd untouched */

    rc = protocol_decode_command(payload, len, &tmp);
    if (rc == PROTOCOL_ERR_TBD) return COMMAND_ERR_TBD;
    if (rc != PROTOCOL_OK) return COMMAND_ERR_PARSE;
    if (!cmd_code_known(tmp.code)) return COMMAND_ERR_UNKNOWN;

    *cmd = tmp;
    return COMMAND_OK;
}

command_rc_t command_apply(command_ctx_t *ctx, const protocol_command_t *cmd, protocol_response_t *resp)
{
    adxl355_config_t next;
    fsm_state_t st;
    fsm_rc_t frc;
    adxl355_rc_t arc;
    command_rc_t crc;

    if (ctx == NULL || cmd == NULL || resp == NULL) return COMMAND_ERR_ARG;
    if (ctx->dev == NULL || ctx->config == NULL || ctx->fsm == NULL) return COMMAND_ERR_ARG;

    /* *resp is always defined on return; all zero while the command set is TBD. */
    resp_set(resp, 0, 0, 0);
    if (cmd_codes_tbd() || CFG_IS_TBD(CFG_PROTO_RESP_ACK) || CFG_IS_TBD(CFG_PROTO_RESP_ERROR)) {
        return COMMAND_ERR_TBD;
    }

    if (!cmd_code_known(cmd->code)) {
        resp_set(resp, (uint8_t)CFG_PROTO_RESP_ERROR, cmd->code, (uint32_t)COMMAND_ERR_UNKNOWN);
        return COMMAND_ERR_UNKNOWN;
    }

    next = *ctx->config;
    crc = cmd_to_config(cmd, &next);
    if (crc != COMMAND_OK) {
        resp_set(resp, (uint8_t)CFG_PROTO_RESP_ERROR, cmd->code, (uint32_t)crc);
        return crc;
    }

    frc = fsm_dispatch(ctx->fsm, FSM_EV_CONFIG_COMMAND, &st);
    if (frc == FSM_ERR_TBD) {
        resp_set(resp, 0, 0, 0);
        return COMMAND_ERR_TBD;
    }
    if (frc != FSM_OK) {
        resp_set(resp, (uint8_t)CFG_PROTO_RESP_ERROR, cmd->code, (uint32_t)COMMAND_ERR_APPLY);
        return COMMAND_ERR_APPLY;
    }

    arc = adxl355_configure(ctx->dev, &next);
    if (arc == ADXL355_ERR_TBD) {
        /* leave CONFIGURING again; the sensor's TBD propagates and *ctx->config is unchanged */
        (void)fsm_dispatch(ctx->fsm, FSM_EV_CONFIG_FAILED, &st);
        resp_set(resp, 0, 0, 0);
        return COMMAND_ERR_TBD;
    }
    if (arc != ADXL355_OK) {
        (void)fsm_dispatch(ctx->fsm, FSM_EV_CONFIG_FAILED, &st);
        resp_set(resp, (uint8_t)CFG_PROTO_RESP_ERROR, cmd->code, (uint32_t)COMMAND_ERR_APPLY);
        return COMMAND_ERR_APPLY;
    }

    *ctx->config = next;
    frc = fsm_dispatch(ctx->fsm, FSM_EV_CONFIG_DONE, &st);
    if (frc != FSM_OK) {
        resp_set(resp, (uint8_t)CFG_PROTO_RESP_ERROR, cmd->code, (uint32_t)COMMAND_ERR_APPLY);
        return COMMAND_ERR_APPLY;
    }
    resp_set(resp, (uint8_t)CFG_PROTO_RESP_ACK, cmd->code, 0u);
    return COMMAND_OK;
}
