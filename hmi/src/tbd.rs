//! TBD constants of the HMI and the Rust mirror of every firmware `CFG_PROTO_*`
//! name (same name without `CFG_`), see firmware/include/config.h and docs/protocol.md.
//!
//! Requirements: PICO355-R5, PICO355-R6 (wire protocol), PICO355-R8, PICO355-R9 (HMI).
//!
//! TBD rule (mirror of config.h `CFG_TBD` / `CFG_IS_TBD`): every not-yet-decided
//! protocol value is `None`; a decided value becomes `Some(v)` with the same value
//! as the firmware macro. `None` cannot collide with any legal byte or length.
//! Code that needs a `None` value returns its `Tbd` error (`ProtocolError::Tbd`,
//! `LinkError::Tbd`) before reading or writing any byte. Never a design value here.

/// Default output data rate in Hz: confirmed, PICO355-R1 (model defaultStreamRate 4000 Hz).
pub const DEFAULT_ODR_HZ: u32 = 4000;

// ---- USB frame format (mirrors CFG_PROTO_*) ----
pub const PROTO_SYNC: Option<u8> = None; // TODO(TBD) frame format: sync byte value
pub const PROTO_HEADER_LEN: Option<usize> = None; // TODO(TBD) frame format: header length in bytes
pub const PROTO_LENGTH_FIELD_BYTES: Option<usize> = None; // TODO(TBD) frame format: width of the length field
pub const PROTO_CHECKSUM_KIND: Option<u8> = None; // TODO(TBD) frame format: checksum choice
pub const PROTO_MAX_PAYLOAD: Option<usize> = None; // TODO(TBD) frame format: maximum payload length
pub const PROTO_TYPE_SAMPLE_BATCH: Option<u8> = None; // TODO(TBD) frame format: sample-batch frame type id
pub const PROTO_TYPE_COMMAND: Option<u8> = None; // TODO(TBD) frame format: command frame type id
pub const PROTO_TYPE_RESPONSE: Option<u8> = None; // TODO(TBD) frame format: response frame type id

// ---- Command set (mirrors CFG_PROTO_CMD_* / CFG_PROTO_RESP_*) ----
pub const PROTO_CMD_SET_ODR: Option<u8> = None; // TODO(TBD) command set: set output data rate
pub const PROTO_CMD_SET_RANGE: Option<u8> = None; // TODO(TBD) command set: set measurement range
pub const PROTO_CMD_SET_FILTER: Option<u8> = None; // TODO(TBD) command set: set filter
pub const PROTO_RESP_ACK: Option<u8> = None; // TODO(TBD) command set: response acknowledge
pub const PROTO_RESP_ERROR: Option<u8> = None; // TODO(TBD) command set: response error
pub const PROTO_RESP_STATUS: Option<u8> = None; // TODO(TBD) command set: response status

// ---- HMI TBDs ----
/// TODO(TBD) HMI form (GUI or CLI): placeholder default is a plain terminal text view.
pub const HMI_FORM: &str = "terminal-text"; // TODO(TBD) HMI form placeholder
/// TODO(TBD) log format and storage: placeholder name only, no layout decided (`None`).
pub const LOG_FORMAT: Option<&str> = None; // TODO(TBD) log format
/// TODO(TBD) serial settings: baud value for the CDC port (CDC may ignore it); `None` = not decided.
pub const SERIAL_BAUD: Option<u32> = None; // TODO(TBD) serial settings
/// TODO(TBD) port discovery: none; the user passes the port path.
pub const PORT_DISCOVERY: bool = false; // TODO(TBD) port discovery
/// TODO(TBD) live view refresh rate (model LiveDataView); `None` = not decided.
pub const LIVE_VIEW_REFRESH_HZ: Option<u32> = None; // TODO(TBD) display form and refresh rate
