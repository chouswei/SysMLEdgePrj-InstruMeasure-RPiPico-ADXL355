/*
 * command.h - command handler (model part commandHandler : CommandHandler).
 *
 * Requirements: PICO355-R6 (accept PC commands that set output data rate,
 * measurement range and filters; answer each one), PICO355-R2 (runtime config).
 * Command set: TODO(TBD) (CFG_PROTO_CMD_* / CFG_PROTO_RESP_* in config.h).
 * Header deps: stdint, stdbool, stddef, protocol.h, adxl355.h, fsm.h.
 */
#ifndef PICO355_COMMAND_H
#define PICO355_COMMAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "adxl355.h"
#include "fsm.h"
#include "protocol.h"

/** Return codes of the command module. */
typedef enum {
    COMMAND_OK = 0,
    COMMAND_ERR_ARG,
    COMMAND_ERR_PARSE,       /**< payload is not a valid command */
    COMMAND_ERR_UNKNOWN,     /**< code is not a CFG_PROTO_CMD_* */
    COMMAND_ERR_APPLY,       /**< sensor or fsm refused it */
    COMMAND_ERR_NOT_IMPLEMENTED
} command_rc_t;

/** Everything a command may touch. */
typedef struct {
    adxl355_t *dev;
    adxl355_config_t *config;  /**< current config, updated on success */
    fsm_t *fsm;
} command_ctx_t;

/** Parse a command frame payload into *cmd (uses protocol_decode_command). */
command_rc_t command_parse(const uint8_t *payload, size_t len, protocol_command_t *cmd);

/** Apply *cmd (model action handleCommand) and fill *resp with the answer (always filled when ctx/resp valid). */
command_rc_t command_apply(command_ctx_t *ctx, const protocol_command_t *cmd, protocol_response_t *resp);

#endif /* PICO355_COMMAND_H */
