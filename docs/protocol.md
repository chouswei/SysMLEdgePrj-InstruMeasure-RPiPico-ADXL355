# Pico355 wire protocol, seams and open TBDs

Not SSOT. The SysMLEdge desk `sysmledgeprj-rpipico-adxl355` @ `e364458f462c7b2153b6e6acd914b187e32b97fb`
is the source of truth; this file only fixes the code seams around it. Every value
marked TBD below is a "not configured" sentinel in code until the owner decides it
(rule in "TBD rule" below).

## TBD rule (one behaviour for every module)

| Kind | Firmware sentinel | Test | Rust mirror |
|---|---|---|---|
| numeric `CFG_*` (SPI clock/mode, FIFO watermark, default filter, overflow policy, every `CFG_PROTO_*`) | `CFG_TBD` = `0xFFFFFFFFFFFFFFFFull` | `CFG_IS_TBD(x)` (C and `#if`) | `Option<T>` = `None` (`tbd.rs`) |
| register address / bit field / code | `ADXL355_TBD` (same value, `adxl355_regs.h`), flag `ADXL355_REGS_ARE_TBD` | `ADXL355_IS_TBD(x)` | - |
| enum value | `ADXL355_RANGE_UNSET`, `FSM_STATE_UNSET`, `FSM_POLICY_UNSET`, `ADXL355_FILTER_UNSET` (-1) | `== *_UNSET` | `Option` (`config.rs`) |
| array size | placeholder `CFG_SAMPLE_BUFFER_DEPTH` 1 + flag `CFG_SAMPLE_BUFFER_DEPTH_IS_TBD` 1 | the flag | - |

1. No sentinel equals a legal value: every decided `CFG_*` or register value fits in 32 bits
   (8 bits for registers), the 64-bit all-ones sentinel does not; enum `*_UNSET` members are
   not states/ranges/policies; no legal filter code is negative.
2. Order in every function: argument check (`*_ERR_ARG`), then TBD check, then the work.
   A function that needs a TBD value returns its module's TBD code and has no side effect on
   hardware or output bytes (`*out_len`, `*consumed`, `*n_read`, `*pushed`, `*sent` set to 0):
   `ADXL355_ERR_TBD`, `ACQ_ERR_TBD`, `USB_STREAM_ERR_TBD`, `COMMAND_ERR_TBD`, `FSM_ERR_TBD`,
   `PROTOCOL_ERR_TBD`; Rust `ProtocolError::Tbd`, `LinkError::Tbd`.
3. A TBD code from a lower module propagates as the caller's TBD code (adxl355 -> acquisition,
   protocol -> usb_stream / command), never as a generic error.
4. A value is used only after its TBD test is false, through an explicit cast to the field
   width. An unchecked implicit use of `CFG_TBD` in a byte/32-bit field fails `-Werror`
   (`-Woverflow` / `-Wtype-limits`) on purpose.
5. Stub bodies (A1) return `*_ERR_NOT_IMPLEMENTED`; implementations never do.
6. Test injection: each `CFG_*` value in `config.h` is wrapped in `#ifndef`, so a host test
   target may compile a module with `-DCFG_PROTO_SYNC=<v>` etc. (test data only, never a
   design value). The fsm table is injected through `fsm_init` (below). Rust tests can only
   observe the `None` behaviour until values are decided.

## ADXL355 driver contract while registers are TBD

`firmware/include/adxl355_regs.h` is entirely TODO(TBD): the datasheet Rev. D could not be
downloaded on the build box (analog.com timed out, retried in the B1 rework). Contract
(`adxl355.h` header comment), for `adxl355_init`, `_configure`, `_set_measuring`,
`_read_sample`, `_read_fifo`, `_status`:

1. `ADXL355_ERR_ARG` for NULL pointers, `bus->transfer` NULL, range not 2G/4G/8G, `odr_hz` 0.
2. `ADXL355_ERR_TBD` if a needed register entry or `CFG_*` value is TBD: zero `transfer()`
   calls, `*out`/`*status` unchanged, `*n_read` = 0. `adxl355_init` still binds `dev->bus`,
   loads `dev->config` defaults (`CFG_DEFAULT_ODR_HZ`, `CFG_DEFAULT_RANGE`, filter
   `ADXL355_FILTER_UNSET`) and leaves `dev->initialised` false.
3. `ADXL355_ERR_NOT_INIT` if init has not succeeded; 4. `ADXL355_ERR_BUS` / `ADXL355_ERR_ID`; 5. `ADXL355_OK`.

Today every valid call returns `ADXL355_ERR_TBD` with zero bus calls. FA implements exactly
that (checking entries with `ADXL355_IS_TBD`, never filling an address); the PF host test
checks it with a mock bus that counts `transfer()` calls, plus the `ADXL355_ERR_ARG` cases.

## State machine table supply

`fsm_init(fsm, table, len, initial, policy)` takes the transition table (not copied; must
outlive the fsm), its length, the initial state and the unknown-event policy, so host tests
inject a table. `fsm_init_default(fsm)` passes the TODO(TBD) defaults from `config.h`
(`CFG_FSM_TRANSITION_TABLE` rows, `CFG_FSM_INITIAL_STATE`, `CFG_FSM_UNKNOWN_EVENT_POLICY`);
`main.c` calls it, and it returns `FSM_ERR_TBD` while they are unset. Full rules in `fsm.h`.

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
of the same name without `CFG_` in `hmi/src/tbd.rs`, typed `Option<T>`: `None` while the
macro is `CFG_TBD`, `Some(v)` with the same value once decided.

## Open TBD list (ten)

1. SPI clock - `CFG_SPI_CLOCK_HZ` (firmware does not initialise SPI0 while TBD).
2. SPI mode - `CFG_SPI_MODE`.
3. FIFO watermark - `CFG_FIFO_WATERMARK`.
4. Sample buffer depth and overflow policy - `CFG_SAMPLE_BUFFER_DEPTH` (placeholder 1, flag `CFG_SAMPLE_BUFFER_DEPTH_IS_TBD`; full after one push), `CFG_SAMPLE_BUFFER_OVERFLOW_POLICY` (while TBD a push on full stores nothing, counts an overflow, returns `SAMPLE_BUFFER_OVERFLOW`).
5. USB frame format - `CFG_PROTO_*` table above.
6. Command set - `CFG_PROTO_CMD_*` / `CFG_PROTO_RESP_*` above.
7. State transition details - `CFG_FSM_INITIAL_STATE`, `CFG_FSM_TRANSITION_TABLE`, `CFG_FSM_UNKNOWN_EVENT_POLICY` (PICO355-R7).
8. LDO output check - TPS7A2033 output voltage not yet checked against its datasheet (PICO355-R3); hardware, no code constant.
9. Cable pinout - X5 cable assumed straight 1:1, unverified; hardware, no code constant.
10. HMI form - GUI or CLI; placeholder is a plain terminal text view (`tbd::HMI_FORM`). Related HMI TBDs: log format, serial settings, port discovery, refresh rate (`tbd.rs`).

Also open: every ADXL355 register address and bit field in `firmware/include/adxl355_regs.h`
is a TODO(TBD) sentinel, because the datasheet Rev. D could not be fetched on the build box;
fill each with a "Datasheet Rev. D, Table/Section" citation (driver contract above). Default range and filter
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
| `adxl355.h` | spiDriver | `adxl355_bus_t{transfer}`; `adxl355_init(dev, bus)`, `adxl355_configure(dev, cfg)`, `adxl355_set_measuring(dev, on)`, `adxl355_read_sample(dev, out)`, `adxl355_read_fifo(dev, out, max, n_read)`, `adxl355_status(dev, status)` -> `adxl355_rc_t` (`_ERR_TBD`, `_ERR_NOT_INIT`) |
| `acquisition.h` | drdyAcquisition | `acq_hal_t{drdy_read}`; `acq_init(acq, hal, dev, buf)`, `acq_step(acq, pushed)` -> `acq_rc_t` (`ACQ_ERR_TBD`) |
| `sample_buffer.h` | sampleBuffer | `sample_buffer_init(buf)`, `_push(buf, s)`, `_pop(buf, out)`, `_count(buf)`, `_overflows(buf)` |
| `usb_stream.h` | usbCdcStreamer | `usb_stream_hal_t{write, read}`; `usb_stream_init(us, hal, frame_buf, cap)`, `_send_batch(us, buf, max, sent)`, `_send_response(us, resp)`, `_receive(us, buf, cap, n)` (`USB_STREAM_ERR_TBD`) |
| `command.h` | commandHandler | `command_parse(payload, len, cmd)`, `command_apply(ctx, cmd, resp)` -> `command_rc_t` (`COMMAND_ERR_TBD`) |
| `fsm.h` | FirmwareStates | `fsm_init(fsm, table, len, initial, policy)`, `fsm_init_default(fsm)`, `fsm_dispatch(fsm, ev, next)`, `fsm_state(fsm)`; states idle/configuring/streaming/fault |
| `protocol.h` | (link) | `protocol_encode_sample_batch(samples, count, out, cap, out_len)`, `_encode_response(resp, ...)`, `_encode_command(cmd, ...)`, `_decode_frame(in, len, frame, consumed)`, `_decode_command(payload, len, cmd)` |

Each module returns its own rc enum with a `*_NOT_IMPLEMENTED` code that stubs return.

HMI (`hmi/src/`): `protocol.rs` (`encode_command`, `decode_frame`, `Sample`, `Command`, `Response`, `Frame`),
`link.rs` (`trait Transport`, `SerialTransport::open`), `config.rs` (`HmiConfig`, `Range`, `DEFAULT_ODR_HZ` = `tbd::DEFAULT_ODR_HZ`),
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
