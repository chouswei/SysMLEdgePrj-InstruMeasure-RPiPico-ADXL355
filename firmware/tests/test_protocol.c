/*
 * test_protocol.c - host test for firmware/src/protocol.c on the REAL config.h frame layout
 * (PICO355-R5, PICO355-R6). No -D injection: every CFG_PROTO_* is the shipped value.
 * Layout, command set and checksum are design decisions (2026-11-19, owner go t37u);
 * they are not part specs. The golden byte vectors below are the same literal bytes as in
 * hmi/tests/golden.rs (docs/protocol.md "Golden frames"); each line starts with the marker
 * "GOLDEN <name>:" so the two files can be diffed by tools.
 */
#include <stdio.h>
#include <string.h>

#include "protocol.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

/* GOLDEN BATCH: a5 01 12 00 01 00 00 fe ff ff 03 00 00 00 00 f8 ff ff 07 00 00 00 b1 b3 */
static const uint8_t G_BATCH[] = {0xa5,0x01,0x12,0x00,0x01,0x00,0x00,0xfe,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0xf8,0xff,0xff,0x07,0x00,0x00,0x00,0xb1,0xb3};
/* GOLDEN CMD_ODR: a5 02 05 00 01 a0 0f 00 00 e7 ed */
static const uint8_t G_CMD_ODR[] = {0xa5,0x02,0x05,0x00,0x01,0xa0,0x0f,0x00,0x00,0xe7,0xed};
/* GOLDEN CMD_RANGE: a5 02 05 00 02 03 00 00 00 ae 5e */
static const uint8_t G_CMD_RANGE[] = {0xa5,0x02,0x05,0x00,0x02,0x03,0x00,0x00,0x00,0xae,0x5e};
/* GOLDEN CMD_FILTER: a5 02 05 00 03 06 00 00 00 ba 48 */
static const uint8_t G_CMD_FILTER[] = {0xa5,0x02,0x05,0x00,0x03,0x06,0x00,0x00,0x00,0xba,0x48};
/* GOLDEN RESP_ACK: a5 03 06 00 01 01 00 00 00 00 0e bc */
static const uint8_t G_RESP_ACK[] = {0xa5,0x03,0x06,0x00,0x01,0x01,0x00,0x00,0x00,0x00,0x0e,0xbc};
/* GOLDEN RESP_ERROR: a5 03 06 00 02 02 01 00 00 00 88 ea */
static const uint8_t G_RESP_ERROR[] = {0xa5,0x03,0x06,0x00,0x02,0x02,0x01,0x00,0x00,0x00,0x88,0xea};
/* GOLDEN UNKNOWN_TYPE: a5 7f 01 00 00 94 b9 */
static const uint8_t G_UNKNOWN_TYPE[] = {0xa5,0x7f,0x01,0x00,0x00,0x94,0xb9};

static const adxl355_sample_t G_SAMPLES[3] = {{1, -2, 3}, {-524288, 524287, 0}};

#define N(a) (sizeof(a) / sizeof((a)[0]))

static const struct { const char *name; const uint8_t *b; size_t n; } k_golden[] = {
    {"BATCH", G_BATCH, sizeof G_BATCH},       {"CMD_ODR", G_CMD_ODR, sizeof G_CMD_ODR},
    {"CMD_RANGE", G_CMD_RANGE, sizeof G_CMD_RANGE}, {"CMD_FILTER", G_CMD_FILTER, sizeof G_CMD_FILTER},
    {"RESP_ACK", G_RESP_ACK, sizeof G_RESP_ACK},   {"RESP_ERROR", G_RESP_ERROR, sizeof G_RESP_ERROR},
};

static void crc_check_value(void)
{
    /* CRC-16/CCITT-FALSE catalogue check value over "123456789" is 0x29B1 */
    CHECK(protocol_crc16((const uint8_t *)"123456789", 9u) == 0x29B1u);
}

/* C3 golden bytes, encode side */
static void golden_encode(void)
{
    uint8_t out[64];
    size_t len = 0;
    protocol_command_t c = {(uint8_t)CFG_PROTO_CMD_SET_ODR, 4000u};
    protocol_response_t r = {(uint8_t)CFG_PROTO_RESP_ACK, (uint8_t)CFG_PROTO_CMD_SET_ODR, 0u};

    CHECK(protocol_encode_sample_batch(G_SAMPLES, 2, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(len == sizeof G_BATCH && memcmp(out, G_BATCH, len) == 0);
    CHECK(protocol_encode_command(&c, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(len == sizeof G_CMD_ODR && memcmp(out, G_CMD_ODR, len) == 0);
    c.code = (uint8_t)CFG_PROTO_CMD_SET_RANGE; c.value = 3u;
    CHECK(protocol_encode_command(&c, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(len == sizeof G_CMD_RANGE && memcmp(out, G_CMD_RANGE, len) == 0);
    c.code = (uint8_t)CFG_PROTO_CMD_SET_FILTER; c.value = 6u;
    CHECK(protocol_encode_command(&c, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(len == sizeof G_CMD_FILTER && memcmp(out, G_CMD_FILTER, len) == 0);
    CHECK(protocol_encode_response(&r, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(len == sizeof G_RESP_ACK && memcmp(out, G_RESP_ACK, len) == 0);
    r.code = (uint8_t)CFG_PROTO_RESP_ERROR; r.cmd_code = (uint8_t)CFG_PROTO_CMD_SET_RANGE; r.value = 1u;
    CHECK(protocol_encode_response(&r, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(len == sizeof G_RESP_ERROR && memcmp(out, G_RESP_ERROR, len) == 0);
}

/* C1/C3 decode side: golden bytes give back the original values */
static void golden_decode(void)
{
    protocol_frame_t f;
    size_t used = 0, n = 0;
    adxl355_sample_t s[4];
    protocol_command_t c;
    protocol_response_t r;

    CHECK(protocol_decode_frame(G_BATCH, sizeof G_BATCH, &f, &used) == PROTOCOL_OK);
    CHECK(used == sizeof G_BATCH && f.type == (uint8_t)CFG_PROTO_TYPE_SAMPLE_BATCH);
    CHECK(protocol_decode_sample_batch(f.payload, f.payload_len, s, N(s), &n) == PROTOCOL_OK);
    CHECK(n == 2 && s[0].x == 1 && s[0].y == -2 && s[0].z == 3);
    CHECK(s[1].x == -524288 && s[1].y == 524287 && s[1].z == 0);

    CHECK(protocol_decode_frame(G_CMD_ODR, sizeof G_CMD_ODR, &f, &used) == PROTOCOL_OK);
    CHECK(f.type == (uint8_t)CFG_PROTO_TYPE_COMMAND);
    CHECK(protocol_decode_command(f.payload, f.payload_len, &c) == PROTOCOL_OK);
    CHECK(c.code == (uint8_t)CFG_PROTO_CMD_SET_ODR && c.value == 4000u);

    CHECK(protocol_decode_frame(G_RESP_ERROR, sizeof G_RESP_ERROR, &f, &used) == PROTOCOL_OK);
    CHECK(f.type == (uint8_t)CFG_PROTO_TYPE_RESPONSE);
    CHECK(protocol_decode_response(f.payload, f.payload_len, &r) == PROTOCOL_OK);
    CHECK(r.code == (uint8_t)CFG_PROTO_RESP_ERROR && r.cmd_code == (uint8_t)CFG_PROTO_CMD_SET_RANGE && r.value == 1u);
}

/* C1 round trip with values the goldens do not hold, no inject build */
static void round_trip(void)
{
    uint8_t out[CFG_USB_FRAME_BUF_BYTES];
    adxl355_sample_t in[CFG_SAMPLE_BUFFER_DEPTH], back[CFG_SAMPLE_BUFFER_DEPTH];
    protocol_frame_t f;
    size_t i, len = 0, used = 0, n = 0;
    protocol_command_t c = {(uint8_t)CFG_PROTO_CMD_SET_FILTER, 0xFFFFFFFFu}, cb;
    protocol_response_t r = {(uint8_t)CFG_PROTO_RESP_STATUS, 0xFFu, 0x01020304u}, rb;

    for (i = 0; i < CFG_SAMPLE_BUFFER_DEPTH; i++) {
        in[i].x = (int32_t)i * 8191 - 262144; in[i].y = -(int32_t)i * 4099; in[i].z = (int32_t)(i % 2) ? 524287 : -524288;
    }
    CHECK(protocol_encode_sample_batch(in, CFG_SAMPLE_BUFFER_DEPTH, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(protocol_decode_frame(out, len, &f, &used) == PROTOCOL_OK && used == len);
    CHECK(protocol_decode_sample_batch(f.payload, f.payload_len, back, CFG_SAMPLE_BUFFER_DEPTH, &n) == PROTOCOL_OK);
    CHECK(n == CFG_SAMPLE_BUFFER_DEPTH && memcmp(in, back, sizeof in) == 0);
    /* empty batch is a legal frame */
    CHECK(protocol_encode_sample_batch(in, 0, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(protocol_decode_frame(out, len, &f, &used) == PROTOCOL_OK && f.payload_len == 0);

    CHECK(protocol_encode_command(&c, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(protocol_decode_frame(out, len, &f, &used) == PROTOCOL_OK);
    CHECK(protocol_decode_command(f.payload, f.payload_len, &cb) == PROTOCOL_OK && cb.code == c.code && cb.value == c.value);
    CHECK(protocol_encode_response(&r, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(protocol_decode_frame(out, len, &f, &used) == PROTOCOL_OK);
    CHECK(protocol_decode_response(f.payload, f.payload_len, &rb) == PROTOCOL_OK &&
          rb.code == r.code && rb.cmd_code == r.cmd_code && rb.value == r.value);
}

/* C4: flip any one byte of a golden frame after the sync byte: never decodes as the same frame
 * silently; payload and CRC bytes give PROTOCOL_ERR_CHECKSUM. */
static void checksum_fail(void)
{
    size_t g, i;
    protocol_frame_t f;
    for (g = 0; g < N(k_golden); g++) {
        uint8_t b[64];
        size_t hl = (size_t)CFG_PROTO_HEADER_LEN;
        for (i = hl; i < k_golden[g].n; i++) {   /* payload and checksum bytes */
            size_t used = 99;
            memcpy(b, k_golden[g].b, k_golden[g].n);
            b[i] ^= 0x01u;
            CHECK(protocol_decode_frame(b, k_golden[g].n, &f, &used) == PROTOCOL_ERR_CHECKSUM);
            CHECK(used == 1u);                   /* drop the false sync byte and rescan */
        }
    }
}

/* C5: golden cut at every length 0..n-1 is SHORT and consumes nothing */
static void truncated(void)
{
    size_t g, k;
    protocol_frame_t f;
    for (g = 0; g < N(k_golden); g++) {
        for (k = 0; k < k_golden[g].n; k++) {
            size_t used = 99;
            CHECK(protocol_decode_frame(k_golden[g].b, k, &f, &used) == PROTOCOL_ERR_SHORT);
            CHECK(used == 0);
        }
    }
}

/* receiver loop as main.c and the HMI run it: drop `used`, retry, until a frame decodes */
static int scan(const uint8_t *in, size_t len, protocol_frame_t *f, size_t *off)
{
    size_t pos = 0, used = 0;
    while (pos < len) {
        protocol_rc_t rc = protocol_decode_frame(in + pos, len - pos, f, &used);
        if (rc == PROTOCOL_OK) { *off = pos; return 1; }
        if (rc == PROTOCOL_ERR_SHORT) return 0;
        if (used == 0) return 0;
        pos += used;
    }
    return 0;
}

/* C6: leading garbage (including false sync bytes) before a valid frame */
static void sync_loss(void)
{
    static const uint8_t junk[] = {0x00, 0xA5, 0x7F, 0xA5, 0xA5, 0x01, 0x02};
    uint8_t b[sizeof junk + sizeof G_CMD_ODR];
    protocol_frame_t f;
    size_t off = 0, k;

    for (k = 0; k <= sizeof junk; k++) {          /* every garbage length 0..sizeof junk */
        memcpy(b, junk, k);
        memcpy(b + k, G_CMD_ODR, sizeof G_CMD_ODR);
        CHECK(scan(b, k + sizeof G_CMD_ODR, &f, &off) == 1);
        CHECK(off == k && f.type == (uint8_t)CFG_PROTO_TYPE_COMMAND && f.payload_len == 5u);
    }
    /* pure garbage with no sync: SYNC and consumed > 0 */
    {
        uint8_t g[8];
        size_t used = 0;
        memset(g, 0x11, sizeof g);
        CHECK(protocol_decode_frame(g, sizeof g, &f, &used) == PROTOCOL_ERR_SYNC);
        CHECK(used > 0 && used <= sizeof g);
    }
}

/* C7: command set and type ids; unknown frame type */
static void command_set_and_types(void)
{
    protocol_frame_t f;
    size_t used = 99;
    CHECK(protocol_decode_frame(G_UNKNOWN_TYPE, sizeof G_UNKNOWN_TYPE, &f, &used) == PROTOCOL_ERR_TYPE);
    CHECK(used == 1u);
    CHECK(CFG_PROTO_CMD_SET_ODR != CFG_PROTO_CMD_SET_RANGE && CFG_PROTO_CMD_SET_RANGE != CFG_PROTO_CMD_SET_FILTER &&
          CFG_PROTO_CMD_SET_ODR != CFG_PROTO_CMD_SET_FILTER);
    CHECK(CFG_PROTO_RESP_ACK != CFG_PROTO_RESP_ERROR && CFG_PROTO_RESP_ERROR != CFG_PROTO_RESP_STATUS &&
          CFG_PROTO_RESP_ACK != CFG_PROTO_RESP_STATUS);
    /* a length field above the maximum is rejected, one byte dropped */
    {
        uint8_t big[8] = {0xA5, 0x01, 0xFF, 0xFF, 0, 0, 0, 0};
        CHECK(protocol_decode_frame(big, sizeof big, &f, &used) == PROTOCOL_ERR_TOO_LONG && used == 1u);
    }
}

static void bounds_and_args(void)
{
    uint8_t out[16];
    size_t len = 99;
    adxl355_sample_t s[1] = {{1, 2, 3}};
    protocol_command_t c = {1, 0};
    protocol_frame_t f;
    size_t used = 7;
    uint8_t in[4] = {0, 0, 0, 0};

    CHECK(protocol_encode_command(&c, out, 1, &len) == PROTOCOL_ERR_TOO_LONG && len == 0);
    CHECK(protocol_encode_sample_batch(NULL, 1, out, sizeof out, &len) == PROTOCOL_ERR_ARG);
    CHECK(protocol_encode_sample_batch(s, 1, NULL, sizeof out, &len) == PROTOCOL_ERR_ARG);
    CHECK(protocol_decode_frame(NULL, sizeof in, &f, &used) == PROTOCOL_ERR_ARG);
    CHECK(protocol_decode_frame(in, sizeof in, NULL, &used) == PROTOCOL_ERR_ARG);
    CHECK(protocol_decode_frame(in, sizeof in, &f, NULL) == PROTOCOL_ERR_ARG);
    CHECK(protocol_decode_command(NULL, 5, &c) == PROTOCOL_ERR_ARG);
    CHECK(protocol_decode_command(in, sizeof in, &c) == PROTOCOL_ERR_SHORT);   /* wrong payload length */
}

int main(void)
{
    crc_check_value();
    golden_encode();
    golden_decode();
    round_trip();
    checksum_fail();
    truncated();
    sync_loss();
    command_set_and_types();
    bounds_and_args();
    if (g_fail) { printf("test_protocol: %d check(s) failed\n", g_fail); return 1; }
    printf("test_protocol: ok\n");
    return 0;
}
