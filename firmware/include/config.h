/*
 * config.h - the ONLY place for firmware constants (confirmed and TBD).
 *
 * Requirements: PICO355-R1 (default ODR 4000 Hz), PICO355-R2 (runtime config),
 * PICO355-R4 (pins, SPI0, DRDY), PICO355-R5 (buffer, frame format),
 * PICO355-R6 (command set), PICO355-R7 (state machine).
 * Model pin: desk sysmledgeprj-rpipico-adxl355 @ e364458f462c7b2153b6e6acd914b187e32b97fb.
 *
 * Rules: every macro is prefixed CFG_. A value that is not in the model files,
 * AGENTS.md or the ADXL355 datasheet Rev. D carries TODO(TBD) and a sentinel,
 * never an invented number. The CFG_PROTO_* names are mirrored 1:1 in
 * hmi/src/tbd.rs (without CFG_) and described in docs/protocol.md.
 *
 * TBD sentinel rule ("not configured", see docs/protocol.md "TBD rule"):
 *   - Every numeric TBD constant is CFG_TBD, a 64-bit all-ones value. Every CFG_
 *     value that is ever decided fits in 32 bits (byte fields, lengths, codes,
 *     Hz), so CFG_TBD never equals a legal value.
 *   - Test with CFG_IS_TBD(x); it works in C expressions and in #if.
 *   - A module that needs a TBD constant returns its own *_ERR_TBD code and
 *     touches no hardware and no output bytes.
 *   - Use a value only after CFG_IS_TBD is false, with an explicit cast to the
 *     field width. An unchecked implicit use of CFG_TBD in a byte or 32-bit
 *     field is a -Woverflow / -Wtype-limits error under -Werror (by design).
 *   - Enum-typed TBDs use the enum's own *_UNSET member (ADXL355_RANGE_UNSET,
 *     FSM_STATE_UNSET, FSM_POLICY_UNSET), which is not a legal member value.
 *   - Constants that must be a usable number to compile (array sizes) carry a
 *     *_IS_TBD flag instead (CFG_SAMPLE_BUFFER_DEPTH_IS_TBD).
 *   - Every CFG_ value below is wrapped in #ifndef, so a host test target may
 *     inject test values with -DCFG_...=<v> (test data only, never a design value).
 *
 * Hardware TBDs (comment only, no constant):
 *   - TODO(TBD) LDO output check: TPS7A2033 output not yet checked against its
 *     datasheet (PICO355-R3).
 *   - TODO(TBD) cable pinout: X5 cable assumed straight 1:1, unverified (model X5Port).
 *
 * Header deps: none (macros only).
 */
#ifndef PICO355_CONFIG_H
#define PICO355_CONFIG_H

/* ---- Sentinel ---------------------------------------------------------- */
#define CFG_TBD 0xFFFFFFFFFFFFFFFFull /* TODO(TBD) sentinel "not configured": outside every legal (<= 32-bit) CFG_ value */
#define CFG_IS_TBD(x) ((x) == CFG_TBD) /* TODO(TBD) sentinel test: true while x is still not configured; usable in #if */

/* ---- Pins (confirmed, model X5Port / RaspberryPiPico) ---------------------- */
#define CFG_PIN_SPI0_RX_MISO 16 /* model X5Port pin 3: SPI0_RX GP16 <- MISO (PICO355-R4) */
#define CFG_PIN_SPI0_CSN 17 /* model X5Port pin 1: SPI0_CSN GP17 -> CS (PICO355-R4) */
#define CFG_PIN_SPI0_SCK 18 /* model X5Port pin 4: SPI0_SCK GP18 -> SCLK (PICO355-R4) */
#define CFG_PIN_SPI0_TX_MOSI 19 /* model X5Port pin 2: SPI0_TX GP19 -> MOSI (PICO355-R4) */
#define CFG_PIN_DRDY_IN 22 /* model X5Port pin 5: GP22 <- DRDY, input only, never driven; INT1/INT2 unconnected (PICO355-R4) */
#define CFG_SPI_INSTANCE_INDEX 0 /* model SPI0 bus (PICO355-R4) */

/* ---- SPI (TBD) ----------------------------------------------------------- */
#ifndef CFG_SPI_CLOCK_HZ
#define CFG_SPI_CLOCK_HZ CFG_TBD /* TODO(TBD) SPI clock: not decided (model SpiBusPort) */
#endif
#ifndef CFG_SPI_MODE
#define CFG_SPI_MODE CFG_TBD /* TODO(TBD) SPI mode (CPOL/CPHA, 0..3 once decided): not decided (model SpiBusPort) */
#endif

/* ---- Sensor defaults ------------------------------------------------------- */
#ifndef CFG_DEFAULT_ODR_HZ
#define CFG_DEFAULT_ODR_HZ 4000u /* default output data rate, PICO355-R1 (defaultStreamRate 4000 Hz) */
#endif
#ifndef CFG_DEFAULT_RANGE
#define CFG_DEFAULT_RANGE ADXL355_RANGE_UNSET /* TODO(TBD) default range: model AccelRange says default TBD */
#endif
#ifndef CFG_DEFAULT_FILTER
#define CFG_DEFAULT_FILTER CFG_TBD /* TODO(TBD) filter settings exposed: default filter code not decided (PICO355-R2) */
#endif
#ifndef CFG_FIFO_WATERMARK
#define CFG_FIFO_WATERMARK CFG_TBD /* TODO(TBD) FIFO watermark: not decided (PICO355-R4) */
#endif

/* ---- Sample buffer (TBD) ----------------------------------------------- */
#ifndef CFG_SAMPLE_BUFFER_DEPTH
#define CFG_SAMPLE_BUFFER_DEPTH 1u /* TODO(TBD) buffer depth: 1 is the smallest compilable array size, not a design value; it makes "full" and overflow reachable after one push (PICO355-R5) */
#endif
#ifndef CFG_SAMPLE_BUFFER_DEPTH_IS_TBD
#define CFG_SAMPLE_BUFFER_DEPTH_IS_TBD 1 /* TODO(TBD) buffer depth flag: 1 while CFG_SAMPLE_BUFFER_DEPTH is the placeholder; set 0 when decided */
#endif
#ifndef CFG_SAMPLE_BUFFER_OVERFLOW_POLICY
#define CFG_SAMPLE_BUFFER_OVERFLOW_POLICY CFG_TBD /* TODO(TBD) buffer depth overflow policy: drop-oldest or drop-newest not decided; behaviour while TBD in sample_buffer.h (PICO355-R5) */
#endif

/* ---- USB frame format (TBD), CFG_PROTO_* ----------------------------------- */
#ifndef CFG_PROTO_SYNC
#define CFG_PROTO_SYNC CFG_TBD /* TODO(TBD) frame format: sync byte value */
#endif
#ifndef CFG_PROTO_HEADER_LEN
#define CFG_PROTO_HEADER_LEN CFG_TBD /* TODO(TBD) frame format: header length in bytes */
#endif
#ifndef CFG_PROTO_LENGTH_FIELD_BYTES
#define CFG_PROTO_LENGTH_FIELD_BYTES CFG_TBD /* TODO(TBD) frame format: width of the length field */
#endif
#ifndef CFG_PROTO_CHECKSUM_KIND
#define CFG_PROTO_CHECKSUM_KIND CFG_TBD /* TODO(TBD) frame format: checksum choice (none/sum/CRC) */
#endif
#ifndef CFG_PROTO_MAX_PAYLOAD
#define CFG_PROTO_MAX_PAYLOAD CFG_TBD /* TODO(TBD) frame format: maximum payload length */
#endif
#ifndef CFG_PROTO_TYPE_SAMPLE_BATCH
#define CFG_PROTO_TYPE_SAMPLE_BATCH CFG_TBD /* TODO(TBD) frame format: frame type id of a sample batch (PICO355-R5) */
#endif
#ifndef CFG_PROTO_TYPE_COMMAND
#define CFG_PROTO_TYPE_COMMAND CFG_TBD /* TODO(TBD) frame format: frame type id of a command (PICO355-R6) */
#endif
#ifndef CFG_PROTO_TYPE_RESPONSE
#define CFG_PROTO_TYPE_RESPONSE CFG_TBD /* TODO(TBD) frame format: frame type id of a response (PICO355-R6) */
#endif

/* ---- Command set (TBD), CFG_PROTO_CMD_* / CFG_PROTO_RESP_* ------------------ */
#ifndef CFG_PROTO_CMD_SET_ODR
#define CFG_PROTO_CMD_SET_ODR CFG_TBD /* TODO(TBD) command set: code for "set output data rate" (PICO355-R2, PICO355-R6) */
#endif
#ifndef CFG_PROTO_CMD_SET_RANGE
#define CFG_PROTO_CMD_SET_RANGE CFG_TBD /* TODO(TBD) command set: code for "set measurement range" (PICO355-R2, PICO355-R6) */
#endif
#ifndef CFG_PROTO_CMD_SET_FILTER
#define CFG_PROTO_CMD_SET_FILTER CFG_TBD /* TODO(TBD) command set: code for "set filter" (PICO355-R2, PICO355-R6) */
#endif
#ifndef CFG_PROTO_RESP_ACK
#define CFG_PROTO_RESP_ACK CFG_TBD /* TODO(TBD) command set: response code acknowledge (PICO355-R6) */
#endif
#ifndef CFG_PROTO_RESP_ERROR
#define CFG_PROTO_RESP_ERROR CFG_TBD /* TODO(TBD) command set: response code error (PICO355-R6) */
#endif
#ifndef CFG_PROTO_RESP_STATUS
#define CFG_PROTO_RESP_STATUS CFG_TBD /* TODO(TBD) command set: response code status (PICO355-R6) */
#endif

/* ---- State machine (TBD), PICO355-R7 --------------------------------------- */
#ifndef CFG_FSM_INITIAL_STATE
#define CFG_FSM_INITIAL_STATE FSM_STATE_UNSET /* TODO(TBD) state transition details: initial state not decided */
#endif
#ifndef CFG_FSM_UNKNOWN_EVENT_POLICY
#define CFG_FSM_UNKNOWN_EVENT_POLICY FSM_POLICY_UNSET /* TODO(TBD) state transition details: unknown-event policy */
#endif
/* TODO(TBD) state transition table: default rows X(from, event, to), used by
 * fsm_init_default(); empty until decided. Host tests pass their own table to
 * fsm_init() instead (fsm.h). */
#define CFG_FSM_TRANSITION_TABLE(X) /* TODO(TBD) state transition table rows */

#endif /* PICO355_CONFIG_H */
