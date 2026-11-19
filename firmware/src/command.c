/* command.c - command handler stubs (PICO355-R6, PICO355-R2). Command set TODO(TBD). */
#include "command.h"

command_rc_t command_parse(const uint8_t *payload, size_t len, protocol_command_t *cmd)
{
    if (payload == NULL || cmd == NULL) return COMMAND_ERR_ARG;
    (void)len;
    return COMMAND_ERR_NOT_IMPLEMENTED;
}

command_rc_t command_apply(command_ctx_t *ctx, const protocol_command_t *cmd, protocol_response_t *resp)
{
    if (ctx == NULL || cmd == NULL || resp == NULL) return COMMAND_ERR_ARG;
    resp->code = 0;
    resp->cmd_code = 0;
    resp->value = 0;
    return COMMAND_ERR_NOT_IMPLEMENTED;
}
