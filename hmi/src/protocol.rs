//! Wire codec on the PC side (PICO355-R5, PICO355-R6): encode commands, decode
//! sample-batch and response frames. Layout constants come only from `tbd.rs`
//! (mirror of firmware `CFG_PROTO_*`); see docs/protocol.md.

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

/// Encode one command frame.
pub fn encode_command(cmd: &Command) -> Result<Vec<u8>, ProtocolError> {
    let _ = cmd;
    Err(ProtocolError::NotImplemented)
}

/// Decode one frame at the start of `buf`; returns the frame and the bytes consumed.
pub fn decode_frame(buf: &[u8]) -> Result<(Frame, usize), ProtocolError> {
    let _ = buf;
    Err(ProtocolError::NotImplemented)
}
