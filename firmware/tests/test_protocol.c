/*
 * test_protocol.c - host test for firmware/src/protocol.c (PICO355-R5, PICO355-R6).
 * Two builds of this file (see CMakeLists.txt):
 *   protocol         config.h defaults: every CFG_PROTO_* is still TBD, so the TBD-rule tests run.
 *   protocol_inject  CFG_PROTO_* come from -D test data (never the protocol): mechanics tests run.
 * Assertions follow protocol.h and docs/protocol.md "TBD rule"; no literal protocol numbers here.
 */
#include <stdio.h>
#include <string.h>

#include "protocol.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

#define ANY_TBD (CFG_IS_TBD(CFG_PROTO_SYNC) || CFG_IS_TBD(CFG_PROTO_HEADER_LEN) || \
                 CFG_IS_TBD(CFG_PROTO_LENGTH_FIELD_BYTES) || CFG_IS_TBD(CFG_PROTO_CHECKSUM_KIND) || \
                 CFG_IS_TBD(CFG_PROTO_MAX_PAYLOAD) || CFG_IS_TBD(CFG_PROTO_TYPE_SAMPLE_BATCH) || \
                 CFG_IS_TBD(CFG_PROTO_TYPE_COMMAND) || CFG_IS_TBD(CFG_PROTO_TYPE_RESPONSE))

#ifdef PF_REQUIRE_INJECT
#if ANY_TBD
#error "protocol_inject target must define every frame CFG_PROTO_* as test data"
#endif
#endif

#define FILL 0xEE

static void args_first(void)
{
    uint8_t out[16];
    size_t len = 99, used = 99;
    adxl355_sample_t s[1] = {{1, 2, 3}};
    protocol_command_t cmd = {0, 0};
    protocol_response_t resp = {0, 0, 0};
    protocol_frame_t frame;
    uint8_t in[4] = {0, 0, 0, 0};

    CHECK(protocol_encode_sample_batch(s, 1, NULL, sizeof out, &len) == PROTOCOL_ERR_ARG);
    CHECK(protocol_encode_sample_batch(s, 1, out, sizeof out, NULL) == PROTOCOL_ERR_ARG);
    CHECK(protocol_encode_sample_batch(NULL, 1, out, sizeof out, &len) == PROTOCOL_ERR_ARG);
    CHECK(protocol_encode_response(NULL, out, sizeof out, &len) == PROTOCOL_ERR_ARG);
    CHECK(protocol_encode_response(&resp, NULL, sizeof out, &len) == PROTOCOL_ERR_ARG);
    CHECK(protocol_encode_response(&resp, out, sizeof out, NULL) == PROTOCOL_ERR_ARG);
    CHECK(protocol_encode_command(NULL, out, sizeof out, &len) == PROTOCOL_ERR_ARG);
    CHECK(protocol_encode_command(&cmd, NULL, sizeof out, &len) == PROTOCOL_ERR_ARG);
    CHECK(protocol_encode_command(&cmd, out, sizeof out, NULL) == PROTOCOL_ERR_ARG);
    CHECK(protocol_decode_frame(NULL, sizeof in, &frame, &used) == PROTOCOL_ERR_ARG);
    CHECK(protocol_decode_frame(in, sizeof in, NULL, &used) == PROTOCOL_ERR_ARG);
    CHECK(protocol_decode_frame(in, sizeof in, &frame, NULL) == PROTOCOL_ERR_ARG);
    CHECK(protocol_decode_command(NULL, sizeof in, &cmd) == PROTOCOL_ERR_ARG);
    CHECK(protocol_decode_command(in, sizeof in, NULL) == PROTOCOL_ERR_ARG);
}

#if ANY_TBD
/* TBD state (docs/protocol.md TBD rule 2): PROTOCOL_ERR_TBD, *out_len / *consumed = 0, no byte written. */
static void tbd_state(void)
{
    uint8_t out[16];
    size_t len = 99, used = 99;
    adxl355_sample_t s[2] = {{1, 2, 3}, {4, 5, 6}};
    protocol_command_t cmd = {1, 2};
    protocol_response_t resp = {1, 2, 3};
    protocol_frame_t frame;
    uint8_t in[8];
    uint8_t pay[4] = {1, 2, 3, 4};
    protocol_command_t dcmd = {7, 8};
    size_t i;
    int clean = 1;

    memset(out, FILL, sizeof out);
    CHECK(protocol_encode_sample_batch(s, 2, out, sizeof out, &len) == PROTOCOL_ERR_TBD);
    CHECK(len == 0);
    for (i = 0; i < sizeof out; i++) if (out[i] != FILL) clean = 0;
    CHECK(clean);

    len = 99; memset(out, FILL, sizeof out); clean = 1;
    CHECK(protocol_encode_response(&resp, out, sizeof out, &len) == PROTOCOL_ERR_TBD);
    CHECK(len == 0);
    for (i = 0; i < sizeof out; i++) if (out[i] != FILL) clean = 0;
    CHECK(clean);

    len = 99; memset(out, FILL, sizeof out); clean = 1;
    CHECK(protocol_encode_command(&cmd, out, sizeof out, &len) == PROTOCOL_ERR_TBD);
    CHECK(len == 0);
    for (i = 0; i < sizeof out; i++) if (out[i] != FILL) clean = 0;
    CHECK(clean);

    memset(in, 0x5A, sizeof in);
    CHECK(protocol_decode_frame(in, sizeof in, &frame, &used) == PROTOCOL_ERR_TBD);
    CHECK(used == 0);

    CHECK(protocol_decode_command(pay, sizeof pay, &dcmd) == PROTOCOL_ERR_TBD);
    CHECK(dcmd.code == 7 && dcmd.value == 8); /* output left untouched */
}
#else
/* Decided (or injected) frame format: mechanics only, layout-agnostic. */
static void mechanics(void)
{
    uint8_t out[256];
    size_t len = 0, used = 0, i;
    adxl355_sample_t s[3] = {{1, -2, 3}, {-4, 5, -6}, {7, 8, -9}};
    protocol_command_t cmd = {(uint8_t)CFG_PROTO_CMD_SET_ODR, 0x1234u};
    protocol_command_t back = {0, 0};
    protocol_response_t resp = {(uint8_t)CFG_PROTO_RESP_ACK, (uint8_t)CFG_PROTO_CMD_SET_ODR, 5u};
    protocol_frame_t frame;
    static adxl355_sample_t many[(size_t)CFG_PROTO_MAX_PAYLOAD + 1u];

    /* command round trip: encode -> decode_frame -> decode_command */
    CHECK(protocol_encode_command(&cmd, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(len > 0 && len <= sizeof out);
    CHECK(protocol_decode_frame(out, len, &frame, &used) == PROTOCOL_OK);
    CHECK(used == len);
    CHECK(frame.type == (uint8_t)CFG_PROTO_TYPE_COMMAND);
    CHECK(frame.payload != NULL && frame.payload >= out && frame.payload + frame.payload_len <= out + len);
    CHECK(protocol_decode_command(frame.payload, frame.payload_len, &back) == PROTOCOL_OK);
    CHECK(back.code == cmd.code && back.value == cmd.value);

    /* a frame followed by extra bytes: consumed is the frame length only */
    CHECK(protocol_encode_command(&cmd, out, sizeof out, &len) == PROTOCOL_OK);
    for (i = 0; i < 4; i++) out[len + i] = 0x11;
    CHECK(protocol_decode_frame(out, len + 4, &frame, &used) == PROTOCOL_OK);
    CHECK(used == len);

    /* response and sample batch frames carry their own type id */
    CHECK(protocol_encode_response(&resp, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(protocol_decode_frame(out, len, &frame, &used) == PROTOCOL_OK);
    CHECK(frame.type == (uint8_t)CFG_PROTO_TYPE_RESPONSE && used == len && frame.payload_len > 0);

    CHECK(protocol_encode_sample_batch(s, 3, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(protocol_decode_frame(out, len, &frame, &used) == PROTOCOL_OK);
    CHECK(frame.type == (uint8_t)CFG_PROTO_TYPE_SAMPLE_BATCH && used == len && frame.payload_len > 0);
    CHECK(frame.payload_len <= (size_t)CFG_PROTO_MAX_PAYLOAD);

    /* truncated frame: not enough bytes yet, never OK */
    CHECK(protocol_encode_command(&cmd, out, sizeof out, &len) == PROTOCOL_OK);
    CHECK(protocol_decode_frame(out, len - 1, &frame, &used) == PROTOCOL_ERR_SHORT);

    /* bounds: output buffer too small, payload larger than the maximum */
    len = 99;
    CHECK(protocol_encode_command(&cmd, out, 1, &len) == PROTOCOL_ERR_TOO_LONG);
    CHECK(len == 0);
    len = 99;
    CHECK(protocol_encode_sample_batch(many, (size_t)CFG_PROTO_MAX_PAYLOAD + 1u, out, sizeof out, &len) != PROTOCOL_OK);
    (void)len;

    /* decode_command on an empty payload is not a command */
    CHECK(protocol_decode_command(out, 0, &back) != PROTOCOL_OK);
}

/* bytes without any sync: SYNC error and consumed says how many to drop (protocol.h). */
static void no_sync(void)
{
    uint8_t junk[8];
    protocol_frame_t frame;
    size_t used = 0, i;
    for (i = 0; i < sizeof junk; i++) junk[i] = (uint8_t)(CFG_PROTO_SYNC) ^ 0xFFu;
    CHECK(protocol_decode_frame(junk, sizeof junk, &frame, &used) == PROTOCOL_ERR_SYNC);
    CHECK(used > 0 && used <= sizeof junk);
}
#endif

int main(void)
{
    args_first();
#if ANY_TBD
    tbd_state();
#else
    mechanics();
    no_sync();
#endif
    if (g_fail) { printf("test_protocol: %d check(s) failed\n", g_fail); return 1; }
    printf("test_protocol: ok\n");
    return 0;
}
