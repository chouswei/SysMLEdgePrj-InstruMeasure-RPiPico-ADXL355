/*
 * protocol.h - USB CDC wire protocol: the ONLY owner of the frame layout.
 *
 * Requirements: PICO355-R5 (sample batches on sampleStream), PICO355-R6 (commands
 * and responses on commandChannel). Every layout choice is a CFG_PROTO_* in config.h
 * (design decision 2026-11-19, owner go t37u), mirrored in hmi/src/config.rs `link`
 * and described in docs/protocol.md. Frame = header + payload + CRC-16 (little-endian).
 * Model items: SampleBatch, CommandMessage / ConfigCommand, ResponseMessage.
 *
 * TBD rule (docs/protocol.md): each function first checks its arguments
 * (PROTOCOL_ERR_ARG), then returns PROTOCOL_ERR_TBD if any CFG_PROTO_* value it
 * needs is CFG_IS_TBD; then *out_len = 0 / *consumed = 0 and no output byte is
 * written. Values are used only through an explicit cast after that check.
 * Host tests may compile protocol.c with -DCFG_PROTO_...=<v> to inject a test
 * frame format (config.h wraps each value in #ifndef).
 * Header deps: stdint, stdbool, stddef, config.h, adxl355.h.
 */
#ifndef PICO355_PROTOCOL_H
#define PICO355_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "adxl355.h"
#include "config.h"

/** Return codes of the protocol module. */
typedef enum {
    PROTOCOL_OK = 0,
    PROTOCOL_ERR_ARG,
    PROTOCOL_ERR_SHORT,          /**< not enough bytes for a whole frame yet */
    PROTOCOL_ERR_SYNC,           /**< no sync found */
    PROTOCOL_ERR_CHECKSUM,
    PROTOCOL_ERR_TOO_LONG,       /**< payload exceeds CFG_PROTO_MAX_PAYLOAD or out buffer */
    PROTOCOL_ERR_TYPE,           /**< unknown frame type id */
    PROTOCOL_ERR_TBD,            /**< a CFG_PROTO_* value is still TODO(TBD) */
    PROTOCOL_ERR_NOT_IMPLEMENTED
} protocol_rc_t;

/** A decoded frame: type id (CFG_PROTO_TYPE_*) and a view of its payload inside the input. */
typedef struct {
    uint8_t type;
    const uint8_t *payload;
    size_t payload_len;
} protocol_frame_t;

/** Command payload (model ConfigCommand). code is a CFG_PROTO_CMD_*; value encoding TODO(TBD) command set. */
typedef struct {
    uint8_t code;
    uint32_t value;
} protocol_command_t;

/** Response payload (model ResponseMessage). code is a CFG_PROTO_RESP_*; fields TODO(TBD) command set. */
typedef struct {
    uint8_t code;
    uint8_t cmd_code;   /**< command being answered */
    uint32_t value;     /**< status or error detail */
} protocol_response_t;

/** Encode samples[0..count) as one sample-batch frame into out; *out_len = bytes written. */
protocol_rc_t protocol_encode_sample_batch(const adxl355_sample_t *samples, size_t count,
                                           uint8_t *out, size_t out_cap, size_t *out_len);

/** Encode a response frame into out; *out_len = bytes written. */
protocol_rc_t protocol_encode_response(const protocol_response_t *resp,
                                       uint8_t *out, size_t out_cap, size_t *out_len);

/** Encode a command frame (host side and tests); *out_len = bytes written. */
protocol_rc_t protocol_encode_command(const protocol_command_t *cmd,
                                      uint8_t *out, size_t out_cap, size_t *out_len);

/** Find and check one frame at the start of in; *consumed = bytes to drop (also on SYNC, CHECKSUM,
 *  TOO_LONG and TYPE errors: 1 byte, so a receiver rescans; 0 on SHORT: wait for more bytes). */
protocol_rc_t protocol_decode_frame(const uint8_t *in, size_t in_len,
                                    protocol_frame_t *frame, size_t *consumed);

/** Decode a command payload (frame of type CFG_PROTO_TYPE_COMMAND). */
protocol_rc_t protocol_decode_command(const uint8_t *payload, size_t len, protocol_command_t *cmd);

/** Decode a response payload (frame of type CFG_PROTO_TYPE_RESPONSE); host side and tests. */
protocol_rc_t protocol_decode_response(const uint8_t *payload, size_t len, protocol_response_t *resp);

/** Decode a sample-batch payload into out[0..max); *count = samples decoded (host side and tests). */
protocol_rc_t protocol_decode_sample_batch(const uint8_t *payload, size_t len,
                                           adxl355_sample_t *out, size_t max, size_t *count);

/** CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no final xor) of data[0..len). */
uint16_t protocol_crc16(const uint8_t *data, size_t len);

#endif /* PICO355_PROTOCOL_H */
