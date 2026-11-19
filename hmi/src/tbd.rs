//! TBD constants of the HMI and the Rust mirror of every firmware `CFG_PROTO_*`
//! name (same name without `CFG_`), see firmware/include/config.h and docs/protocol.md.
//!
//! Requirements: PICO355-R5, PICO355-R6 (wire protocol), PICO355-R8, PICO355-R9 (HMI).
//! Every value here is a sentinel until the owner decides it; never a design value.

/// Sentinel: "not decided". TODO(TBD).
pub const TBD: u8 = 0;

// ---- USB frame format (mirrors CFG_PROTO_*) ----
pub const PROTO_SYNC: u8 = TBD; // TODO(TBD) frame format: sync byte value
pub const PROTO_HEADER_LEN: usize = 0; // TODO(TBD) frame format: header length in bytes
pub const PROTO_LENGTH_FIELD_BYTES: usize = 0; // TODO(TBD) frame format: width of the length field
pub const PROTO_CHECKSUM_KIND: u8 = TBD; // TODO(TBD) frame format: checksum choice
pub const PROTO_MAX_PAYLOAD: usize = 0; // TODO(TBD) frame format: maximum payload length
pub const PROTO_TYPE_SAMPLE_BATCH: u8 = TBD; // TODO(TBD) frame format: sample-batch frame type id
pub const PROTO_TYPE_COMMAND: u8 = TBD; // TODO(TBD) frame format: command frame type id
pub const PROTO_TYPE_RESPONSE: u8 = TBD; // TODO(TBD) frame format: response frame type id

// ---- Command set (mirrors CFG_PROTO_CMD_* / CFG_PROTO_RESP_*) ----
pub const PROTO_CMD_SET_ODR: u8 = TBD; // TODO(TBD) command set: set output data rate
pub const PROTO_CMD_SET_RANGE: u8 = TBD; // TODO(TBD) command set: set measurement range
pub const PROTO_CMD_SET_FILTER: u8 = TBD; // TODO(TBD) command set: set filter
pub const PROTO_RESP_ACK: u8 = TBD; // TODO(TBD) command set: response acknowledge
pub const PROTO_RESP_ERROR: u8 = TBD; // TODO(TBD) command set: response error
pub const PROTO_RESP_STATUS: u8 = TBD; // TODO(TBD) command set: response status

// ---- HMI TBDs ----
/// TODO(TBD) HMI form (GUI or CLI): placeholder default is a plain terminal text view.
pub const HMI_FORM: &str = "terminal-text"; // TODO(TBD) HMI form placeholder
/// TODO(TBD) log format and storage: placeholder name only, no layout decided.
pub const LOG_FORMAT: &str = "tbd"; // TODO(TBD) log format
/// TODO(TBD) serial settings: baud value for the CDC port (CDC may ignore it); 0 = not decided.
pub const SERIAL_BAUD: u32 = 0; // TODO(TBD) serial settings
/// TODO(TBD) port discovery: none; the user passes the port path.
pub const PORT_DISCOVERY: bool = false; // TODO(TBD) port discovery
/// TODO(TBD) live view refresh rate (model LiveDataView); 0 = not decided.
pub const LIVE_VIEW_REFRESH_HZ: u32 = 0; // TODO(TBD) display form and refresh rate
