# Pico355 wire protocol, seams and open TBDs

Not SSOT. The SysMLEdge desk `sysmledgeprj-rpipico-adxl355` @ `e364458f462c7b2153b6e6acd914b187e32b97fb`
is the source of truth; this file only fixes the code seams around it. Every value
marked TBD below is a sentinel in code (`CFG_TBD` = 0, or an `*_UNSET` token) until the
owner decides it.

## Build targets

- Firmware: `firmware/CMakeLists.txt`, CMake target `pico355_fw`, output **`pico355_fw.uf2`**
  (plus `.elf`, `.bin`, `.hex`). Build: `source toolchain.env; cmake -S firmware -B <dir>; cmake --build <dir>`.
- HMI: crate `hmi/` (`pico355-hmi`), lib `pico355_hmi` plus bin `pico355-hmi`.
  Serial dependency: `serialport` 4 with `default-features = false` (no libudev; the user gives the port path).

## Link

USB CDC serial (proposed in the model, pending owner approval), one byte stream in each direction:

- Pico -> PC: sample-batch frames (sampleStream, PICO355-R5) and response frames (commandChannel, PICO355-R6).
- PC -> Pico: command frames (commandChannel, PICO355-R6).

## Frame layout (all TBD)

`firmware/src/protocol.c` is the only firmware owner of the layout; `hmi/src/protocol.rs` is its mirror.
The fields of a frame, in order, are named by these constants:

| Firmware (`config.h`) | Rust (`tbd.rs`) | Meaning | Value |
|---|---|---|---|
| `CFG_PROTO_SYNC` | `PROTO_SYNC` | sync byte at frame start | TBD |
| `CFG_PROTO_HEADER_LEN` | `PROTO_HEADER_LEN` | header length in bytes | TBD |
| `CFG_PROTO_LENGTH_FIELD_BYTES` | `PROTO_LENGTH_FIELD_BYTES` | width of the payload length field | TBD |
| `CFG_PROTO_CHECKSUM_KIND` | `PROTO_CHECKSUM_KIND` | checksum choice (none / sum / CRC) | TBD |
| `CFG_PROTO_MAX_PAYLOAD` | `PROTO_MAX_PAYLOAD` | maximum payload length | TBD |
| `CFG_PROTO_TYPE_SAMPLE_BATCH` | `PROTO_TYPE_SAMPLE_BATCH` | frame type id: sample batch | TBD |
| `CFG_PROTO_TYPE_COMMAND` | `PROTO_TYPE_COMMAND` | frame type id: command | TBD |
| `CFG_PROTO_TYPE_RESPONSE` | `PROTO_TYPE_RESPONSE` | frame type id: response | TBD |

Sample-batch payload: N samples of `x, y, z` (model AccelSample, 20-bit raw); per-sample byte
encoding and batch size TBD (frame format and FIFO watermark TBD).

## Command set (all TBD)

Command payload `{code, value}` (`protocol_command_t` / `protocol::Command`); response payload
`{code, cmd_code, value}` (`protocol_response_t` / `protocol::Response`). Byte layout TBD.

| Firmware | Rust | Meaning (model) | Value |
|---|---|---|---|
| `CFG_PROTO_CMD_SET_ODR` | `PROTO_CMD_SET_ODR` | set output data rate (PICO355-R2, R6) | TBD |
| `CFG_PROTO_CMD_SET_RANGE` | `PROTO_CMD_SET_RANGE` | set measurement range | TBD |
| `CFG_PROTO_CMD_SET_FILTER` | `PROTO_CMD_SET_FILTER` | set filter | TBD |
| `CFG_PROTO_RESP_ACK` | `PROTO_RESP_ACK` | acknowledge (ResponseMessage) | TBD |
| `CFG_PROTO_RESP_ERROR` | `PROTO_RESP_ERROR` | error | TBD |
| `CFG_PROTO_RESP_STATUS` | `PROTO_RESP_STATUS` | status | TBD |

Rust mirror rule: every `CFG_PROTO_*` macro in `firmware/include/config.h` has a `pub const`
of the same name without `CFG_` in `hmi/src/tbd.rs`, with the same value.

## Open TBD list (ten)

1. SPI clock - `CFG_SPI_CLOCK_HZ` (firmware does not initialise SPI0 while TBD).
2. SPI mode - `CFG_SPI_MODE`.
3. FIFO watermark - `CFG_FIFO_WATERMARK`.
4. Sample buffer depth and overflow policy - `CFG_SAMPLE_BUFFER_DEPTH` (placeholder 1), `CFG_SAMPLE_BUFFER_OVERFLOW_POLICY`.
5. USB frame format - `CFG_PROTO_*` table above.
6. Command set - `CFG_PROTO_CMD_*` / `CFG_PROTO_RESP_*` above.
7. State transition details - `CFG_FSM_INITIAL_STATE`, `CFG_FSM_TRANSITION_TABLE`, `CFG_FSM_UNKNOWN_EVENT_POLICY` (PICO355-R7).
8. LDO output check - TPS7A2033 output voltage not yet checked against its datasheet (PICO355-R3); hardware, no code constant.
9. Cable pinout - X5 cable assumed straight 1:1, unverified; hardware, no code constant.
10. HMI form - GUI or CLI; placeholder is a plain terminal text view (`tbd::HMI_FORM`). Related HMI TBDs: log format, serial settings, port discovery, refresh rate (`tbd.rs`).

Also open: every ADXL355 register address and bit field in `firmware/include/adxl355_regs.h`
is a TODO(TBD) sentinel, because the datasheet Rev. D could not be fetched when the skeleton was
written; fill each with a "Datasheet Rev. D, Table/Section" citation. Default range and filter
settings exposed are TBD in the model.

## Confirmed constants

- Pins (model X5Port): GP16 SPI0 RX/MISO, GP17 CSN, GP18 SCK, GP19 TX/MOSI, GP22 DRDY input (never driven); INT1/INT2 unconnected.
- Default ODR 4000 Hz (PICO355-R1): `CFG_DEFAULT_ODR_HZ`, `config::DEFAULT_ODR_HZ`.

## Signature index

Firmware (`firmware/include/`, host-compilable, no SDK headers; `main.c` binds the hal structs):

| Header | Model part | Main API |
|---|---|---|
| `config.h` | - | all `CFG_*` constants |
| `adxl355_regs.h` | - | register/bit-field sentinels |
| `adxl355.h` | spiDriver | `adxl355_bus_t{transfer}`; `adxl355_init`, `adxl355_configure`, `adxl355_set_measuring`, `adxl355_read_sample`, `adxl355_read_fifo`, `adxl355_status` -> `adxl355_rc_t` |
| `acquisition.h` | drdyAcquisition | `acq_hal_t{drdy_read}`; `acq_init`, `acq_step` -> `acq_rc_t` |
| `sample_buffer.h` | sampleBuffer | `sample_buffer_init`, `_push`, `_pop`, `_count`, `_overflows` |
| `usb_stream.h` | usbCdcStreamer | `usb_stream_hal_t{write, read}`; `usb_stream_init`, `_send_batch`, `_send_response`, `_receive` |
| `command.h` | commandHandler | `command_parse`, `command_apply` -> `command_rc_t` |
| `fsm.h` | FirmwareStates | `fsm_init`, `fsm_dispatch`, `fsm_state`; states idle/configuring/streaming/fault |
| `protocol.h` | (link) | `protocol_encode_sample_batch`, `_encode_response`, `_encode_command`, `_decode_frame`, `_decode_command` |

Each module returns its own rc enum with a `*_NOT_IMPLEMENTED` code that stubs return.

HMI (`hmi/src/`): `protocol.rs` (`encode_command`, `decode_frame`, `Sample`, `Command`, `Response`, `Frame`),
`link.rs` (`trait Transport`, `SerialTransport::open`), `config.rs` (`HmiConfig`, `Range`, `DEFAULT_ODR_HZ`),
`live_view.rs` (`render`), `logger.rs` (`Logger::new`, `log_batch`, `open_file`), `tbd.rs` (constants), `main.rs` (CLI).

## Requirement trace

| Requirement | Files |
|---|---|
| PICO355-R1 default 4000 Hz raw XYZ | `config.h` (`CFG_DEFAULT_ODR_HZ`), `hmi/src/config.rs` |
| PICO355-R2 runtime config over USB | `command.h`, `config.h`, `hmi/src/config.rs`, `hmi/src/lib.rs` |
| PICO355-R3 sensor supply (LDO output check) | open hardware TBD 8 above |
| PICO355-R4 DRDY-gated SPI0 reads | `adxl355.h`, `adxl355_regs.h`, `acquisition.h`, `config.h` (pins) |
| PICO355-R5 buffer and stream | `sample_buffer.h`, `usb_stream.h`, `protocol.h` |
| PICO355-R6 command handling | `command.h`, `protocol.h` |
| PICO355-R7 state machine | `fsm.h`, `config.h` (`CFG_FSM_*`) |
| PICO355-R8 HMI serial + config UI | `hmi/src/link.rs`, `hmi/src/config.rs`, `hmi/src/main.rs` |
| PICO355-R9 HMI live view + logging | `hmi/src/live_view.rs`, `hmi/src/logger.rs` |
