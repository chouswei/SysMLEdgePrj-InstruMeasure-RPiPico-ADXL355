//! Wire codec on the PC side (PICO355-R5, PICO355-R6): encode commands, decode
//! sample-batch and response frames. Layout constants come only from `tbd.rs`
//! (mirror of firmware `CFG_PROTO_*`); see docs/protocol.md.
//!
//! TBD rule: while a `tbd::PROTO_*` value a function needs is `None`, it returns
//! `Err(ProtocolError::Tbd)` and produces or consumes no bytes. `NotImplemented`
//! is returned only by the A1 stub bodies.

#[allow(unused_imports)]
use crate::tbd;

/// One raw X, Y, Z sample (model item AccelSample), sign-extended 20-bit.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Sample {
    pub x: i32,
    pub y: i32,
    pub z: i32,
}

/// Command payload (model ConfigCommand); `code` is a `tbd::PROTO_CMD_*`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Command {
    pub code: u8,
    /// Value encoding: TODO(TBD) command set.
    pub value: u32,
}

/// Response payload (model ResponseMessage); `code` is a `tbd::PROTO_RESP_*`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Response {
    pub code: u8,
    pub cmd_code: u8,
    pub value: u32,
}

/// A decoded frame from the Pico.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Frame {
    SampleBatch(Vec<Sample>),
    Response(Response),
}

/// Codec errors.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ProtocolError {
    /// Not enough bytes for a whole frame yet.
    Short,
    /// No sync found; drop this many bytes.
    Sync(usize),
    Checksum,
    TooLong,
    UnknownType(u8),
    /// A `tbd::PROTO_*` value is still TODO(TBD).
    Tbd,
    NotImplemented,
}

impl std::fmt::Display for ProtocolError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "protocol error: {self:?}")
    }
}

impl std::error::Error for ProtocolError {}

/// Unwrap a `tbd::PROTO_*` value, or report the TBD error (no bytes touched).
fn decided<T>(value: Option<T>) -> Result<T, ProtocolError> {
    value.ok_or(ProtocolError::Tbd)
}

/// Frame layout values every encode/decode path needs (docs/protocol.md, "Frame layout").
// Fields are read once the byte layout is decided; until then only the TBD check uses them.
#[allow(dead_code)]
struct Layout {
    sync: u8,
    header_len: usize,
    length_field_bytes: usize,
    checksum_kind: u8,
    max_payload: usize,
}

fn layout() -> Result<Layout, ProtocolError> {
    Ok(Layout {
        sync: decided(tbd::PROTO_SYNC)?,
        header_len: decided(tbd::PROTO_HEADER_LEN)?,
        length_field_bytes: decided(tbd::PROTO_LENGTH_FIELD_BYTES)?,
        checksum_kind: decided(tbd::PROTO_CHECKSUM_KIND)?,
        max_payload: decided(tbd::PROTO_MAX_PAYLOAD)?,
    })
}

/// Encode one command frame.
///
/// Returns `Tbd` while any frame or command constant is `None`. Even when all of
/// them are decided, the Rust codec for the layout names in `tbd.rs` (offsets,
/// widths, byte order; docs/protocol.md "Frame layout") is not written yet, so this
/// still returns `Tbd` and produces no bytes. It never invents a layout.
pub fn encode_command(cmd: &Command) -> Result<Vec<u8>, ProtocolError> {
    let _ = layout()?;
    let _ = decided(tbd::PROTO_TYPE_COMMAND)?;
    let _ = decided(tbd::PROTO_CMD_SET_ODR)?;
    let _ = decided(tbd::PROTO_CMD_SET_RANGE)?;
    let _ = decided(tbd::PROTO_CMD_SET_FILTER)?;
    let _ = cmd;
    // TODO(TBD) command byte layout (docs/protocol.md "Command set"): no bytes until decided.
    Err(ProtocolError::Tbd)
}

/// Decode one frame at the start of `buf`; returns the frame and the bytes consumed.
///
/// Returns `Tbd` while any frame constant is `None`, before reading any byte of `buf`.
/// The byte layout is TODO(TBD) as in `encode_command`, so decoding also stays `Tbd`.
pub fn decode_frame(buf: &[u8]) -> Result<(Frame, usize), ProtocolError> {
    let _ = layout()?;
    let _ = decided(tbd::PROTO_TYPE_SAMPLE_BATCH)?;
    let _ = decided(tbd::PROTO_TYPE_RESPONSE)?;
    let _ = buf;
    // TODO(TBD) frame byte layout (docs/protocol.md "Frame layout"): no bytes consumed until decided.
    Err(ProtocolError::Tbd)
}
