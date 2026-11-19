/* protocol.c - frame encode/decode (PICO355-R5, PICO355-R6). Layout: CFG_PROTO_* (TODO(TBD)).
 *
 * Order in every function (protocol.h, docs/protocol.md "TBD rule"): argument check first
 * (PROTOCOL_ERR_ARG), then the TBD check (PROTOCOL_ERR_TBD) with *out_len / *consumed = 0 and
 * no output byte written. Nothing here invents a frame byte: a value is used only after its
 * CFG_IS_TBD test is false.
 *
 * Still TBD even when every CFG_PROTO_* frame value is decided (docs/protocol.md, open TBD 6):
 * the payload field widths (command value, response fields, per-sample bytes) are not in
 * config.h, so the payload encoders stay on PROTOCOL_ERR_TBD until the owner decides them.
 */
#include "protocol.h"

/* TODO(TBD) payload field widths: command code/value, response code/cmd_code/value and
 * sample x,y,z byte encoding are not decided in config.h. Until they are, no payload is built. */
static bool proto_payload_decided(void)
{
    return false;
}

/* True when every frame-level CFG_PROTO_* value (sync, header, length field, checksum,
 * maximum payload, frame type ids) is decided. */
static bool proto_frame_decided(void)
{
    return !(CFG_IS_TBD(CFG_PROTO_SYNC) || CFG_IS_TBD(CFG_PROTO_HEADER_LEN) ||
             CFG_IS_TBD(CFG_PROTO_LENGTH_FIELD_BYTES) || CFG_IS_TBD(CFG_PROTO_CHECKSUM_KIND) ||
             CFG_IS_TBD(CFG_PROTO_MAX_PAYLOAD) || CFG_IS_TBD(CFG_PROTO_TYPE_SAMPLE_BATCH) ||
             CFG_IS_TBD(CFG_PROTO_TYPE_COMMAND) || CFG_IS_TBD(CFG_PROTO_TYPE_RESPONSE));
}

protocol_rc_t protocol_encode_sample_batch(const adxl355_sample_t *samples, size_t count,
                                           uint8_t *out, size_t out_cap, size_t *out_len)
{
    if ((samples == NULL && count != 0) || out == NULL || out_len == NULL) return PROTOCOL_ERR_ARG;
    *out_len = 0;
    if (!proto_frame_decided() || !proto_payload_decided()) return PROTOCOL_ERR_TBD;
    (void)out_cap;
    return PROTOCOL_ERR_TBD;
}

protocol_rc_t protocol_encode_response(const protocol_response_t *resp,
                                       uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (resp == NULL || out == NULL || out_len == NULL) return PROTOCOL_ERR_ARG;
    *out_len = 0;
    if (!proto_frame_decided() || !proto_payload_decided()) return PROTOCOL_ERR_TBD;
    (void)out_cap;
    return PROTOCOL_ERR_TBD;
}

protocol_rc_t protocol_encode_command(const protocol_command_t *cmd,
                                      uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (cmd == NULL || out == NULL || out_len == NULL) return PROTOCOL_ERR_ARG;
    *out_len = 0;
    if (!proto_frame_decided() || !proto_payload_decided()) return PROTOCOL_ERR_TBD;
    (void)out_cap;
    return PROTOCOL_ERR_TBD;
}

protocol_rc_t protocol_decode_frame(const uint8_t *in, size_t in_len,
                                    protocol_frame_t *frame, size_t *consumed)
{
    if (in == NULL || frame == NULL || consumed == NULL) return PROTOCOL_ERR_ARG;
    *consumed = 0;
    if (!proto_frame_decided()) return PROTOCOL_ERR_TBD;
    (void)in_len;
    return PROTOCOL_ERR_TBD;
}

protocol_rc_t protocol_decode_command(const uint8_t *payload, size_t len, protocol_command_t *cmd)
{
    if (payload == NULL || cmd == NULL) return PROTOCOL_ERR_ARG;
    if (!proto_payload_decided()) return PROTOCOL_ERR_TBD;
    (void)len;
    return PROTOCOL_ERR_TBD;
}
