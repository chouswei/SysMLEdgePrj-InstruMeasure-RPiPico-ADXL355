# Pico355 wire protocol, seams and open TBDs

Not SSOT. The SysMLEdge desk `sysmledgeprj-rpipico-adxl355` @ `e364458f462c7b2153b6e6acd914b187e32b97fb`
is the source of truth; this file only fixes the code seams around it. Every value
marked TBD below is a "not configured" sentinel in code until the owner decides it
(rule in "TBD rule" below).

## TBD rule (one behaviour for every module)

| Kind | Firmware sentinel | Test | Rust mirror |
|---|---|---|---|
| numeric `CFG_*` still open (default filter, buffer overflow policy; SPI clock/mode and every `CFG_PROTO_*` are now settled, see "Settled link values") | `CFG_TBD` = `0xFFFFFFFFFFFFFFFFull` | `CFG_IS_TBD(x)` (C and `#if`) | `Option<T>` = `None` (`tbd.rs`, the open HMI items) |
| register address / bit field / code | `ADXL355_TBD` (same value, `adxl355_regs.h`), flag `ADXL355_REGS_ARE_TBD` | `ADXL355_IS_TBD(x)` | - |
| enum value | `ADXL355_RANGE_UNSET`, `FSM_STATE_UNSET`, `FSM_POLICY_UNSET`, `ADXL355_FILTER_UNSET` (-1) | `== *_UNSET` | `Option` (`config.rs`) |
| array size | settled: `CFG_SAMPLE_BUFFER_DEPTH` 64, `CFG_USB_FRAME_BUF_BYTES`, `CFG_USB_RX_BUF_BYTES`; flags `CFG_SAMPLE_BUFFER_DEPTH_IS_TBD` / `CFG_USB_BUF_BYTES_IS_TBD` are 0 | the flag | - |

1. No sentinel equals a legal value: every decided `CFG_*` or register value fits in 32 bits
   (8 bits for registers), the 64-bit all-ones sentinel does not; enum `*_UNSET` members are
   not states/ranges/policies; no legal filter code is negative.
2. Order in every function: argument check (`*_ERR_ARG`), then TBD check, then the work.
   A function that needs a TBD value returns its module's TBD code and has no side effect on
   hardware or output bytes (`*out_len`, `*consumed`, `*n_read`, `*pushed`, `*sent` set to 0):
   `ADXL355_ERR_TBD`, `ACQ_ERR_TBD`, `USB_STREAM_ERR_TBD`, `COMMAND_ERR_TBD`, `FSM_ERR_TBD`,
   `PROTOCOL_ERR_TBD` (these codes stay for any future TBD value; none of the settled items returns them). Rust: the `tbd.rs` items that stay open are `None`.
3. A TBD code from a lower module propagates as the caller's TBD code (adxl355 -> acquisition,
   protocol -> usb_stream / command), never as a generic error.
4. A value is used only after its TBD test is false, through an explicit cast to the field
   width. An unchecked implicit use of `CFG_TBD` in a byte/32-bit field fails `-Werror`
   (`-Woverflow` / `-Wtype-limits`) on purpose.
5. Stub bodies (A1) return `*_ERR_NOT_IMPLEMENTED`; implementations never do.
6. Test injection: each `CFG_*` value in `config.h` is wrapped in `#ifndef`, so a host test
   target may compile a module with `-DCFG_PROTO_SYNC=<v>` etc. (test data only, never a
   design value). The fsm table is injected through `fsm_init` (below). Rust tests can only
   observe the `None` behaviour of the items that stay TBD; the settled frame layout is tested
   on the real values (no `-D` injection build any more).

## ADXL355 driver contract (register map from no-OS)

`firmware/include/adxl355_regs.h` is filled from the Analog Devices no-OS driver
(`drivers/accel/adxl355/adxl355.h` and `adxl355.c`, pinned to commit
`4557b1f58f8a233bd116dbb02d8d5c447d3699b6`; owner-named source). Every value carries a
`no-OS H:<line>` / `no-OS C:<line>` citation in the header. Entries the no-OS driver does not
define stay `ADXL355_TBD`. `ADXL355_FIFO_CAPACITY` is 96 entries (32 XYZ sets), cited no-OS pin
`4557b1f58f8a233bd116dbb02d8d5c447d3699b6` `drivers/accel/adxl355/README.rst:92` "There are 96 locations in the FIFO"
(:96 default and maximum FIFO samples value 96; equals `ADXL355_FIFO_SAMPLES_MAX` 0x60). `ADXL355_REGS_ARE_TBD` is computed from the entries the driver
uses: today 0. Contract (`adxl355.h` header comment), for `adxl355_init`, `_configure`,
`_set_measuring`, `_read_sample`, `_read_fifo`, `_status`:

1. `ADXL355_ERR_ARG` for NULL pointers, `bus->transfer` NULL, range not 2G/4G/8G, `odr_hz` not one of
   4000/2000/1000/500/250/125, filter not `ADXL355_FILTER_UNSET` or 0..`ADXL355_HPF_CODE_MAX`.
2. `ADXL355_ERR_TBD` if a register entry the operation needs is TBD: zero `transfer()`
   calls, `*out`/`*status` unchanged, `*n_read` = 0. `adxl355_init` still binds `dev->bus`,
   loads `dev->config` defaults and leaves `dev->initialised` false. The check is per operation.
3. `ADXL355_ERR_NOT_INIT` if init has not succeeded; 4. `ADXL355_ERR_BUS` / `ADXL355_ERR_ID`; 5. `ADXL355_OK`.

Sequences implemented from no-OS: init = soft reset (write reset code, poll STATUS until
NVM_BUSY clears, 255 tries) then DEVID_AD, DEVID_MST, PARTID check; configure = POWER_CTL to
standby if measuring, RANGE, ODR/LPF, HPF read-modify-write, restore POWER_CTL;
set_measuring = POWER_CTL write; read_sample = three 3-byte reads (X, Y, Z), 20-bit two's
complement in bits 23..4; read_fifo = the DMA FIFO drain (section "FIFO drain"); status = STATUS read
that also counts `FIFO_OVR`. Post-reset delay: `adxl355_bus_t.delay_ms` (NULL is `ADXL355_ERR_ARG`)
is called once with `CFG_ADXL355_RESET_DELAY_MS` after NVM_BUSY clears and before the ID reads
(cited: no-OS `adxl355.c` C:297-298 `no_os_mdelay(1)`); `main.c` binds it to `sleep_ms`. Not taken
from no-OS: the shadow-register compare after reset (C:300-308; the ID check stands in). SPI command
byte = (address << 1) | read bit (no-OS C:46, C:81). SPI mode and clock come from the no-OS Pico
example (see "Settled link values"). Host tests: `test_adxl355.c` (mock register file, cited IDs),
`test_adxl355_tbd.c` (all-TBD header generated by CMake), `test_adxl355_partial.c` (one entry TBD).

## State machine table supply

`fsm_init(fsm, table, len, initial, policy)` takes the transition table (not copied; must
outlive the fsm), its length, the initial state and the unknown-event policy, so host tests
inject a table. `fsm_init_default(fsm)` passes the defaults from `config.h`
(`CFG_FSM_TRANSITION_TABLE` rows, `CFG_FSM_INITIAL_STATE`, `CFG_FSM_UNKNOWN_EVENT_POLICY`), which are
a design decision (2026-11-19, owner go t37u): initial IDLE; config command -> CONFIGURING (from IDLE,
STREAMING or FAULT); CONFIGURING + done -> STREAMING; CONFIGURING + failed -> IDLE; STREAMING + stream
error -> FAULT; an event with no row is ignored. `main.c` runs IDLE -> CONFIGURING -> STREAMING at
boot once the sensor is configured and measuring. Full rules in `fsm.h`.

## Build targets

- Firmware: `firmware/CMakeLists.txt`, CMake target `pico355_fw`, output **`pico355_fw.uf2`**
  (plus `.elf`, `.bin`, `.hex`). Build: `source toolchain.env; cmake -S firmware -B <dir>; cmake --build <dir>`.
- HMI: crate `hmi/` (`pico355-hmi`), lib `pico355_hmi` plus bin `pico355-hmi`.
  Serial dependency: `serialport` 4 with `default-features = false` (no libudev; the user gives the port path).

## Settled link values (end-to-end link job, 2026-11-19)

Label key: **cited** = Analog Devices no-OS commit `4557b1f58f8a233bd116dbb02d8d5c447d3699b6`
(the file is named); **design decision (2026-11-19, owner go t37u)** = our own choice, not a part spec;
**design choice** = needed to stream, conservative, no source gives a value.
The datasheet (analog.com) could not be fetched, so no value here is cited to it.

| Item | Value | Label / source | Constant |
|---|---|---|---|
| SPI instance and pins | SPI0; GP16 MISO, GP17 CSN, GP18 SCK, GP19 MOSI; DRDY GP22 (input only) | model X5Port (the no-OS example uses GP20 for DRDY; this project keeps the model's GP22) | `CFG_PIN_*` |
| SPI clock | 1 000 000 Hz | cited: no-OS `projects/eval-adxl355-pmdz/src/platform/pico/parameters.h` `SPI_BAUDRATE 1000000` | `CFG_SPI_CLOCK_HZ` |
| SPI mode | MODE 0 (CPOL 0, CPHA 0) | cited: no-OS `projects/eval-adxl355-pmdz/src/common/common_data.c` `NO_OS_SPI_MODE_0` | `CFG_SPI_MODE` |
| SPI bit order | MSB first | cited: same `common_data.c` `NO_OS_SPI_BIT_ORDER_MSB_FIRST` | `CFG_SPI_BIT_ORDER_MSB_FIRST` |
| Post-reset delay | 1 ms | cited: no-OS `drivers/accel/adxl355/adxl355.c` C:297-298 `no_os_mdelay(1)` | `CFG_ADXL355_RESET_DELAY_MS` |
| USB link | USB CDC serial (stdio over USB), one byte stream each way | design decision (2026-11-19, owner go t37u) | - |
| HMI serial open | `pico355-hmi --port <path>` (e.g. `COM5`), 115200 baud value (CDC ignores it), 8N1, no flow control, 100 ms read timeout, DTR asserted | design decision (2026-11-19, owner go t37u) | `config::link::SERIAL_*`, `READ_TIMEOUT_MS`, `ASSERT_DTR` |
| Frame format, layout, checksum, command set | tables below | design decision (2026-11-19, owner go t37u) | `CFG_PROTO_*` |
| Boot state machine | IDLE -> CONFIGURING -> STREAMING at power-up; table in `config.h` | design decision (2026-11-19, owner go t37u) | `CFG_FSM_*` |
| Default range | +/-2 g | design choice: `adxl355_configure()` needs a legal range to write ODR 4000 Hz; smallest range, no source gives a default. The model's default-range TBD is unchanged. | `CFG_DEFAULT_RANGE` |
| Sample buffer depth | 64 samples (768 B RAM, 16 ms at 4 kHz) | design choice: depth 1 cannot hold one FIFO burst (32 samples = 0x60 entries / 3) | `CFG_SAMPLE_BUFFER_DEPTH` |

Settled by the FIFO drain job (owner t39u, see "FIFO drain"): FIFO capacity 96 (cited), watermark, trigger, overflow count and status response.

Stay TBD on purpose (not needed to stream): LDO output check, cable pinout, CI pins,
(HMI form, log format, port discovery and refresh rate were settled later by the GUI job; see "HMI GUI and HDF5 log". The `--cli --port` text stream still prints one `x y z` raw-count line per sample.)
default filter (the high-pass is left at the sensor value), buffer overflow policy (newest sample dropped and
counted, a stream-error fault is not raised for it), HMI range/filter defaults and flags.

## Link

USB CDC serial, one byte stream in each direction:

- Pico -> PC: sample-batch frames (sampleStream, PICO355-R5) and response frames (commandChannel, PICO355-R6).
- PC -> Pico: command frames (commandChannel, PICO355-R6).

The Pico USB CDC stdio only transmits while a host has the port open with DTR set; the HMI asserts DTR
after opening. Bytes sent before the port is opened are dropped, not queued.

### Byte rate: 4 kHz raw XYZ against USB full speed

- Payload: 4000 samples/s x 3 axes x 3 B = **36 000 B/s** (288 kbit/s).
- Worst framing, one sample per frame: 4000 x (4 header + 9 payload + 2 CRC) = **60 000 B/s** (480 kbit/s).
- Designed framing, full buffer (64 samples = 576 B payload): 62.5 frames/s x 6 B overhead = 375 B/s,
  total **36 375 B/s** (291 kbit/s).
- USB full speed signalling rate: 12 Mbit/s = 1 500 000 B/s (USB 2.0 specification, general knowledge,
  not fetched in this job). Worst framing uses 60 000 / 1 500 000 = **4 %** of the raw signalling rate,
  the designed framing 2.4 %; a margin of 25x or more, so no bulk-transfer ceiling is needed to call it feasible.
- SPI side at 1 MHz: 36 000 B/s x 8 = 288 kbit/s of FIFO data, 29 % of the 1 Mbit/s clock, plus the drain
  overhead worked out in "FIFO drain bandwidth" (29.8 % data plus drain; about 36 % if one poll per sample is added, an assumption, not a bound). **Not measured on hardware**.

## FIFO drain (SPI0 DMA)

Citations are to the no-OS pin `4557b1f58f8a233bd116dbb02d8d5c447d3699b6` (`drivers/accel/adxl355/`) and to
`raspberrypi/pico-examples` `spi/spi_dma/spi_dma.c` (local copy under `ds/pico-examples/`). Anything not
cited is labelled design decision (owner t39u) and is not a part spec.

- **Capacity**: 96 entries = 32 XYZ sets (`README.rst:92`; :96 default and maximum 96; :127 the overflow
  interrupt fires on an attempt to write past the 96 locations and a STATUS read clears it).
  `ADXL355_FIFO_CAPACITY` = 96; a `FIFO_ENTRIES` reading above it is capped, so a drain is never longer than 96 entries.
- **Sequence** (`adxl355_read_fifo`): read `FIFO_ENTRIES` first (no-OS `adxl355.c:664-668`); below the
  watermark nothing else happens; otherwise whole XYZ sets only (entries rounded down to a multiple of 3, as
  `adxl355.c:714`; the tail stays in the FIFO for the next drain; at most as many sets as the caller can store).
  Then ONE CS-low window of `1 + entries x 3` bytes: the TX DMA channel repeats the `FIFO_DATA` read byte
  `(0x11 << 1) | READ_BIT` = 0x23 (`read_increment = false`, as the RX channel in `spi_dma.c:74`), the RX DMA
  channel writes every received byte into the buffer (`write_increment = true`, `spi_dma.c:75`); the first received byte is
  the dummy clocked with the command byte, the FIFO data starts at the second. Both channels are started
  together with `dma_start_channel_mask` (`spi_dma.c:85`). `main.c` `bus_fifo_read` binds this to SPI0 DMA;
  `adxl355_fifo_hal_t` is the mock point, and with no hal one `transfer()` carries the same bytes, so the host tests need no DMA.
  When `FIFO_ENTRIES` is a multiple of 3 (the watermark case) the length is exactly `FIFO_ENTRIES x 3` data bytes.
- **Marker alignment** (`adxl355_fifo_parse`; no-OS `adxl355.c:723-731`): an entry whose third byte has bit 0 (X
  marker) set and bit 1 (empty) clear starts a set (`adxl355_regs.h` `ADXL355_FIFO_X_MARKER`, `_EMPTY`, no-OS C:724-725). The next
  two entries are Y and Z and must be neither X-marked nor empty. Entries before the first X, empty entries and a
  partial set (X, Y at the end, or an X that arrives before the Z) are discarded; the count of whole sets for N aligned entries is N / 3.
  An all-empty drain yields no sample and no error. Extra rule beyond no-OS (design decision (owner t39u)): a set with a
  non-plain Y or Z entry is dropped, the scan restarts at the next entry.
- **Watermark** `CFG_FIFO_WATERMARK` = 48 entries (16 XYZ sets), written to `FIFO_SAMPLES` (0x29) by
  `adxl355_set_fifo_watermark` before measuring starts (no-OS `adxl355.c:678-692`, value above 0x60 refused): design decision (owner t39u).
  Reason: 16 sets per drain means 250 drains/s at 4 kHz, and after the watermark the FIFO still has
  (96 - 48) / 3 = 16 sets = 4 ms of room before it is full.
- **Trigger** `CFG_FIFO_TRIGGER` = poll `FIFO_ENTRIES` against the watermark, evaluated on each DRDY GP22 high: design decision (owner t39u).
  The `FIFO_ENTRIES` read is needed for the drain length anyway; the DRDY gate of PICO355-R4 stays, so there is no bus
  traffic while DRDY is low. Rejected: drain on every DRDY (one drain per sample, no batching, the per-drain overhead repeated
  4000 times a second), and drain on the `FIFO_FULL` flag. no-OS `README.rst:126` does define it: FIFO FULL fires when the FIFO entries equal the
  `FIFO_SAMPLES` setting, clears when the entries fall below that after enough FIFO data is read, and can also clear on a STATUS
  read when the entries are already below. DATA READY is cleared on a STATUS read (`README.rst:125`). The FIFO_FULL trigger is not
  excluded by missing documentation; polling `FIFO_ENTRIES` on DRDY is kept as a design decision (owner t39u). Not verified: how long DRDY stays high in FIFO mode. If it is a short pulse the loop could
  look too late; the overflow count below is how a hardware run would show that.
- **Overflow** (design decision (owner t39u)): after each drain `acq_step` reads STATUS once; a set `FIFO_OVR` (no-OS
  `adxl355_regs.h` `ADXL355_STATUS_FIFO_OVR`, H:222) adds 1 to `adxl355_t.fifo_overflows`, a 32-bit counter that saturates at
  0xFFFFFFFF instead of wrapping and is cleared by `adxl355_init`. A STATUS read clears the interrupt (`README.rst:127`),
  so it counts STATUS observations of the flag, about one per overflow event, not lost samples. A `CMD_GET_STATUS` frame (0x04) is answered
  with a `RESP_STATUS` frame (0x03) whose `value` field (u32 LE, payload offset 2) is that count; the command changes nothing else.
  The HMI mirrors the code (`CMD_GET_STATUS`) but has no flag that sends it yet.
- Host tests: `test_fifo.c` (marker alignment, partial-set discard, empty marker, overflow count, status response,
  capacity, watermark gate, drain length and TX byte at the hook, non-DMA fallback) and the `fifo_labels` check of the labels and verdict below.
  **Not run on hardware.** No Pico was attached: the DMA path in `main.c` is built, not exercised.

### FIFO drain bandwidth (SPI0 at 1 MHz, `CFG_SPI_CLOCK_HZ` unchanged)

Wire time only, 8 clocks per byte, 1 MHz = 125 000 B/s:

| Item | Bytes | Rate at 4 kHz |
|---|---|---|
| FIFO data, 4000 samples/s x 9 B | 36 000 B/s | 288 kbit/s |
| Per drain (16 sets): `FIFO_ENTRIES` read 2 B + `FIFO_DATA` window 1 + 144 B + STATUS read 2 B = 149 B, 250 drains/s | 37 250 B/s | 298 kbit/s (29.8 %) |
| Polls below the watermark, **assumption**: at most one `FIFO_ENTRIES` read (2 B) per sample time (DRDY pulses once per sample), 3750/s | 7 500 B/s | 60 kbit/s |
| Total under that assumption (not a worst case) | 44 750 B/s | 358 kbit/s (35.8 %) |

Nothing enforces one poll per sample: `main.c` spins in a tight loop and `acq_step` polls whenever DRDY is high, and STATUS is
read only after a drain (a STATUS read clears DATA READY, `README.rst:125`). If DRDY stays high between drains, the poll rate is
the loop speed and the SPI load is unbounded (near saturated while waiting). The 29.8 % (data plus drain) is the firm wire-time number.

The one CS-low drain window is 145 B x 8 = 1.16 ms. Margin on the firm number: 1 / 0.298 = about 3.4 times on wire time;
under the one-poll assumption 1 / 0.358 = about 2.8 times.

Not in the numbers (not measured, no hardware): gaps between transactions and CS timing, DMA setup per drain, main-loop and
USB service time (the drain blocks while the RX channel runs; the 4 ms of FIFO room after the watermark must cover it).

Verdict: 1 MHz fits with margin (wire time, 29.8 % data plus drain; about 36 % only if DRDY pulses once per sample, otherwise the poll load is unbounded). Hardware check still open: measure the DRDY width (GP22) and the overflow count. Not a hardware result. The clock is not raised; `CFG_SPI_CLOCK_HZ` stays the cited 1 000 000.
If a hardware run shows the overflow count rising, the first things to look at are the loop and USB time, not the SPI clock.

## Frame layout (design decision (2026-11-19, owner go t37u), not a part spec)

`firmware/src/protocol.c` is the only firmware owner of the layout; `hmi/src/protocol.rs` is its mirror.
Frame = header (sync, type, length u16 LE) + payload + CRC-16/CCITT-FALSE (u16 LE) over header + payload:

```
 offset 0     1      2..3        4 .. 4+len-1   4+len .. 5+len
        sync  type   length LE   payload        crc16 LE (over bytes 0 .. 3+len)
        0xA5  01/02/03
```

| Firmware (`config.h`) | Rust (`config::link`) | Meaning | Value |
|---|---|---|---|
| `CFG_PROTO_SYNC` | `SYNC` | sync byte at frame start | 0xA5 |
| `CFG_PROTO_HEADER_LEN` | `HEADER_LEN` | header length in bytes | 4 |
| `CFG_PROTO_LENGTH_FIELD_BYTES` | `LENGTH_FIELD_BYTES` | width of the payload length field | 2 |
| `CFG_PROTO_CHECKSUM_KIND` | `CHECKSUM_KIND` | checksum choice | 1 = CRC-16/CCITT-FALSE |
| `CFG_PROTO_CHECKSUM_NONE` | `CHECKSUM_NONE` | the kind code meaning "no checksum" | 0 |
| `CFG_PROTO_CHECKSUM_CRC16` | `CHECKSUM_CRC16` | the kind code for CRC-16/CCITT-FALSE | 1 |
| `CFG_PROTO_CHECKSUM_LEN_BYTES` | `CHECKSUM_LEN_BYTES` | checksum field width, LE | 2 |
| `CFG_PROTO_MAX_PAYLOAD` | `MAX_PAYLOAD` | maximum payload length (64 samples x 9 B) | 576 |
| `CFG_PROTO_TYPE_SAMPLE_BATCH` | `TYPE_SAMPLE_BATCH` | frame type id: sample batch | 0x01 |
| `CFG_PROTO_TYPE_COMMAND` | `TYPE_COMMAND` | frame type id: command | 0x02 |
| `CFG_PROTO_TYPE_RESPONSE` | `TYPE_RESPONSE` | frame type id: response | 0x03 |
| `CFG_PROTO_SYNC_OFFSET` | `SYNC_OFFSET` | header offset of the sync byte | 0 |
| `CFG_PROTO_TYPE_OFFSET` | `TYPE_OFFSET` | header offset of the frame type id byte | 1 |
| `CFG_PROTO_LENGTH_OFFSET` | `LENGTH_OFFSET` | header offset of the payload length field | 2 |
| `CFG_PROTO_LENGTH_MSB_FIRST` | `LENGTH_MSB_FIRST` | byte order of the length field (1 MSB first, 0 LSB first) | 0 |
| `CFG_PROTO_PAD_BYTE` | `PAD_BYTE` | value of header/payload bytes no field covers (ignored on decode) | 0x00 |
| `CFG_PROTO_CMD_PAYLOAD_LEN` | `CMD_PAYLOAD_LEN` | command payload length | 5 |
| `CFG_PROTO_CMD_CODE_OFFSET` | `CMD_CODE_OFFSET` | payload offset of the command code byte | 0 |
| `CFG_PROTO_CMD_VALUE_OFFSET` | `CMD_VALUE_OFFSET` | payload offset of the command value | 1 |
| `CFG_PROTO_CMD_VALUE_BYTES` | `CMD_VALUE_BYTES` | width of the command value | 4 |
| `CFG_PROTO_RESP_PAYLOAD_LEN` | `RESP_PAYLOAD_LEN` | response payload length | 6 |
| `CFG_PROTO_RESP_CODE_OFFSET` | `RESP_CODE_OFFSET` | payload offset of the response code byte | 0 |
| `CFG_PROTO_RESP_CMD_CODE_OFFSET` | `RESP_CMD_CODE_OFFSET` | payload offset of the answered command code byte | 1 |
| `CFG_PROTO_RESP_VALUE_OFFSET` | `RESP_VALUE_OFFSET` | payload offset of the response value | 2 |
| `CFG_PROTO_RESP_VALUE_BYTES` | `RESP_VALUE_BYTES` | width of the response value | 4 |
| `CFG_PROTO_VALUE_MSB_FIRST` | `VALUE_MSB_FIRST` | byte order of the command/response value | 0 |
| `CFG_PROTO_SAMPLE_AXIS_BYTES` | `SAMPLE_AXIS_BYTES` | bytes per axis value (two's complement, at least `ADXL355_SAMPLE_BITS` = 20 bits) | 3 |
| `CFG_PROTO_SAMPLE_MSB_FIRST` | `SAMPLE_MSB_FIRST` | byte order of each axis value | 0 |

`hmi/tests/link_config.rs` parses `config.h` and checks every row against `config::link`.

- Sample-batch payload: N samples (0 <= N <= 64) of `x, y, z` in that order, each 3 bytes two's complement LE
  (the 20-bit sensor value sign-extended to 24 bits).
- Command payload (5 B): `code` (1 B), `value` u32 LE. Response payload (6 B): `code`, `cmd_code` (the command answered), `value` u32 LE.
- Checksum: CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no final xor; check value 0x29B1 over
  `123456789`), little-endian after the payload, over header + payload. `CFG_PROTO_CHECKSUM_NONE` stays a legal
  kind for tests; any other kind is `PROTOCOL_ERR_NOT_IMPLEMENTED`.
- Decoder rules (firmware `protocol_decode_frame`, Rust `decode_frame`, same order): sync, length above
  `CFG_PROTO_MAX_PAYLOAD`, unknown type, then wait for the whole frame (`Short`/`PROTOCOL_ERR_SHORT`, nothing consumed),
  then CRC. A bad sync drops up to the next candidate sync byte; length, type and CRC errors drop 1 byte so the
  receiver rescans. `main.c` and `pico355-hmi` run that loop over an accumulating buffer.
- A value or sample axis that does not fit its width returns `PROTOCOL_ERR_TOO_LONG` / `ProtocolError::TooLong`.

## Command set (design decision (2026-11-19, owner go t37u), not a part spec)

| Firmware | Rust | Meaning (model) | Value | `value` field |
|---|---|---|---|---|
| `CFG_PROTO_CMD_SET_ODR` | `CMD_SET_ODR` | set output data rate (PICO355-R2, R6) | 0x01 | Hz: 4000, 2000, 1000, 500, 250 or 125 (the driver's cited ODR codes) |
| `CFG_PROTO_CMD_SET_RANGE` | `CMD_SET_RANGE` | set measurement range | 0x02 | `adxl355_range_t`: 1 = 2 g, 2 = 4 g, 3 = 8 g |
| `CFG_PROTO_CMD_SET_FILTER` | `CMD_SET_FILTER` | set filter | 0x03 | high-pass corner code 0..6 (no-OS `adxl355_hpf_corner`) |
| `CFG_PROTO_RESP_ACK` | `RESP_ACK` | acknowledge (ResponseMessage) | 0x01 | 0 |
| `CFG_PROTO_RESP_ERROR` | `RESP_ERROR` | error | 0x02 | `command_rc_t` of the refusal |
| `CFG_PROTO_RESP_STATUS` | `RESP_STATUS` | status: answer to `CMD_GET_STATUS` (design decision (owner t39u)) | 0x03 | FIFO overflow count: u32, little-endian, saturates at 0xFFFFFFFF (field `value`, payload offset 2); `cmd_code` = 0x04 |
| `CFG_PROTO_CMD_GET_STATUS` | `CMD_GET_STATUS` | get status (design decision (owner t39u)) | 0x04 | ignored (send 0); no configuration change, no state change, streaming continues |

The HMI sends `CMD_SET_ODR` (default 4000 Hz, `--odr <hz>`) after opening the port; the firmware answers with an
ACK frame, which the HMI prints as `response: ...`. Range and filter commands exist in the set but have no HMI
flag yet.

Rust mirror rule: every `CFG_PROTO_*` macro in `firmware/include/config.h` has a `pub const` of the same name without
`CFG_PROTO_` in `hmi/src/config.rs` module `link`, with the same numeric value (checked by `hmi/tests/link_config.rs`).

## Golden frames

The same literal bytes are in `firmware/tests/test_protocol.c` and `hmi/tests/golden.rs` (lines starting
`GOLDEN <name>:`); each side encodes to them and decodes them.

| Name | Meaning | Bytes |
|---|---|---|
| BATCH | samples (1,-2,3), (-524288,524287,0) | `a5 01 12 00 01 00 00 fe ff ff 03 00 00 00 00 f8 ff ff 07 00 00 00 b1 b3` |
| CMD_ODR | set ODR 4000 | `a5 02 05 00 01 a0 0f 00 00 e7 ed` |
| CMD_RANGE | set range 8 g (value 3) | `a5 02 05 00 02 03 00 00 00 ae 5e` |
| CMD_FILTER | set filter code 6 | `a5 02 05 00 03 06 00 00 00 ba 48` |
| RESP_ACK | ACK of set-ODR, value 0 | `a5 03 06 00 01 01 00 00 00 00 0e bc` |
| RESP_ERROR | ERROR for set-range, value 1 | `a5 03 06 00 02 02 01 00 00 00 88 ea` |
| UNKNOWN_TYPE | type 0x7f, 1 payload byte (must be rejected) | `a5 7f 01 00 00 94 b9` |

Proof level: host encode/decode round trip on both sides. No Pico was attached; flashing and `pico355-hmi --port COM5`
on Windows are the owner's step and have not been run.

## HMI GUI and HDF5 log

Requirements PICO355-R8 and PICO355-R9 (HMI). Design decisions are labelled `(owner t43s2)`; the scale is cited
(see below). The GUI is the default `pico355-hmi` mode; `--cli --port <path>` keeps the headless text stream.

### GUI form

- Native window, eframe 0.37 with the glow backend (OpenGL), charts with egui_plot 0.38. Default mode: `pico355-hmi`
  opens the window; `--port <path>` preselects the port in the list.
- `pico355-hmi --cli --port <path>`: headless stream, same flow as before (no window). `--cli` without `--port` is a
  usage error.
- Live refresh: repaint every 25 ms (`REPAINT_INTERVAL_MS`) and a `CMD_GET_STATUS` poll every 1 s
  (`POLL_INTERVAL_MS`) (both design decisions (owner t43s2)).
- Chart windows 2 / 5 / 10 s (`WINDOW_CHOICES_S`) (design decision (owner t43s2)).
- Three stacked charts, one per axis, with vertical axis labels `X (g)`, `Y (g)`, `Z (g)` (`axis_label`). Charts never
  show raw counts; the envelope values used for plotting are g (`SampleStore::envelope`, `min_g` / `max_g`).

### Port discovery

- The list comes from serialport `available_ports()` (`worker::list_ports`). A Pico is flagged by USB VID `0x2E8A`
  and PID `0x000A` (`PICO_VID`, `PICO_PID`), as stated in the owner's ask (design decision (owner t43s2)).
- A custom path may be typed in addition to the list. Refresh rescans the list.
- Native file picker: not taken; the log path is an editable text field. **TBD** (native file picker).

### Connect and controls

- Connect opens the port with the `link` settings (8N1, no flow control, DTR asserted, read timeout 100 ms). No
  filter write happens on connect (design decision (owner t43s2)).
- On connect the GUI sends the chosen range (`CMD_SET_RANGE`) and ODR (`CMD_SET_ODR`). The g scale follows the
  range the device ACKed, not the range the user selected; a range that was never ACKed does not change the scale.
- ODR, range and the high-pass Apply control are locked while logging, so the file attributes stay true (design
  decision (owner t43s2)). Port selection is not locked by logging.
- The last ACK or ERR is shown next to the control that sent it. Status (FIFO overflow count, measured rate,
  sample count) comes from `CMD_GET_STATUS` (~1 Hz) and manual refresh; the measured rate and sample count are
  host-side values.

### Scale to g (cited)

- g per LSB = `3.9e-6 x {1, 2, 4}` for range code `{1, 2, 3}` (2 g, 4 g, 8 g): `3.9e-6`, `7.8e-6`, `15.6e-6` g/LSB.
  Cited: no-OS commit `4557b1f58f8a233bd116dbb02d8d5c447d3699b6`, `drivers/accel/adxl355/adxl355.h:62-65`
  (3.9 ug/LSB at 2 g, multiplier 2 for 4 g and 4 for 8 g), `:169-174` (range enum 1 / 2 / 3), `adxl355.c:18`
  (scale multiplier table {0, 1, 2, 4}), `adxl355.c:920-930` (20-bit sign extension, then the scale).
- `scale_lsb_per_g` (LSB per g) is the reciprocal of g per LSB: DERIVED (1 / 3.9e-6 = 256410.26...), not a datasheet
  or no-OS value. Any other scale value stays TBD.
- Code: `scale::G_PER_LSB_2G`, `g_per_lsb`, `lsb_per_g` (derived), `raw_to_g`, `sample_to_g`. An unknown range code
  returns `None`.

### Threading

- Reader thread: owns the serial port; reads frames, decodes samples and ACK / ERR / status responses.
- Command channel: GUI to worker (`WorkerCommand`). Event channel: worker to GUI (`WorkerEvent`, drained each frame).
- Writer thread: owned by `h5log::H5Logger`; it receives the samples and the settings from the GUI, writes in
  chunks, and is joined on stop. The GUI never blocks on the file.

### HDF5 schema (`h5log`)

The file is written with hdf5-pure 0.48 (pure Rust, no C library). h5py reads it.

| Object | Type and shape | Meaning |
|---|---|---|
| `accel_g/x_g`, `accel_g/y_g`, `accel_g/z_g` | f64, rank 1, unlimited (maxshape), chunk 4096 | acceleration in g |
| `raw_counts/x`, `raw_counts/y`, `raw_counts/z` | i32, rank 1, unlimited, chunk 4096 | 20-bit signed raw counts (sign extended) |
| `sample_index` | u64, rank 1, unlimited, chunk 4096 | push index of each sample |

Root attributes: `odr_hz` (Hz), `range_g` (2, 4, 8), `scale_g_per_lsb` (cited), `scale_lsb_per_g` (DERIVED),
`hpf_code` (i32, -1 = high-pass not applied by this HMI), `fifo_overflow_count` (last status), `units` = `g`,
`sensor` = `ADXL355`, `started_utc` (`YYYY-MM-DDTHH:MM:SSZ`).

- Why one 1-D dataset per axis: hdf5-pure appends only work on rank-1 datasets (PREFLIGHT.md section 3), so a 2-D
  `N x 3` array is not used.
- Default file name: `pico355_YYYYMMDD_HHMMSS.h5` in UTC (`default_log_filename`) (design decision (owner t43s2)).
- Verification: `hmi/tests/h5log.rs` writes a file and, when the venv python exists, runs `tools/checks/h5verify.py`
  (h5py). Path of the venv used for the check is recorded in the run's done row.

### Build and imports

- Windows build: `export PATH=/home/box/toolchains/venv/bin:$PATH; cd hmi && CARGO_TARGET_DIR=<dir> cargo zigbuild --release --target x86_64-pc-windows-gnu`.
- Expected imports: Windows system DLLs only (no libstdc++, libgcc, libwinpthread, GTK or WebView2). Checked with
  `objdump -p <exe> | grep 'DLL Name'`. The list from the preflight (advapi32, the api-ms-win-core and UCRT
  api-ms-win-crt sets, bcryptprimitives, cfgmgr32, dwmapi, gdi32, imm32, kernel32, ntdll, ole32, opengl32, setupapi,
  shell32, user32, userenv, uxtheme, ws2_32) is the expectation, not the final build; the exe sha256 is recorded by the
  combine step, not here.
- The glow backend needs an OpenGL 2+ driver on the PC. `opengl32.dll` is a system DLL; a real GPU driver run is not
  verified here. **TBD** (real GPU / OpenGL driver check on the PC).

### TBD in this section

- Native file picker (editable path only) - **TBD**.
- Log rotation - **TBD**.
- LSB per g beyond the derived reciprocal - **TBD** (no other source).
- `hmi/src/tbd.rs` is NOT edited by this section: it keeps the old terminal placeholders (`HMI_FORM`, `LOG_FORMAT`,
  `PORT_DISCOVERY`, `LIVE_VIEW_REFRESH_HZ`) for the `--cli` path.

## Open TBD list

Settled by the end-to-end link job (see "Settled link values"): SPI clock and mode, post-reset delay, USB frame
format and layout, command set, serial settings, boot state machine table, default range (design choice),
sample buffer depth (design choice). Still TBD:

1. (settled by the FIFO drain job, owner t39u) FIFO watermark - `CFG_FIFO_WATERMARK`; see "FIFO drain". Still TBD there: the hardware proof (below).
2. Default filter (settings exposed) - `CFG_DEFAULT_FILTER`, HMI `filter`.
3. Buffer overflow policy - `CFG_SAMPLE_BUFFER_OVERFLOW_POLICY` (behaviour while TBD: the newest sample is dropped and counted).
4. LDO output check - TPS7A2033 output voltage not yet checked against its datasheet (PICO355-R3); hardware, no code constant.
5. Cable pinout - X5 cable assumed straight 1:1, unverified; hardware, no code constant.
6. HMI form, log format, port discovery and refresh rate: settled for the GUI, see "HMI GUI and HDF5 log" (`tbd.rs` keeps the old placeholders for `--cli`); still TBD there: native file picker, log rotation, real GPU/OpenGL run. Model default range for the HMI (`config.rs` `range`).
7. CI pins. (`ADXL355_FIFO_CAPACITY` is settled: 96, cited.)

## Confirmed constants

- Pins (model X5Port): GP16 SPI0 RX/MISO, GP17 CSN, GP18 SCK, GP19 TX/MOSI, GP22 DRDY input (never driven); INT1/INT2 unconnected.
- Default ODR 4000 Hz (PICO355-R1): `CFG_DEFAULT_ODR_HZ`, `config::DEFAULT_ODR_HZ`.
- SPI0 1 MHz MODE_0 MSB first and the 1 ms post-reset delay: cited to the no-OS pin (see "Settled link values").

## Signature index

Firmware (`firmware/include/`, host-compilable, no SDK headers; `main.c` binds the hal structs):

| Header | Model part | Main API |
|---|---|---|
| `config.h` | - | all `CFG_*` constants |
| `adxl355_regs.h` | - | register/bit-field sentinels |
| `adxl355.h` | spiDriver | `adxl355_bus_t{transfer, ctx, delay_ms}`; `adxl355_init(dev, bus)`, `adxl355_configure(dev, cfg)`, `adxl355_set_measuring(dev, on)`, `adxl355_read_sample(dev, out)`, `adxl355_read_fifo(dev, out, max, n_read)`, `adxl355_fifo_parse(raw, n_entries, out, max, n_out)`, `adxl355_set_fifo_hal(dev, hal)` (`adxl355_fifo_hal_t{read, ctx}`), `adxl355_set_fifo_watermark(dev, entries)`, `adxl355_status(dev, status)` (counts `FIFO_OVR` in `dev->fifo_overflows`) -> `adxl355_rc_t` (`_ERR_TBD`, `_ERR_NOT_INIT`) |
| `acquisition.h` | drdyAcquisition | `acq_hal_t{drdy_read}`; `acq_init(acq, hal, dev, buf)`, `acq_step(acq, pushed)` -> `acq_rc_t` (`ACQ_ERR_TBD`) |
| `sample_buffer.h` | sampleBuffer | `sample_buffer_init(buf)`, `_push(buf, s)`, `_pop(buf, out)`, `_count(buf)`, `_overflows(buf)` |
| `usb_stream.h` | usbCdcStreamer | `usb_stream_hal_t{write, read}`; `usb_stream_init(us, hal, frame_buf, cap)`, `_send_batch(us, buf, max, sent)`, `_send_response(us, resp)`, `_receive(us, buf, cap, n)` (`USB_STREAM_ERR_TBD`) |
| `command.h` | commandHandler | `command_parse(payload, len, cmd)`, `command_apply(ctx, cmd, resp)` -> `command_rc_t` (`COMMAND_ERR_TBD`) |
| `fsm.h` | FirmwareStates | `fsm_init(fsm, table, len, initial, policy)`, `fsm_init_default(fsm)`, `fsm_dispatch(fsm, ev, next)`, `fsm_state(fsm)`; states idle/configuring/streaming/fault |
| `protocol.h` | (link) | `protocol_encode_sample_batch(samples, count, out, cap, out_len)`, `_encode_response(resp, ...)`, `_encode_command(cmd, ...)`, `_decode_frame(in, len, frame, consumed)`, `_decode_command(payload, len, cmd)`, `_decode_response`, `_decode_sample_batch`, `protocol_crc16` |

Each module returns its own rc enum with a `*_NOT_IMPLEMENTED` code that stubs return.

HMI (`hmi/src/`): `protocol.rs` (`encode_command`, `encode_response`, `encode_sample_batch`, `decode_frame`, `crc16`, `Sample`, `Command`, `Response`, `Frame`),
`link.rs` (`trait Transport`, `SerialTransport::open`), `config.rs` (`HmiConfig`, `Range`, `DEFAULT_ODR_HZ` = `tbd::DEFAULT_ODR_HZ`, module `link` = settled wire/serial constants),
`live_view.rs` (`render`), `logger.rs` (`Logger::new`, `log_batch`, `open_file`), `tbd.rs` (constants), `main.rs` (CLI).
GUI job modules (see "HMI GUI and HDF5 log"): `gui.rs` (eframe window, `run`), `worker.rs` (serial reader thread, command and event channels, `list_ports`),
`samples.rs` (`SampleStore`, plot window in g), `scale.rs` (cited counts to g), `h5log.rs` (HDF5 writer thread); `lib.rs` lists them. `tbd.rs` is unchanged by this job.

## Requirement trace

| Requirement | Files |
|---|---|
| PICO355-R1 default 4000 Hz raw XYZ | `config.h` (`CFG_DEFAULT_ODR_HZ`), `hmi/src/config.rs` |
| PICO355-R2 runtime config over USB | `command.h`, `config.h`, `hmi/src/config.rs`, `hmi/src/lib.rs` |
| PICO355-R3 sensor supply (LDO output check) | open TBD 4 above |
| PICO355-R4 DRDY-gated SPI0 reads | `adxl355.h`, `adxl355_regs.h`, `acquisition.h`, `config.h` (pins) |
| PICO355-R5 buffer and stream | `sample_buffer.h`, `usb_stream.h`, `protocol.h` |
| PICO355-R6 command handling | `command.h`, `protocol.h` |
| PICO355-R7 state machine | `fsm.h`, `config.h` (`CFG_FSM_*`) |
| PICO355-R8 HMI serial + config UI | `hmi/src/link.rs`, `hmi/src/config.rs`, `hmi/src/main.rs` |
| PICO355-R9 HMI live view + logging | `hmi/src/live_view.rs`, `hmi/src/logger.rs` |
