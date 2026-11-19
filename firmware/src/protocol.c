/* protocol.c - frame encode/decode stubs (PICO355-R5, PICO355-R6). Layout: CFG_PROTO_* (TODO(TBD)). */
#include "protocol.h"

protocol_rc_t protocol_encode_sample_batch(const adxl355_sample_t *samples, size_t count,
                                           uint8_t *out, size_t out_cap, size_t *out_len)
{
    if ((samples == NULL && count != 0) || out == NULL || out_len == NULL) return PROTOCOL_ERR_ARG;
    (void)out_cap;
    *out_len = 0;
    return PROTOCOL_ERR_NOT_IMPLEMENTED;
}

protocol_rc_t protocol_encode_response(const protocol_response_t *resp,
                                       uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (resp == NULL || out == NULL || out_len == NULL) return PROTOCOL_ERR_ARG;
    (void)out_cap;
    *out_len = 0;
    return PROTOCOL_ERR_NOT_IMPLEMENTED;
}

protocol_rc_t protocol_encode_command(const protocol_command_t *cmd,
                                      uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (cmd == NULL || out == NULL || out_len == NULL) return PROTOCOL_ERR_ARG;
    (void)out_cap;
    *out_len = 0;
    return PROTOCOL_ERR_NOT_IMPLEMENTED;
}

protocol_rc_t protocol_decode_frame(const uint8_t *in, size_t in_len,
                                    protocol_frame_t *frame, size_t *consumed)
{
    if (in == NULL || frame == NULL || consumed == NULL) return PROTOCOL_ERR_ARG;
    (void)in_len;
    *consumed = 0;
    return PROTOCOL_ERR_NOT_IMPLEMENTED;
}

protocol_rc_t protocol_decode_command(const uint8_t *payload, size_t len, protocol_command_t *cmd)
{
    if (payload == NULL || cmd == NULL) return PROTOCOL_ERR_ARG;
    (void)len;
    return PROTOCOL_ERR_NOT_IMPLEMENTED;
}
