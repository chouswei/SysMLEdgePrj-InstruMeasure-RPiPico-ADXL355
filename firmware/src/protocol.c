/* protocol.c - frame encode/decode (PICO355-R5, PICO355-R6). Layout: CFG_PROTO_* (TODO(TBD)).
 *
 * Order in every function (protocol.h, docs/protocol.md "TBD rule"): argument check first
 * (PROTOCOL_ERR_ARG), then the TBD check (PROTOCOL_ERR_TBD) with *out_len / *consumed = 0 and
 * no output byte written, then the work. A value is used only after its CFG_IS_TBD test is
 * false, through an explicit cast.
 *
 * Frame layout built from the CFG_PROTO_* names only (no literal protocol numbers here):
 *   byte 0                 CFG_PROTO_SYNC
 *   byte 1                 frame type id (CFG_PROTO_TYPE_*)
 *   bytes 2 .. 2+LFB-1     payload length, LFB = CFG_PROTO_LENGTH_FIELD_BYTES, big-endian
 *   bytes up to HEADER_LEN header padding, written as zero, skipped on decode
 *   then payload           HEADER_LEN bytes in, payload_len bytes
 *   checksum               CFG_PROTO_CHECKSUM_KIND 0 = none; any other kind is NOT_IMPLEMENTED
 *
 * Payload layouts (placeholders, see TODO(TBD) below; the docs leave byte widths open):
 *   command  : code (1 byte), value (uint32, 4 bytes big-endian)
 *   response : code (1 byte), cmd_code (1 byte), value (uint32, 4 bytes big-endian)
 *   batch    : per sample x, y, z, each 24-bit two's complement, big-endian
 */
#include "protocol.h"

/* Frame-level values needed by every function: all of them must be decided. */
static bool proto_frame_decided(void)
{
    return !(CFG_IS_TBD(CFG_PROTO_SYNC) || CFG_IS_TBD(CFG_PROTO_HEADER_LEN) ||
             CFG_IS_TBD(CFG_PROTO_LENGTH_FIELD_BYTES) || CFG_IS_TBD(CFG_PROTO_CHECKSUM_KIND) ||
             CFG_IS_TBD(CFG_PROTO_MAX_PAYLOAD) || CFG_IS_TBD(CFG_PROTO_TYPE_SAMPLE_BATCH) ||
             CFG_IS_TBD(CFG_PROTO_TYPE_COMMAND) || CFG_IS_TBD(CFG_PROTO_TYPE_RESPONSE));
}

/* TODO(TBD) payload byte widths/order: docs/protocol.md leaves them open. The sizes below are
 * the C field widths of the payload types (uint8 / uint32 / 3 bytes per 20-bit raw axis), with
 * a big-endian placeholder order. Owner to confirm or replace. */
#define PROTO_CMD_PAYLOAD_LEN       (1u + 4u)
#define PROTO_RESP_PAYLOAD_LEN      (1u + 1u + 4u)
#define PROTO_AXIS_BYTES            3u
#define PROTO_SAMPLE_BYTES          (3u * PROTO_AXIS_BYTES)

static size_t proto_header_len(void)
{
    return (size_t)(uint32_t)CFG_PROTO_HEADER_LEN;
}

static size_t proto_len_bytes(void)
{
    return (size_t)(uint32_t)CFG_PROTO_LENGTH_FIELD_BYTES;
}

/* Header must hold sync, type and the length field; anything else is not a decided layout. */
static bool proto_header_fits(void)
{
    return proto_header_len() >= 2u + proto_len_bytes() && proto_len_bytes() >= 1u &&
           proto_len_bytes() <= 4u;
}

/* Checksum kind 0 is taken as "none" (docs/protocol.md lists none / sum / CRC in that order).
 * Any other kind has no algorithm in the docs, so it is not implemented here. */
static bool proto_checksum_none(void)
{
    return (uint32_t)CFG_PROTO_CHECKSUM_KIND == 0u;
}

/* Checks that need no output: payload bound and room for the whole frame. */
static protocol_rc_t proto_size_check(size_t payload_len, size_t out_cap)
{
    if (!proto_checksum_none()) return PROTOCOL_ERR_NOT_IMPLEMENTED;
    if (payload_len > (size_t)(uint32_t)CFG_PROTO_MAX_PAYLOAD) return PROTOCOL_ERR_TOO_LONG;
    if (proto_header_len() + payload_len > out_cap) return PROTOCOL_ERR_TOO_LONG;
    return PROTOCOL_OK;
}

static void put_be(uint8_t *dst, uint32_t v, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        dst[n - 1u - i] = (uint8_t)(v & 0xFFu);
        v >>= 8;
    }
}

static uint32_t get_be(const uint8_t *src, size_t n)
{
    uint32_t v = 0;
    size_t i;
    for (i = 0; i < n; i++) v = (v << 8) | src[i];
    return v;
}

/* Write header + payload (already in out + header) for a frame of payload_len bytes. */
static protocol_rc_t frame_wrap(uint8_t type, size_t payload_len, uint8_t *out, size_t out_cap,
                                size_t *out_len)
{
    size_t hl = proto_header_len();
    size_t lb = proto_len_bytes();
    size_t i;
    size_t max = (size_t)(uint32_t)CFG_PROTO_MAX_PAYLOAD;
    uint64_t lim;

    if (payload_len > max) return PROTOCOL_ERR_TOO_LONG;
    lim = (lb >= 4u) ? 0xFFFFFFFFull : ((uint64_t)1 << (8u * lb)) - 1u;
    if ((uint64_t)payload_len > lim) return PROTOCOL_ERR_TOO_LONG;
    if (hl + payload_len > out_cap) return PROTOCOL_ERR_TOO_LONG;

    for (i = 0; i < hl; i++) out[i] = 0;
    out[0] = (uint8_t)(uint32_t)CFG_PROTO_SYNC;
    out[1] = type;
    put_be(&out[2], (uint32_t)payload_len, lb);
    /* the payload was written at out + hl by the caller */
    *out_len = hl + payload_len;
    return PROTOCOL_OK;
}

protocol_rc_t protocol_encode_sample_batch(const adxl355_sample_t *samples, size_t count,
                                           uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t hl, pl, i, base;
    size_t max = (size_t)(uint32_t)CFG_PROTO_MAX_PAYLOAD;
    protocol_rc_t rc;

    if ((samples == NULL && count != 0) || out == NULL || out_len == NULL) return PROTOCOL_ERR_ARG;
    *out_len = 0;
    if (!proto_frame_decided() || !proto_header_fits()) return PROTOCOL_ERR_TBD;
    if (count > max / PROTO_SAMPLE_BYTES) return PROTOCOL_ERR_TOO_LONG;
    pl = count * PROTO_SAMPLE_BYTES;
    hl = proto_header_len();
    rc = proto_size_check(pl, out_cap);
    if (rc != PROTOCOL_OK) return rc;

    base = hl;
    for (i = 0; i < count; i++) {
        put_be(&out[base], (uint32_t)samples[i].x & 0xFFFFFFu, PROTO_AXIS_BYTES);
        put_be(&out[base + PROTO_AXIS_BYTES], (uint32_t)samples[i].y & 0xFFFFFFu, PROTO_AXIS_BYTES);
        put_be(&out[base + 2u * PROTO_AXIS_BYTES], (uint32_t)samples[i].z & 0xFFFFFFu, PROTO_AXIS_BYTES);
        base += PROTO_SAMPLE_BYTES;
    }
    rc = frame_wrap((uint8_t)(uint32_t)CFG_PROTO_TYPE_SAMPLE_BATCH, pl, out, out_cap, out_len);
    if (rc != PROTOCOL_OK) *out_len = 0;
    return rc;
}

protocol_rc_t protocol_encode_response(const protocol_response_t *resp,
                                       uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t hl, pl;
    protocol_rc_t rc;

    if (resp == NULL || out == NULL || out_len == NULL) return PROTOCOL_ERR_ARG;
    *out_len = 0;
    if (!proto_frame_decided() || !proto_header_fits()) return PROTOCOL_ERR_TBD;
    hl = proto_header_len();
    pl = PROTO_RESP_PAYLOAD_LEN;
    rc = proto_size_check(pl, out_cap);
    if (rc != PROTOCOL_OK) return rc;
    out[hl] = resp->code;
    out[hl + 1u] = resp->cmd_code;
    put_be(&out[hl + 2u], resp->value, 4u);
    rc = frame_wrap((uint8_t)(uint32_t)CFG_PROTO_TYPE_RESPONSE, pl, out, out_cap, out_len);
    if (rc != PROTOCOL_OK) *out_len = 0;
    return rc;
}

protocol_rc_t protocol_encode_command(const protocol_command_t *cmd,
                                      uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t hl, pl;
    protocol_rc_t rc;

    if (cmd == NULL || out == NULL || out_len == NULL) return PROTOCOL_ERR_ARG;
    *out_len = 0;
    if (!proto_frame_decided() || !proto_header_fits()) return PROTOCOL_ERR_TBD;
    hl = proto_header_len();
    pl = PROTO_CMD_PAYLOAD_LEN;
    rc = proto_size_check(pl, out_cap);
    if (rc != PROTOCOL_OK) return rc;
    out[hl] = cmd->code;
    put_be(&out[hl + 1u], cmd->value, 4u);
    rc = frame_wrap((uint8_t)(uint32_t)CFG_PROTO_TYPE_COMMAND, pl, out, out_cap, out_len);
    if (rc != PROTOCOL_OK) *out_len = 0;
    return rc;
}

protocol_rc_t protocol_decode_frame(const uint8_t *in, size_t in_len,
                                    protocol_frame_t *frame, size_t *consumed)
{
    size_t hl, lb, pl, i;
    uint8_t type;

    if (in == NULL || frame == NULL || consumed == NULL) return PROTOCOL_ERR_ARG;
    *consumed = 0;
    if (!proto_frame_decided() || !proto_header_fits()) return PROTOCOL_ERR_TBD;
    hl = proto_header_len();
    lb = proto_len_bytes();

    /* resync: drop bytes up to the next sync byte (or all of them when there is none) */
    if (in_len == 0) return PROTOCOL_ERR_SHORT;
    if (in[0] != (uint8_t)(uint32_t)CFG_PROTO_SYNC) {
        for (i = 1; i < in_len && in[i] != (uint8_t)(uint32_t)CFG_PROTO_SYNC; i++) {}
        *consumed = (i < in_len) ? i : in_len;
        return PROTOCOL_ERR_SYNC;
    }
    if (in_len < hl) return PROTOCOL_ERR_SHORT;

    type = in[1];
    pl = (size_t)get_be(&in[2], lb);
    if (pl > (size_t)(uint32_t)CFG_PROTO_MAX_PAYLOAD) {
        *consumed = 1;
        return PROTOCOL_ERR_TOO_LONG;
    }
    if (!proto_checksum_none()) return PROTOCOL_ERR_NOT_IMPLEMENTED;
    if (in_len < hl + pl) return PROTOCOL_ERR_SHORT;

    if (type != (uint8_t)(uint32_t)CFG_PROTO_TYPE_SAMPLE_BATCH &&
        type != (uint8_t)(uint32_t)CFG_PROTO_TYPE_COMMAND &&
        type != (uint8_t)(uint32_t)CFG_PROTO_TYPE_RESPONSE) {
        *consumed = 1;
        return PROTOCOL_ERR_TYPE;
    }

    frame->type = type;
    frame->payload = &in[hl];
    frame->payload_len = pl;
    *consumed = hl + pl;
    return PROTOCOL_OK;
}

protocol_rc_t protocol_decode_command(const uint8_t *payload, size_t len, protocol_command_t *cmd)
{
    if (payload == NULL || cmd == NULL) return PROTOCOL_ERR_ARG;
    if (!proto_frame_decided() || !proto_header_fits()) return PROTOCOL_ERR_TBD;
    if (len != PROTO_CMD_PAYLOAD_LEN) return PROTOCOL_ERR_SHORT;
    cmd->code = payload[0];
    cmd->value = get_be(&payload[1], 4u);
    return PROTOCOL_OK;
}
