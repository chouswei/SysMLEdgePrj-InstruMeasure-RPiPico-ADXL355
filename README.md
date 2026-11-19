# SysMLEdgePrj-RPiPico-ADXL355
Raspberry Pi Pico + ADXL355B streaming raw XYZ to a PC over USB (SysMLEdge desk SSOT; sysml-models/ is backup)

Project title: "RPi Pico - ADXL2358" (typo kept as the title; the sensor is the ADXL355B, not the ADXL358).

## Source of truth

- The SysMLEdge desk `sysmledgeprj-rpipico-adxl355` is the SSOT. Propose changes on the desk; the owner's Save updates the backup.
- `sysml-models/` is a backup export, not the SSOT. `docs/` is not SSOT either.

## Lane

The owner merges every PR.

## Build, flash and run

Status: the firmware builds and the host tests pass, but the firmware cannot read the ADXL355B yet. Its register map is TODO(TBD) (`firmware/include/adxl355_regs.h`), so no sensor data flows until the TBD list below is decided.

### Firmware build (Pico SDK, CMake)

Prerequisites: a CMake toolchain and the Raspberry Pi Pico SDK (`PICO_SDK_PATH` set). The firmware is C and targets the RP2040.

```sh
cmake -S firmware -B build-fw
cmake --build build-fw
```

The output is `pico355_fw.uf2` (CMake target `pico355_fw`, see `firmware/CMakeLists.txt`).

### Flashing the UF2

1. Hold BOOTSEL on the Pico while you connect it over USB. It mounts as a mass-storage drive.
2. Copy `pico355_fw.uf2` onto that drive. The board reboots into the firmware.

### Running the HMI (PC, Rust)

The PC HMI is in `hmi/` and talks to the Pico over USB CDC. Port discovery is TODO(TBD), so pass the port path yourself:

```sh
cargo run --manifest-path hmi/Cargo.toml -- --port <path> [--odr <hz>]
```

`--help` prints the usage. `--odr` is optional; the default comes from `CFG_DEFAULT_ODR_HZ` (4000 Hz, PICO355-R1).

Until the serial settings are decided (`SERIAL_BAUD` in `hmi/src/tbd.rs` is TODO(TBD)), the binary opens no port: it exits with a TBD link error. Only `--help` and the tests are useful today.

### Check scripts

Run from the repository root. Each script exits non-zero on failure.

| Script | What it checks |
|---|---|
| `tools/checks/firmware-crossbuild.sh` | Pico SDK cross build of `firmware/`; asserts a `.uf2` is produced |
| `tools/checks/host-tests.sh` | Builds and runs the firmware host tests in `firmware/tests/` (gcc, ctest) |
| `tools/checks/firmware-lint.sh` | Static rules for `firmware/` (SDK includes, cited register defines, DRDY never driven, forbidden names) |
| `tools/checks/hmi.sh` | `cargo build` and `cargo test` for `hmi/` |
| `tools/checks/readme.sh` | This README: required sections, trace table, linked paths, forbidden names |
| `tools/checks/ci.sh` | CI workflow contract (the workflow file under .github) |

### Open TBD list

Each item is a single-place placeholder with a `TODO(TBD)` comment. Do not fill one in without a source (model files, AGENTS.md or the ADXL355 datasheet Rev. D).

- ADXL355 register addresses, masks and bit fields: `firmware/include/adxl355_regs.h` (datasheet not reachable from the build box)
- SPI clock, SPI mode and FIFO watermark: `firmware/include/config.h`
- Sensor supply, LDO output check (PICO355-R3): `firmware/include/config.h`
- Sample buffer depth and overflow policy: `firmware/include/config.h`
- USB frame format (sync, header length, length field width, checksum kind, maximum payload, frame type ids): `firmware/include/config.h`, `hmi/src/tbd.rs`, `docs/protocol.md`
- USB frame layout (`CFG_PROTO_*` offsets, widths, byte order, pad byte, checksum-none code, command/response payload layout, sample encoding width and byte order; table in `docs/protocol.md` "Frame layout"): `firmware/include/config.h`, `hmi/src/tbd.rs`
- USB scratch buffer sizes (`CFG_USB_FRAME_BUF_BYTES`, `CFG_USB_RX_BUF_BYTES`, placeholder 64): `firmware/include/config.h`
- Command set and response codes: `firmware/include/config.h`, `hmi/src/tbd.rs`, `docs/protocol.md`
- State transition details: `firmware/include/config.h`
- Cable pinout (X5 assumed straight 1:1): `firmware/include/config.h`
- Default range and default filter: `firmware/include/config.h`
- HMI form, log format, serial settings, port discovery, live view refresh: `hmi/src/tbd.rs`, `docs/protocol.md`

### Requirement trace

Requirements are the `PICO355-R` tags in the model files. The files below are the ones that carry each tag in this tree.

| Requirement | Files |
|---|---|
| PICO355-R1 default 4000 Hz raw XYZ | `firmware/include/config.h`, `firmware/include/adxl355.h`, `hmi/src/config.rs`, `hmi/src/tbd.rs` |
| PICO355-R2 runtime config over USB | `firmware/include/command.h`, `firmware/src/command.c`, `firmware/include/config.h`, `hmi/src/config.rs`, `hmi/src/lib.rs` |
| PICO355-R3 sensor supply (LDO output check) | `firmware/include/config.h` (TBD, not yet checked) |
| PICO355-R4 DRDY-gated SPI0 reads | `firmware/include/adxl355.h`, `firmware/include/adxl355_regs.h`, `firmware/include/acquisition.h`, `firmware/src/acquisition.c`, `firmware/src/adxl355.c`, `firmware/include/config.h` (pins) |
| PICO355-R5 buffer and stream | `firmware/include/sample_buffer.h`, `firmware/src/sample_buffer.c`, `firmware/include/usb_stream.h`, `firmware/src/usb_stream.c`, `firmware/include/protocol.h`, `firmware/src/protocol.c` |
| PICO355-R6 command handling | `firmware/include/command.h`, `firmware/src/command.c`, `firmware/include/protocol.h`, `firmware/src/protocol.c`, `hmi/src/protocol.rs` |
| PICO355-R7 state machine | `firmware/include/fsm.h`, `firmware/src/fsm.c`, `firmware/include/config.h` |
| PICO355-R8 HMI serial + config UI | `hmi/src/link.rs`, `hmi/src/config.rs`, `hmi/src/main.rs` |
| PICO355-R9 HMI live view + logging | `hmi/src/live_view.rs`, `hmi/src/logger.rs`, `hmi/src/main.rs` |
