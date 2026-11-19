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
 * never an invented number. Every settled value below carries one label:
 *   "cited"   = Analog Devices no-OS pin 4557b1f58f8a233bd116dbb02d8d5c447d3699b6 (file named on the line);
 *   "design decision (2026-11-19, owner go t37u)" = our own choice (link, protocol, boot state), NOT a part spec;
 *   "design choice" = a value needed to stream, conservative, no source gives one.
 * The CFG_PROTO_* names are mirrored 1:1 in hmi/src/config.rs `link` (without CFG_;
 * hmi/tests/link_config.rs parses this file and compares) and described in docs/protocol.md.
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

/* ---- SPI (cited: no-OS pin 4557b1f58f8a233bd116dbb02d8d5c447d3699b6, projects/eval-adxl355-pmdz Pico example) ---- */
#ifndef CFG_SPI_CLOCK_HZ
#define CFG_SPI_CLOCK_HZ 1000000u /* cited: no-OS pin projects/eval-adxl355-pmdz/src/platform/pico/parameters.h SPI_BAUDRATE 1000000; common_data.c max_speed_hz = SPI_BAUDRATE */
#endif
#ifndef CFG_SPI_MODE
#define CFG_SPI_MODE 0u /* cited: no-OS pin projects/eval-adxl355-pmdz/src/common/common_data.c .mode = NO_OS_SPI_MODE_0 (CPOL 0, CPHA 0) */
#endif

#ifndef CFG_SPI_BIT_ORDER_MSB_FIRST
#define CFG_SPI_BIT_ORDER_MSB_FIRST 1 /* cited: no-OS pin projects/eval-adxl355-pmdz/src/common/common_data.c .bit_order = NO_OS_SPI_BIT_ORDER_MSB_FIRST */
#endif
#ifndef CFG_ADXL355_RESET_DELAY_MS
#define CFG_ADXL355_RESET_DELAY_MS 1u /* cited: no-OS pin drivers/accel/adxl355/adxl355.c C:297-298 no_os_mdelay(1) after soft reset and NVM_BUSY poll */
#endif

/* ---- Sensor defaults ------------------------------------------------------- */
#ifndef CFG_DEFAULT_ODR_HZ
#define CFG_DEFAULT_ODR_HZ 4000u /* default output data rate, PICO355-R1 (defaultStreamRate 4000 Hz) */
#endif
#ifndef CFG_DEFAULT_RANGE
#define CFG_DEFAULT_RANGE ADXL355_RANGE_2G /* design choice (2026-11-19): needed to stream, adxl355_configure() writes ODR only with a legal range; smallest range chosen, no source gives a default; the HMI/model default range stays TBD */
#endif
#ifndef CFG_DEFAULT_FILTER
#define CFG_DEFAULT_FILTER CFG_TBD /* TODO(TBD) filter settings exposed: default filter code not decided (PICO355-R2); streaming does not need it, the HPF is left at the sensor value (ADXL355_FILTER_UNSET) */
#endif
#ifndef CFG_FIFO_WATERMARK
#define CFG_FIFO_WATERMARK CFG_TBD /* TODO(TBD) FIFO watermark: not decided (PICO355-R4); streaming does not need it, the driver reads whatever FIFO_ENTRIES reports when DRDY is high */
#endif

/* ---- Sample buffer and USB scratch sizes (design choice, needed to stream) ---- */
#ifndef CFG_SAMPLE_BUFFER_DEPTH
#define CFG_SAMPLE_BUFFER_DEPTH 64u /* design choice (2026-11-19): needed to stream (depth 1 cannot hold one FIFO burst); 64 samples x 12 B = 768 B RAM, 16 ms at 4 kHz, covers one full FIFO read (32 samples, 0x60 entries / 3) */
#endif
#ifndef CFG_SAMPLE_BUFFER_DEPTH_IS_TBD
#define CFG_SAMPLE_BUFFER_DEPTH_IS_TBD 0 /* design choice flag: depth is decided (CFG_SAMPLE_BUFFER_DEPTH) */
#endif
#ifndef CFG_USB_FRAME_BUF_BYTES
#define CFG_USB_FRAME_BUF_BYTES (CFG_PROTO_HEADER_LEN + CFG_PROTO_MAX_PAYLOAD + CFG_PROTO_CHECKSUM_LEN_BYTES) /* design decision (2026-11-19, owner go t37u): one whole frame, header + max payload + checksum */
#endif
#ifndef CFG_USB_RX_BUF_BYTES
#define CFG_USB_RX_BUF_BYTES 64u /* design decision (2026-11-19, owner go t37u): receive accumulator in main.c; holds several 11-byte command frames, resync drops garbage */
#endif
#ifndef CFG_USB_BUF_BYTES_IS_TBD
#define CFG_USB_BUF_BYTES_IS_TBD 0 /* design decision flag: scratch sizes are decided */
#endif
#ifndef CFG_SAMPLE_BUFFER_OVERFLOW_POLICY
#define CFG_SAMPLE_BUFFER_OVERFLOW_POLICY CFG_TBD /* TODO(TBD) buffer depth overflow policy: drop-oldest or drop-newest not decided; behaviour while TBD in sample_buffer.h (PICO355-R5) */
#endif

/* ---- USB frame format, CFG_PROTO_* (all: design decision (2026-11-19, owner go t37u), not a part spec) ----
 * Frame = header (sync, type, length u16 LE) + payload + CRC-16/CCITT-FALSE (u16 LE) over header+payload. */
#ifndef CFG_PROTO_CHECKSUM_CRC16
#define CFG_PROTO_CHECKSUM_CRC16 1u /* design decision (2026-11-19, owner go t37u): CFG_PROTO_CHECKSUM_KIND code for CRC-16/CCITT-FALSE */
#endif
#ifndef CFG_PROTO_CHECKSUM_LEN_BYTES
#define CFG_PROTO_CHECKSUM_LEN_BYTES 2u /* design decision (2026-11-19, owner go t37u): CRC-16 field width, little-endian, right after the payload */
#endif
#ifndef CFG_PROTO_SYNC
#define CFG_PROTO_SYNC 0xA5u /* design decision (2026-11-19, owner go t37u): sync byte value */
#endif
#ifndef CFG_PROTO_HEADER_LEN
#define CFG_PROTO_HEADER_LEN 4u /* design decision (2026-11-19, owner go t37u): header length in bytes: sync, type, length x2 */
#endif
#ifndef CFG_PROTO_LENGTH_FIELD_BYTES
#define CFG_PROTO_LENGTH_FIELD_BYTES 2u /* design decision (2026-11-19, owner go t37u): width of the payload length field */
#endif
#ifndef CFG_PROTO_CHECKSUM_KIND
#define CFG_PROTO_CHECKSUM_KIND CFG_PROTO_CHECKSUM_CRC16 /* design decision (2026-11-19, owner go t37u): checksum choice: CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no final xor), over header and payload */
#endif
#ifndef CFG_PROTO_MAX_PAYLOAD
#define CFG_PROTO_MAX_PAYLOAD 576u /* design decision (2026-11-19, owner go t37u): maximum payload length: 64 samples x 9 B, one full sample buffer per frame */
#endif
#ifndef CFG_PROTO_TYPE_SAMPLE_BATCH
#define CFG_PROTO_TYPE_SAMPLE_BATCH 0x01u /* design decision (2026-11-19, owner go t37u): frame type id of a sample batch (PICO355-R5) */
#endif
#ifndef CFG_PROTO_TYPE_COMMAND
#define CFG_PROTO_TYPE_COMMAND 0x02u /* design decision (2026-11-19, owner go t37u): frame type id of a command (PICO355-R6) */
#endif
#ifndef CFG_PROTO_TYPE_RESPONSE
#define CFG_PROTO_TYPE_RESPONSE 0x03u /* design decision (2026-11-19, owner go t37u): frame type id of a response (PICO355-R6) */
#endif

/* ---- USB frame layout, CFG_PROTO_* -----------------------------------
 * Where each field sits and how it is encoded. protocol.c reads only these names
 * (docs/protocol.md "Frame layout"). Offsets are byte offsets from the start of
 * the header (header fields) or of the payload (payload fields). A *_MSB_FIRST
 * flag is 1 for most significant byte first (big-endian), 0 for least
 * significant byte first (little-endian). Field widths are 1..4 bytes. */
#ifndef CFG_PROTO_SYNC_OFFSET
#define CFG_PROTO_SYNC_OFFSET 0u /* design decision (2026-11-19, owner go t37u): header offset of the sync byte */
#endif
#ifndef CFG_PROTO_TYPE_OFFSET
#define CFG_PROTO_TYPE_OFFSET 1u /* design decision (2026-11-19, owner go t37u): header offset of the frame type id byte */
#endif
#ifndef CFG_PROTO_LENGTH_OFFSET
#define CFG_PROTO_LENGTH_OFFSET 2u /* design decision (2026-11-19, owner go t37u): header offset of the payload length field */
#endif
#ifndef CFG_PROTO_LENGTH_MSB_FIRST
#define CFG_PROTO_LENGTH_MSB_FIRST 0u /* design decision (2026-11-19, owner go t37u): byte order of the length field: little-endian */
#endif
#ifndef CFG_PROTO_PAD_BYTE
#define CFG_PROTO_PAD_BYTE 0x00u /* design decision (2026-11-19, owner go t37u): value written to header/payload bytes no field covers */
#endif
#ifndef CFG_PROTO_CHECKSUM_NONE
#define CFG_PROTO_CHECKSUM_NONE 0u /* design decision (2026-11-19, owner go t37u): the CFG_PROTO_CHECKSUM_KIND code that means "no checksum" */
#endif
#ifndef CFG_PROTO_CMD_PAYLOAD_LEN
#define CFG_PROTO_CMD_PAYLOAD_LEN 5u /* design decision (2026-11-19, owner go t37u): command payload length in bytes: code + u32 value (PICO355-R6) */
#endif
#ifndef CFG_PROTO_CMD_CODE_OFFSET
#define CFG_PROTO_CMD_CODE_OFFSET 0u /* design decision (2026-11-19, owner go t37u): payload offset of the command code byte */
#endif
#ifndef CFG_PROTO_CMD_VALUE_OFFSET
#define CFG_PROTO_CMD_VALUE_OFFSET 1u /* design decision (2026-11-19, owner go t37u): payload offset of the command value */
#endif
#ifndef CFG_PROTO_CMD_VALUE_BYTES
#define CFG_PROTO_CMD_VALUE_BYTES 4u /* design decision (2026-11-19, owner go t37u): width of the command value */
#endif
#ifndef CFG_PROTO_RESP_PAYLOAD_LEN
#define CFG_PROTO_RESP_PAYLOAD_LEN 6u /* design decision (2026-11-19, owner go t37u): response payload length in bytes: code, answered code, u32 value (PICO355-R6) */
#endif
#ifndef CFG_PROTO_RESP_CODE_OFFSET
#define CFG_PROTO_RESP_CODE_OFFSET 0u /* design decision (2026-11-19, owner go t37u): payload offset of the response code byte */
#endif
#ifndef CFG_PROTO_RESP_CMD_CODE_OFFSET
#define CFG_PROTO_RESP_CMD_CODE_OFFSET 1u /* design decision (2026-11-19, owner go t37u): payload offset of the answered command code byte */
#endif
#ifndef CFG_PROTO_RESP_VALUE_OFFSET
#define CFG_PROTO_RESP_VALUE_OFFSET 2u /* design decision (2026-11-19, owner go t37u): payload offset of the response value */
#endif
#ifndef CFG_PROTO_RESP_VALUE_BYTES
#define CFG_PROTO_RESP_VALUE_BYTES 4u /* design decision (2026-11-19, owner go t37u): width of the response value */
#endif
#ifndef CFG_PROTO_VALUE_MSB_FIRST
#define CFG_PROTO_VALUE_MSB_FIRST 0u /* design decision (2026-11-19, owner go t37u): byte order of the command/response value: little-endian */
#endif
#ifndef CFG_PROTO_SAMPLE_AXIS_BYTES
#define CFG_PROTO_SAMPLE_AXIS_BYTES 3u /* design decision (2026-11-19, owner go t37u): bytes per axis value, two's complement, holds ADXL355_SAMPLE_BITS = 20 bits (PICO355-R5) */
#endif
#ifndef CFG_PROTO_SAMPLE_MSB_FIRST
#define CFG_PROTO_SAMPLE_MSB_FIRST 0u /* design decision (2026-11-19, owner go t37u): byte order of each axis value: little-endian */
#endif

/* ---- Command set, CFG_PROTO_CMD_* / CFG_PROTO_RESP_* ------------------ */
#ifndef CFG_PROTO_CMD_SET_ODR
#define CFG_PROTO_CMD_SET_ODR 0x01u /* design decision (2026-11-19, owner go t37u): code for "set output data rate", value = Hz (PICO355-R2, PICO355-R6) */
#endif
#ifndef CFG_PROTO_CMD_SET_RANGE
#define CFG_PROTO_CMD_SET_RANGE 0x02u /* design decision (2026-11-19, owner go t37u): code for "set measurement range", value = adxl355_range_t (1 = 2 g, 2 = 4 g, 3 = 8 g) (PICO355-R2, PICO355-R6) */
#endif
#ifndef CFG_PROTO_CMD_SET_FILTER
#define CFG_PROTO_CMD_SET_FILTER 0x03u /* design decision (2026-11-19, owner go t37u): code for "set filter", value = high-pass corner code 0..6 (PICO355-R2, PICO355-R6) */
#endif
#ifndef CFG_PROTO_RESP_ACK
#define CFG_PROTO_RESP_ACK 0x01u /* design decision (2026-11-19, owner go t37u): response code acknowledge, value 0 (PICO355-R6) */
#endif
#ifndef CFG_PROTO_RESP_ERROR
#define CFG_PROTO_RESP_ERROR 0x02u /* design decision (2026-11-19, owner go t37u): response code error, value = command_rc_t of the refusal (PICO355-R6) */
#endif
#ifndef CFG_PROTO_RESP_STATUS
#define CFG_PROTO_RESP_STATUS 0x03u /* design decision (2026-11-19, owner go t37u): response code status (reserved, not sent by this firmware yet) (PICO355-R6) */
#endif

/* ---- State machine, PICO355-R7 (design decision (2026-11-19, owner go t37u): boot must reach STREAMING) ---- */
#ifndef CFG_FSM_INITIAL_STATE
#define CFG_FSM_INITIAL_STATE FSM_STATE_IDLE /* design decision (2026-11-19, owner go t37u): power-up state; main.c then runs IDLE -> CONFIGURING -> STREAMING once the sensor is configured */
#endif
#ifndef CFG_FSM_UNKNOWN_EVENT_POLICY
#define CFG_FSM_UNKNOWN_EVENT_POLICY FSM_POLICY_IGNORE /* design decision (2026-11-19, owner go t37u): an event with no row leaves the state unchanged */
#endif
/* State transition table, design decision (2026-11-19, owner go t37u): rows X(from, event, to) used by
 * fsm_init_default(). A configuration command (boot or from the PC) moves to CONFIGURING; a finished
 * configuration streams; a failed one returns to IDLE; a stream error faults; a command leaves FAULT. */
#define CFG_FSM_TRANSITION_TABLE(X) \
    X(FSM_STATE_IDLE, FSM_EV_CONFIG_COMMAND, FSM_STATE_CONFIGURING) \
    X(FSM_STATE_STREAMING, FSM_EV_CONFIG_COMMAND, FSM_STATE_CONFIGURING) \
    X(FSM_STATE_FAULT, FSM_EV_CONFIG_COMMAND, FSM_STATE_CONFIGURING) \
    X(FSM_STATE_CONFIGURING, FSM_EV_CONFIG_DONE, FSM_STATE_STREAMING) \
    X(FSM_STATE_CONFIGURING, FSM_EV_CONFIG_FAILED, FSM_STATE_IDLE) \
    X(FSM_STATE_STREAMING, FSM_EV_STREAM_ERROR, FSM_STATE_FAULT)

#endif /* PICO355_CONFIG_H */
