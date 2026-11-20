/* protocol.c - frame encode/decode (PICO355-R5, PICO355-R6). Layout: CFG_PROTO_* (design decision 2026-11-19, owner go t37u).
 *
 * Order in every function (protocol.h, docs/protocol.md "TBD rule"): argument check first
 * (PROTOCOL_ERR_ARG), then the TBD check (PROTOCOL_ERR_TBD) with *out_len / *consumed = 0 and
 * no output byte written, then the work. A value is used only after its CFG_IS_TBD test is
 * false, through an explicit cast.
 *
 * Every layout choice comes from a named constant in config.h (docs/protocol.md "Frame
 * layout"); there are no literal layout numbers in this file:
 *   header   CFG_PROTO_HEADER_LEN bytes: CFG_PROTO_SYNC at CFG_PROTO_SYNC_OFFSET, type id
 *            (CFG_PROTO_TYPE_*) at CFG_PROTO_TYPE_OFFSET, payload length at
 *            CFG_PROTO_LENGTH_OFFSET (CFG_PROTO_LENGTH_FIELD_BYTES wide, order
 *            CFG_PROTO_LENGTH_MSB_FIRST); every other header byte is CFG_PROTO_PAD_BYTE.
 *   payload  follows the header, payload_len bytes; bytes no field covers are CFG_PROTO_PAD_BYTE.
 *   checksum CFG_PROTO_CHECKSUM_KIND: CFG_PROTO_CHECKSUM_NONE (no bytes) or CFG_PROTO_CHECKSUM_CRC16
 *            (CRC-16/CCITT-FALSE over header + payload, CFG_PROTO_CHECKSUM_LEN_BYTES bytes, little-endian,
 *            right after the payload). Any other kind is PROTOCOL_ERR_NOT_IMPLEMENTED.
 *   command  CFG_PROTO_CMD_PAYLOAD_LEN bytes: code at CFG_PROTO_CMD_CODE_OFFSET, value at
 *            CFG_PROTO_CMD_VALUE_OFFSET (CFG_PROTO_CMD_VALUE_BYTES, CFG_PROTO_VALUE_MSB_FIRST).
 *   response CFG_PROTO_RESP_PAYLOAD_LEN bytes: code, cmd_code, value at the CFG_PROTO_RESP_*
 *            offsets (value CFG_PROTO_RESP_VALUE_BYTES, CFG_PROTO_VALUE_MSB_FIRST).
 *   batch    per sample the model AccelSample axes x, y, z, each CFG_PROTO_SAMPLE_AXIS_BYTES
 *            two's complement, order CFG_PROTO_SAMPLE_MSB_FIRST.
 * A set of values that does not form a consistent layout (overlapping fields, a field outside
 * its header/payload, a width outside 1..4 bytes) is treated as not decided: PROTOCOL_ERR_TBD.
 */
#include "protocol.h"

#include "adxl355_regs.h" /* ADXL355_SAMPLE_BITS (model resolutionBits) */

#define BITS_PER_BYTE 8u
#define MAX_FIELD_BYTES sizeof(uint32_t) /* C field width of every multi-byte value */
#define SAMPLE_AXES 3u                   /* model AccelSample: x, y, z */

#define U(x) ((size_t)(uint32_t)(x)) /* use only after the CFG_IS_TBD test of x */

/* ---- decided checks (TBD rule) ----------------------------------------- */

static bool frame_decided(void)
{
    return !(CFG_IS_TBD(CFG_PROTO_SYNC) || CFG_IS_TBD(CFG_PROTO_HEADER_LEN) ||
             CFG_IS_TBD(CFG_PROTO_LENGTH_FIELD_BYTES) || CFG_IS_TBD(CFG_PROTO_CHECKSUM_KIND) ||
             CFG_IS_TBD(CFG_PROTO_MAX_PAYLOAD) || CFG_IS_TBD(CFG_PROTO_TYPE_SAMPLE_BATCH) ||
             CFG_IS_TBD(CFG_PROTO_TYPE_COMMAND) || CFG_IS_TBD(CFG_PROTO_TYPE_RESPONSE) ||
             CFG_IS_TBD(CFG_PROTO_SYNC_OFFSET) || CFG_IS_TBD(CFG_PROTO_TYPE_OFFSET) ||
             CFG_IS_TBD(CFG_PROTO_LENGTH_OFFSET) || CFG_IS_TBD(CFG_PROTO_LENGTH_MSB_FIRST) ||
             CFG_IS_TBD(CFG_PROTO_PAD_BYTE) || CFG_IS_TBD(CFG_PROTO_CHECKSUM_NONE));
}

static bool cmd_decided(void)
{
    return !(CFG_IS_TBD(CFG_PROTO_CMD_PAYLOAD_LEN) || CFG_IS_TBD(CFG_PROTO_CMD_CODE_OFFSET) ||
             CFG_IS_TBD(CFG_PROTO_CMD_VALUE_OFFSET) || CFG_IS_TBD(CFG_PROTO_CMD_VALUE_BYTES) ||
             CFG_IS_TBD(CFG_PROTO_VALUE_MSB_FIRST));
}

static bool resp_decided(void)
{
    return !(CFG_IS_TBD(CFG_PROTO_RESP_PAYLOAD_LEN) || CFG_IS_TBD(CFG_PROTO_RESP_CODE_OFFSET) ||
             CFG_IS_TBD(CFG_PROTO_RESP_CMD_CODE_OFFSET) || CFG_IS_TBD(CFG_PROTO_RESP_VALUE_OFFSET) ||
             CFG_IS_TBD(CFG_PROTO_RESP_VALUE_BYTES) || CFG_IS_TBD(CFG_PROTO_VALUE_MSB_FIRST));
}

static bool sample_decided(void)
{
    return !(CFG_IS_TBD(CFG_PROTO_SAMPLE_AXIS_BYTES) || CFG_IS_TBD(CFG_PROTO_SAMPLE_MSB_FIRST));
}

/* ---- layout consistency (called only after the matching *_decided) ------- */

static bool width_ok(size_t n)
{
    return n >= 1u && n <= MAX_FIELD_BYTES;
}

static bool flag_ok(size_t f)
{
    return f == 0u || f == 1u;
}

static bool inside(size_t off, size_t n, size_t total)
{
    return off <= total && n <= total - off;
}

static bool apart(size_t a, size_t an, size_t b, size_t bn)
{
    return a + an <= b || b + bn <= a;
}

static bool frame_fits(void)
{
    size_t hl = U(CFG_PROTO_HEADER_LEN), lb = U(CFG_PROTO_LENGTH_FIELD_BYTES);
    size_t so = U(CFG_PROTO_SYNC_OFFSET), to = U(CFG_PROTO_TYPE_OFFSET), lo = U(CFG_PROTO_LENGTH_OFFSET);
    return width_ok(lb) && flag_ok(U(CFG_PROTO_LENGTH_MSB_FIRST)) && inside(so, 1u, hl) &&
           inside(to, 1u, hl) && inside(lo, lb, hl) && apart(so, 1u, to, 1u) &&
           apart(so, 1u, lo, lb) && apart(to, 1u, lo, lb);
}

static bool cmd_fits(void)
{
    size_t pl = U(CFG_PROTO_CMD_PAYLOAD_LEN), vb = U(CFG_PROTO_CMD_VALUE_BYTES);
    size_t co = U(CFG_PROTO_CMD_CODE_OFFSET), vo = U(CFG_PROTO_CMD_VALUE_OFFSET);
    return width_ok(vb) && flag_ok(U(CFG_PROTO_VALUE_MSB_FIRST)) && inside(co, 1u, pl) &&
           inside(vo, vb, pl) && apart(co, 1u, vo, vb);
}

static bool resp_fits(void)
{
    size_t pl = U(CFG_PROTO_RESP_PAYLOAD_LEN), vb = U(CFG_PROTO_RESP_VALUE_BYTES);
    size_t co = U(CFG_PROTO_RESP_CODE_OFFSET), cc = U(CFG_PROTO_RESP_CMD_CODE_OFFSET);
    size_t vo = U(CFG_PROTO_RESP_VALUE_OFFSET);
    return width_ok(vb) && flag_ok(U(CFG_PROTO_VALUE_MSB_FIRST)) && inside(co, 1u, pl) &&
           inside(cc, 1u, pl) && inside(vo, vb, pl) && apart(co, 1u, cc, 1u) &&
           apart(co, 1u, vo, vb) && apart(cc, 1u, vo, vb);
}

static bool sample_fits(void)
{
    size_t ab = U(CFG_PROTO_SAMPLE_AXIS_BYTES);
    return width_ok(ab) && ab * BITS_PER_BYTE >= (size_t)ADXL355_SAMPLE_BITS &&
           flag_ok(U(CFG_PROTO_SAMPLE_MSB_FIRST));
}

static bool frame_ready(void) { return frame_decided() && frame_fits(); }
static bool cmd_ready(void) { return frame_ready() && cmd_decided() && cmd_fits(); }
static bool resp_ready(void) { return frame_ready() && resp_decided() && resp_fits(); }
static bool sample_ready(void) { return frame_ready() && sample_decided() && sample_fits(); }

/* ---- byte helpers -------------------------------------------------------- */

static void put_uint(uint8_t *dst, uint32_t v, size_t n, bool msb_first)
{
    size_t i;
    for (i = 0; i < n; i++) {
        dst[msb_first ? (n - 1u - i) : i] = (uint8_t)(v & UINT8_MAX);
        v >>= BITS_PER_BYTE;
    }
}

static uint32_t get_uint(const uint8_t *src, size_t n, bool msb_first)
{
    uint32_t v = 0;
    size_t i;
    for (i = 0; i < n; i++) v = (v << BITS_PER_BYTE) | src[msb_first ? i : (n - 1u - i)];
    return v;
}

/* true if v fits an unsigned field of n bytes */
static bool fits_unsigned(uint64_t v, size_t n)
{
    if (n >= MAX_FIELD_BYTES) return v <= UINT32_MAX;
    return v < ((uint64_t)1 << (BITS_PER_BYTE * n));
}

/* true if v fits a two's complement field of n bytes */
static bool fits_signed(int32_t v, size_t n)
{
    int64_t half;
    if (n >= MAX_FIELD_BYTES) return true;
    half = (int64_t)1 << (BITS_PER_BYTE * n - 1u);
    return (int64_t)v >= -half && (int64_t)v < half;
}

/* ---- checksum ------------------------------------------------------------- */

/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final xor (catalogue check 0x29B1
 * over "123456789"). Design decision (2026-11-19, owner go t37u), not a part spec. */
#define CRC16_POLY 0x1021u
#define CRC16_INIT 0xFFFFu
#define CRC16_TOP_BIT 0x8000u

uint16_t protocol_crc16(const uint8_t *data, size_t len)
{
    uint32_t crc = CRC16_INIT;
    size_t i;
    unsigned bit;
    for (i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i] << BITS_PER_BYTE;
        for (bit = 0; bit < BITS_PER_BYTE; bit++)
            crc = ((crc & CRC16_TOP_BIT) != 0u ? ((crc << 1) ^ CRC16_POLY) : (crc << 1)) & UINT16_MAX;
    }
    return (uint16_t)crc;
}

/* checksum bytes after the payload for the configured kind (0 for none); caller checked the kind is known */
static size_t checksum_len(void)
{
    return U(CFG_PROTO_CHECKSUM_KIND) == U(CFG_PROTO_CHECKSUM_NONE) ? 0u : U(CFG_PROTO_CHECKSUM_LEN_BYTES);
}

static bool checksum_known(void)
{
    return U(CFG_PROTO_CHECKSUM_KIND) == U(CFG_PROTO_CHECKSUM_NONE) ||
           U(CFG_PROTO_CHECKSUM_KIND) == U(CFG_PROTO_CHECKSUM_CRC16);
}

/* ---- frame build --------------------------------------------------------- */

/* Size checks before any byte is written; pad-fills header + payload when they pass. */
static protocol_rc_t frame_begin(size_t payload_len, uint8_t *out, size_t out_cap)
{
    size_t hl = U(CFG_PROTO_HEADER_LEN), i, cl;
    if (!checksum_known()) return PROTOCOL_ERR_NOT_IMPLEMENTED;
    cl = checksum_len();
    if (payload_len > U(CFG_PROTO_MAX_PAYLOAD)) return PROTOCOL_ERR_TOO_LONG;
    if (!fits_unsigned((uint64_t)payload_len, U(CFG_PROTO_LENGTH_FIELD_BYTES))) return PROTOCOL_ERR_TOO_LONG;
    if (payload_len > out_cap || hl > out_cap - payload_len || cl > out_cap - payload_len - hl)
        return PROTOCOL_ERR_TOO_LONG;
    for (i = 0; i < hl + payload_len; i++) out[i] = (uint8_t)(uint32_t)CFG_PROTO_PAD_BYTE;
    return PROTOCOL_OK;
}

/* Write the header fields once the payload is in place at out + header length. */
static void frame_end(uint8_t type, size_t payload_len, uint8_t *out, size_t *out_len)
{
    out[U(CFG_PROTO_SYNC_OFFSET)] = (uint8_t)(uint32_t)CFG_PROTO_SYNC;
    out[U(CFG_PROTO_TYPE_OFFSET)] = type;
    put_uint(&out[U(CFG_PROTO_LENGTH_OFFSET)], (uint32_t)payload_len, U(CFG_PROTO_LENGTH_FIELD_BYTES),
             U(CFG_PROTO_LENGTH_MSB_FIRST) == 1u);
    *out_len = U(CFG_PROTO_HEADER_LEN) + payload_len;
    if (checksum_len() != 0u) {
        put_uint(&out[*out_len], (uint32_t)protocol_crc16(out, *out_len), checksum_len(), false);
        *out_len += checksum_len();
    }
}

/* ---- public API ---------------------------------------------------------- */

protocol_rc_t protocol_encode_sample_batch(const adxl355_sample_t *samples, size_t count,
                                           uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t ab, sb, pl, i, base;
    bool msb;
    uint8_t *p;
    protocol_rc_t rc;

    if ((samples == NULL && count != 0) || out == NULL || out_len == NULL) return PROTOCOL_ERR_ARG;
    *out_len = 0;
    if (!sample_ready()) return PROTOCOL_ERR_TBD;
    ab = U(CFG_PROTO_SAMPLE_AXIS_BYTES);
    msb = U(CFG_PROTO_SAMPLE_MSB_FIRST) == 1u;
    sb = SAMPLE_AXES * ab;
    if (count > U(CFG_PROTO_MAX_PAYLOAD) / sb) return PROTOCOL_ERR_TOO_LONG;
    for (i = 0; i < count; i++) {
        if (!fits_signed(samples[i].x, ab) || !fits_signed(samples[i].y, ab) ||
            !fits_signed(samples[i].z, ab))
            return PROTOCOL_ERR_TOO_LONG;
    }
    pl = count * sb;
    rc = frame_begin(pl, out, out_cap);
    if (rc != PROTOCOL_OK) return rc;

    p = &out[U(CFG_PROTO_HEADER_LEN)];
    for (i = 0, base = 0; i < count; i++, base += sb) {
        put_uint(&p[base], (uint32_t)samples[i].x, ab, msb);
        put_uint(&p[base + ab], (uint32_t)samples[i].y, ab, msb);
        put_uint(&p[base + 2u * ab], (uint32_t)samples[i].z, ab, msb);
    }
    frame_end((uint8_t)(uint32_t)CFG_PROTO_TYPE_SAMPLE_BATCH, pl, out, out_len);
    return PROTOCOL_OK;
}

protocol_rc_t protocol_encode_response(const protocol_response_t *resp,
                                       uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t pl, vb;
    uint8_t *p;
    protocol_rc_t rc;

    if (resp == NULL || out == NULL || out_len == NULL) return PROTOCOL_ERR_ARG;
    *out_len = 0;
    if (!resp_ready()) return PROTOCOL_ERR_TBD;
    pl = U(CFG_PROTO_RESP_PAYLOAD_LEN);
    vb = U(CFG_PROTO_RESP_VALUE_BYTES);
    if (!fits_unsigned(resp->value, vb)) return PROTOCOL_ERR_TOO_LONG;
    rc = frame_begin(pl, out, out_cap);
    if (rc != PROTOCOL_OK) return rc;
    p = &out[U(CFG_PROTO_HEADER_LEN)];
    p[U(CFG_PROTO_RESP_CODE_OFFSET)] = resp->code;
    p[U(CFG_PROTO_RESP_CMD_CODE_OFFSET)] = resp->cmd_code;
    put_uint(&p[U(CFG_PROTO_RESP_VALUE_OFFSET)], resp->value, vb, U(CFG_PROTO_VALUE_MSB_FIRST) == 1u);
    frame_end((uint8_t)(uint32_t)CFG_PROTO_TYPE_RESPONSE, pl, out, out_len);
    return PROTOCOL_OK;
}

protocol_rc_t protocol_encode_command(const protocol_command_t *cmd,
                                      uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t pl, vb;
    uint8_t *p;
    protocol_rc_t rc;

    if (cmd == NULL || out == NULL || out_len == NULL) return PROTOCOL_ERR_ARG;
    *out_len = 0;
    if (!cmd_ready()) return PROTOCOL_ERR_TBD;
    pl = U(CFG_PROTO_CMD_PAYLOAD_LEN);
    vb = U(CFG_PROTO_CMD_VALUE_BYTES);
    if (!fits_unsigned(cmd->value, vb)) return PROTOCOL_ERR_TOO_LONG;
    rc = frame_begin(pl, out, out_cap);
    if (rc != PROTOCOL_OK) return rc;
    p = &out[U(CFG_PROTO_HEADER_LEN)];
    p[U(CFG_PROTO_CMD_CODE_OFFSET)] = cmd->code;
    put_uint(&p[U(CFG_PROTO_CMD_VALUE_OFFSET)], cmd->value, vb, U(CFG_PROTO_VALUE_MSB_FIRST) == 1u);
    frame_end((uint8_t)(uint32_t)CFG_PROTO_TYPE_COMMAND, pl, out, out_len);
    return PROTOCOL_OK;
}

protocol_rc_t protocol_decode_frame(const uint8_t *in, size_t in_len,
                                    protocol_frame_t *frame, size_t *consumed)
{
    size_t hl, so, pl, cl, i;
    uint8_t type, sync;

    if (in == NULL || frame == NULL || consumed == NULL) return PROTOCOL_ERR_ARG;
    *consumed = 0;
    if (!frame_ready()) return PROTOCOL_ERR_TBD;
    hl = U(CFG_PROTO_HEADER_LEN);
    so = U(CFG_PROTO_SYNC_OFFSET);
    sync = (uint8_t)(uint32_t)CFG_PROTO_SYNC;

    /* resync: a frame start p has its sync byte at p + so. Drop the starts before the next
     * candidate; with no candidate, drop every start whose sync byte is already in view. */
    if (in_len <= so) return PROTOCOL_ERR_SHORT;
    if (in[so] != sync) {
        for (i = 1; i + so < in_len && in[i + so] != sync; i++) {}
        *consumed = (i + so < in_len) ? i : in_len - so;
        return PROTOCOL_ERR_SYNC;
    }
    if (in_len < hl) return PROTOCOL_ERR_SHORT;

    type = in[U(CFG_PROTO_TYPE_OFFSET)];
    pl = (size_t)get_uint(&in[U(CFG_PROTO_LENGTH_OFFSET)], U(CFG_PROTO_LENGTH_FIELD_BYTES),
                          U(CFG_PROTO_LENGTH_MSB_FIRST) == 1u);
    if (!checksum_known()) return PROTOCOL_ERR_NOT_IMPLEMENTED;
    cl = checksum_len();
    /* a false sync byte is dropped alone so the scan continues one byte on; length and type are
     * checked before waiting for the payload so garbage never stalls the receiver */
    if (pl > U(CFG_PROTO_MAX_PAYLOAD)) {
        *consumed = 1;
        return PROTOCOL_ERR_TOO_LONG;
    }
    if (type != (uint8_t)(uint32_t)CFG_PROTO_TYPE_SAMPLE_BATCH &&
        type != (uint8_t)(uint32_t)CFG_PROTO_TYPE_COMMAND &&
        type != (uint8_t)(uint32_t)CFG_PROTO_TYPE_RESPONSE) {
        *consumed = 1;
        return PROTOCOL_ERR_TYPE;
    }
    if (in_len - hl < pl || in_len - hl - pl < cl) return PROTOCOL_ERR_SHORT;
    if (cl != 0u) {
        uint32_t want = get_uint(&in[hl + pl], cl, false);
        if (want != (uint32_t)protocol_crc16(in, hl + pl)) {
            *consumed = 1;
            return PROTOCOL_ERR_CHECKSUM;
        }
    }

    frame->type = type;
    frame->payload = &in[hl];
    frame->payload_len = pl;
    *consumed = hl + pl + cl;
    return PROTOCOL_OK;
}

protocol_rc_t protocol_decode_command(const uint8_t *payload, size_t len, protocol_command_t *cmd)
{
    if (payload == NULL || cmd == NULL) return PROTOCOL_ERR_ARG;
    if (!cmd_ready()) return PROTOCOL_ERR_TBD;
    if (len != U(CFG_PROTO_CMD_PAYLOAD_LEN)) return PROTOCOL_ERR_SHORT;
    cmd->code = payload[U(CFG_PROTO_CMD_CODE_OFFSET)];
    cmd->value = get_uint(&payload[U(CFG_PROTO_CMD_VALUE_OFFSET)], U(CFG_PROTO_CMD_VALUE_BYTES),
                          U(CFG_PROTO_VALUE_MSB_FIRST) == 1u);
    return PROTOCOL_OK;
}

protocol_rc_t protocol_decode_response(const uint8_t *payload, size_t len, protocol_response_t *resp)
{
    if (payload == NULL || resp == NULL) return PROTOCOL_ERR_ARG;
    if (!resp_ready()) return PROTOCOL_ERR_TBD;
    if (len != U(CFG_PROTO_RESP_PAYLOAD_LEN)) return PROTOCOL_ERR_SHORT;
    resp->code = payload[U(CFG_PROTO_RESP_CODE_OFFSET)];
    resp->cmd_code = payload[U(CFG_PROTO_RESP_CMD_CODE_OFFSET)];
    resp->value = get_uint(&payload[U(CFG_PROTO_RESP_VALUE_OFFSET)], U(CFG_PROTO_RESP_VALUE_BYTES),
                           U(CFG_PROTO_VALUE_MSB_FIRST) == 1u);
    return PROTOCOL_OK;
}

/* sign-extend an n-byte two's complement value read as unsigned */
static int32_t sign_extend(uint32_t v, size_t n)
{
    uint32_t sign;
    if (n >= MAX_FIELD_BYTES) return (int32_t)v;
    sign = (uint32_t)1u << (BITS_PER_BYTE * n - 1u);
    return (int32_t)((v ^ sign) - sign);
}

protocol_rc_t protocol_decode_sample_batch(const uint8_t *payload, size_t len,
                                           adxl355_sample_t *out, size_t max, size_t *count)
{
    size_t ab, sb, n, i;
    bool msb;

    if ((payload == NULL && len != 0) || (out == NULL && max != 0) || count == NULL) return PROTOCOL_ERR_ARG;
    *count = 0;
    if (!sample_ready()) return PROTOCOL_ERR_TBD;
    ab = U(CFG_PROTO_SAMPLE_AXIS_BYTES);
    msb = U(CFG_PROTO_SAMPLE_MSB_FIRST) == 1u;
    sb = SAMPLE_AXES * ab;
    if (len % sb != 0u) return PROTOCOL_ERR_SHORT;
    n = len / sb;
    if (n > max) return PROTOCOL_ERR_TOO_LONG;
    for (i = 0; i < n; i++) {
        const uint8_t *p = &payload[i * sb];
        out[i].x = sign_extend(get_uint(p, ab, msb), ab);
        out[i].y = sign_extend(get_uint(p + ab, ab, msb), ab);
        out[i].z = sign_extend(get_uint(p + 2u * ab, ab, msb), ab);
    }
    *count = n;
    return PROTOCOL_OK;
}
