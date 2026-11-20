# SysMLEdgePrj-RPiPico-ADXL355
Raspberry Pi Pico + ADXL355B streaming raw XYZ to a PC over USB (SysMLEdge desk SSOT; sysml-models/ is backup)

Project title: "RPi Pico - ADXL2358" (typo kept as the title; the sensor is the ADXL355B, not the ADXL358).

## Source of truth

- The SysMLEdge desk `sysmledgeprj-rpipico-adxl355` is the SSOT. Propose changes on the desk; the owner's Save updates the backup.
- `sysml-models/` is a backup export, not the SSOT. `docs/` is not SSOT either.

## Lane

The owner merges every PR.

## Build, flash and run

Status: the firmware builds and the host tests pass (host encode/decode round trip with identical golden bytes in firmware and HMI tests). SPI0 settings and the post-reset delay are cited to the Analog Devices no-OS pin; the USB CDC link, frame layout, command set and boot state table are design decisions (2026-11-19, owner go t37u), all in `docs/protocol.md` "Settled link values". **Not yet run on hardware:** flash `pico355_fw.uf2`, then run `pico355-hmi --port COM5` on Windows (or the port path on Linux); that step is the owner's.

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

The PC HMI is in `hmi/` (binary `pico355-hmi`) and talks to the Pico over USB CDC. The settings for the port are in `hmi/src/config.rs` `link` (8N1, DTR asserted).

Default: a native desktop window (eframe with egui_plot; `hmi/src/gui.rs`):

```sh
cargo run --manifest-path hmi/Cargo.toml -- [--port <path>]
```

`--port <path>` preselects the port. The window has:

- Stacked live X, Y and Z charts. The vertical axis is in g, labelled `X (g)`, `Y (g)` and `Z (g)`. Each value is computed from the 20-bit signed raw counts with the no-OS sensitivity for the active range: 3.9e-6 g per LSB at 2 g, multiplied by 2 at 4 g and by 4 at 8 g (`hmi/src/scale.rs`; cited to `adxl355.h:62-65` and `adxl355.c:920-930` of the no-OS pin `4557b1f58f8a233bd116dbb02d8d5c447d3699b6`). The LSB-per-g value is a derived reciprocal.
- A COM port dropdown with refresh, and a custom path field. The Pico (USB VID 2E8A, PID 000A) is highlighted.
- Controls for ODR (4000, 2000, 1000, 500, 250 and 125 Hz), range (2, 4 and 8 g) and high-pass (codes 0 to 6), each with an explicit Apply. The last ACK or ERR from the firmware is shown.
- Status: measured rate, sample count and FIFO overflow count, from `CMD_GET_STATUS` polled about once per second (and on manual refresh).
- Start / Stop HDF5 logging. The log is a `.h5` file named `pico355_YYYYMMDD_HHMMSS.h5` (UTC time), written from a background thread with the pure-Rust `hdf5-pure` library. It is readable with h5py. Its groups are `accel_g` (`x_g`, `y_g`, `z_g`), `raw_counts` (`x`, `y`, `z`) and `sample_index`; the scale, range, ODR, high-pass code, overflow count and start time are root attributes (`hmi/src/h5log.rs`).

Headless stream (no window), the earlier text flow kept unchanged:

```sh
cargo run --manifest-path hmi/Cargo.toml -- --cli --port <path> [--odr <hz>]
```

It opens the port, sends set-ODR, prints one `x y z` raw-count line per decoded sample and `response: ...` for the firmware's ACK. `--odr` applies only with `--cli`; the default comes from `CFG_DEFAULT_ODR_HZ` (4000 Hz, PICO355-R1). `--cli` without `--port` is a usage error.

`--help` prints the usage.

Not yet run: the native window on a real GPU/OpenGL driver, and the Windows and hardware runs. Those are the owner's and stay unverified here.

Design decisions (owner t43s2): window layout, refresh and poll intervals, and the port dropdown. Still TBD: a native file picker (the log path is an editable text field), log rotation, and a real-hardware GPU/OpenGL check.

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

Each item is a single-place placeholder with a `TODO(TBD)` comment. Do not fill one in without a source (model files, AGENTS.md, the ADXL355 datasheet Rev. D, or for ADXL355 registers the no-OS driver at the commit named in `adxl355_regs.h`).

- ADXL355 entries the no-OS driver does not define (`ADXL355_FIFO_CAPACITY`): `firmware/include/adxl355_regs.h`, `docs/protocol.md`
- FIFO watermark: `firmware/include/config.h`
- Sensor supply, LDO output check (PICO355-R3): `firmware/include/config.h`
- Buffer overflow policy: `firmware/include/config.h`
- Cable pinout (X5 assumed straight 1:1): `firmware/include/config.h`
- Default filter, and the model/HMI default range (the firmware streams at a design-choice 2 g): `firmware/include/config.h`
- HMI form beyond the plain text view, log format, port discovery, live view refresh: `hmi/src/tbd.rs`, `docs/protocol.md`

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
| PICO355-R8 HMI serial + config UI | `hmi/src/link.rs`, `hmi/src/config.rs`, `hmi/src/worker.rs`, `hmi/src/main.rs` |
| PICO355-R9 HMI live view + logging | `hmi/src/live_view.rs`, `hmi/src/logger.rs`, `hmi/src/gui.rs`, `hmi/src/h5log.rs`, `hmi/src/main.rs` |
