/*
 * config.h - the ONLY place for firmware constants (confirmed and TBD).
 *
 * Requirements: PICO355-R1 (default ODR 4000 Hz), PICO355-R2 (runtime config),
 * PICO355-R4 (pins, SPI0, DRDY), PICO355-R5 (buffer, frame format),
 * PICO355-R6 (command set), PICO355-R7 (state machine).
 * Model pin: desk sysmledgeprj-rpipico-adxl355 @ e364458f462c7b2153b6e6acd914b187e32b97fb.
 *
 * Rules: every macro is prefixed CFG_. A value that is not in the model files,
 * AGENTS.md or the ADXL355 datasheet Rev. D carries TODO(TBD) and a sentinel
 * (CFG_TBD = 0, or an UNSET token), never an invented number.
 * The CFG_PROTO_* names are mirrored 1:1 in hmi/src/tbd.rs (without CFG_) and
 * described in docs/protocol.md.
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
#define CFG_TBD 0u /* TODO(TBD) sentinel value: "not decided", never a design value */

/* ---- Pins (confirmed, model X5Port / RaspberryPiPico) ---------------------- */
#define CFG_PIN_SPI0_RX_MISO 16 /* model X5Port pin 3: SPI0_RX GP16 <- MISO (PICO355-R4) */
#define CFG_PIN_SPI0_CSN 17 /* model X5Port pin 1: SPI0_CSN GP17 -> CS (PICO355-R4) */
#define CFG_PIN_SPI0_SCK 18 /* model X5Port pin 4: SPI0_SCK GP18 -> SCLK (PICO355-R4) */
#define CFG_PIN_SPI0_TX_MOSI 19 /* model X5Port pin 2: SPI0_TX GP19 -> MOSI (PICO355-R4) */
#define CFG_PIN_DRDY_IN 22 /* model X5Port pin 5: GP22 <- DRDY, input only, never driven; INT1/INT2 unconnected (PICO355-R4) */
#define CFG_SPI_INSTANCE_INDEX 0 /* model SPI0 bus (PICO355-R4) */

/* ---- SPI (TBD) ----------------------------------------------------------- */
#define CFG_SPI_CLOCK_HZ CFG_TBD /* TODO(TBD) SPI clock: not decided (model SpiBusPort) */
#define CFG_SPI_MODE CFG_SPI_MODE_UNSET /* TODO(TBD) SPI mode (CPOL/CPHA): not decided (model SpiBusPort) */
#define CFG_SPI_MODE_UNSET 0xFFu /* TODO(TBD) SPI mode sentinel: not a valid mode 0..3 */

/* ---- Sensor defaults ------------------------------------------------------- */
#define CFG_DEFAULT_ODR_HZ 4000u /* default output data rate, PICO355-R1 (defaultStreamRate 4000 Hz) */
#define CFG_DEFAULT_RANGE ADXL355_RANGE_UNSET /* TODO(TBD) default range: model AccelRange says default TBD */
#define CFG_DEFAULT_FILTER CFG_TBD /* TODO(TBD) filter settings exposed: not decided (PICO355-R2) */
#define CFG_FIFO_WATERMARK CFG_TBD /* TODO(TBD) FIFO watermark: not decided (PICO355-R4) */

/* ---- Sample buffer (TBD) ----------------------------------------------- */
#define CFG_SAMPLE_BUFFER_DEPTH 1u /* TODO(TBD) buffer depth: 1 is the smallest compilable placeholder, not a design value (PICO355-R5) */
#define CFG_SAMPLE_BUFFER_OVERFLOW_POLICY CFG_TBD /* TODO(TBD) buffer depth overflow policy: drop-oldest or drop-newest not decided (PICO355-R5) */

/* ---- USB frame format (TBD), CFG_PROTO_* ----------------------------------- */
#define CFG_PROTO_SYNC CFG_TBD /* TODO(TBD) frame format: sync byte value */
#define CFG_PROTO_HEADER_LEN CFG_TBD /* TODO(TBD) frame format: header length in bytes */
#define CFG_PROTO_LENGTH_FIELD_BYTES CFG_TBD /* TODO(TBD) frame format: width of the length field */
#define CFG_PROTO_CHECKSUM_KIND CFG_TBD /* TODO(TBD) frame format: checksum choice (none/sum/CRC) */
#define CFG_PROTO_MAX_PAYLOAD CFG_TBD /* TODO(TBD) frame format: maximum payload length */
#define CFG_PROTO_TYPE_SAMPLE_BATCH CFG_TBD /* TODO(TBD) frame format: frame type id of a sample batch (PICO355-R5) */
#define CFG_PROTO_TYPE_COMMAND CFG_TBD /* TODO(TBD) frame format: frame type id of a command (PICO355-R6) */
#define CFG_PROTO_TYPE_RESPONSE CFG_TBD /* TODO(TBD) frame format: frame type id of a response (PICO355-R6) */

/* ---- Command set (TBD), CFG_PROTO_CMD_* / CFG_PROTO_RESP_* ------------------ */
#define CFG_PROTO_CMD_SET_ODR CFG_TBD /* TODO(TBD) command set: code for "set output data rate" (PICO355-R2, PICO355-R6) */
#define CFG_PROTO_CMD_SET_RANGE CFG_TBD /* TODO(TBD) command set: code for "set measurement range" (PICO355-R2, PICO355-R6) */
#define CFG_PROTO_CMD_SET_FILTER CFG_TBD /* TODO(TBD) command set: code for "set filter" (PICO355-R2, PICO355-R6) */
#define CFG_PROTO_RESP_ACK CFG_TBD /* TODO(TBD) command set: response code acknowledge (PICO355-R6) */
#define CFG_PROTO_RESP_ERROR CFG_TBD /* TODO(TBD) command set: response code error (PICO355-R6) */
#define CFG_PROTO_RESP_STATUS CFG_TBD /* TODO(TBD) command set: response code status (PICO355-R6) */

/* ---- State machine (TBD), PICO355-R7 --------------------------------------- */
#define CFG_FSM_INITIAL_STATE FSM_STATE_UNSET /* TODO(TBD) state transition details: initial state not decided */
#define CFG_FSM_UNKNOWN_EVENT_POLICY FSM_POLICY_UNSET /* TODO(TBD) state transition details: unknown-event policy */
/* TODO(TBD) state transition table: rows X(from, event, to); empty until decided. */
#define CFG_FSM_TRANSITION_TABLE(X) /* TODO(TBD) state transition table rows */

#endif /* PICO355_CONFIG_H */
