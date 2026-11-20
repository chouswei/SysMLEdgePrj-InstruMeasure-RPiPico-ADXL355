//! Wire codec on the PC side (PICO355-R5, PICO355-R6): encode commands, responses and sample
//! batches, decode sample-batch, response and command frames. Layout constants come only from
//! `config::link` (mirror of firmware `CFG_PROTO_*`); see docs/protocol.md.
//!
//! Layout, command set and checksum are design decisions (2026-11-19, owner go t37u), not part
//! specs. Frame = header (sync, type, length u16 LE) + payload + CRC-16/CCITT-FALSE (u16 LE)
//! over header + payload. The golden byte vectors in hmi/tests/golden.rs are the same literal
//! bytes as in firmware/tests/test_protocol.c.
//!
//! Receiver rule (same as firmware): `Short` = wait for more bytes (nothing consumed); `Sync(n)`
//! = drop `n` bytes; `Checksum`, `TooLong` and `UnknownType` = drop 1 byte and rescan
//! (`ProtocolError::skip`).

use crate::config::link;

/// One raw X, Y, Z sample (model item AccelSample), sign-extended 20-bit.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Sample {
    pub x: i32,
    pub y: i32,
    pub z: i32,
}

/// Command payload (model ConfigCommand); `code` is a `link::CMD_*`, `value` per command
/// (ODR in Hz, range as firmware `adxl355_range_t`, filter as high-pass code).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Command {
    pub code: u8,
    pub value: u32,
}

/// Response payload (model ResponseMessage); `code` is a `link::RESP_*`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Response {
    pub code: u8,
    pub cmd_code: u8,
    pub value: u32,
}

/// A decoded frame.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Frame {
    SampleBatch(Vec<Sample>),
    Response(Response),
    /// Not sent by the Pico; decoded so the command set round-trips on the host.
    Command(Command),
}

/// Codec errors.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ProtocolError {
    /// Not enough bytes for a whole frame yet; consume nothing.
    Short,
    /// No sync found; drop this many bytes.
    Sync(usize),
    /// CRC mismatch; drop 1 byte and rescan.
    Checksum,
    /// Length above `link::MAX_PAYLOAD`, or a value that does not fit its field.
    TooLong,
    /// Frame type id is not sample batch, command or response; drop 1 byte and rescan.
    UnknownType(u8),
    /// Payload length does not match the frame type's layout.
    BadPayload,
}

impl ProtocolError {
    /// Bytes a receiver drops before retrying; `None` for `Short` (wait for more bytes).
    pub fn skip(&self) -> Option<usize> {
        match self {
            ProtocolError::Short => None,
            ProtocolError::Sync(n) => Some((*n).max(1)),
            _ => Some(1),
        }
    }
}

impl std::fmt::Display for ProtocolError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "protocol error: {self:?}")
    }
}

impl std::error::Error for ProtocolError {}

/// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final xor (check value 0x29B1
/// over "123456789"). Design decision (2026-11-19, owner go t37u).
pub fn crc16(data: &[u8]) -> u16 {
    let mut crc: u16 = 0xFFFF;
    for &b in data {
        crc ^= (b as u16) << 8;
        for _ in 0..8 {
            crc = if crc & 0x8000 != 0 { (crc << 1) ^ 0x1021 } else { crc << 1 };
        }
    }
    crc
}

/// Build one frame: header + payload + CRC.
fn build(frame_type: u8, payload: &[u8]) -> Result<Vec<u8>, ProtocolError> {
    if payload.len() > link::MAX_PAYLOAD {
        return Err(ProtocolError::TooLong);
    }
    let mut f = Vec::with_capacity(link::HEADER_LEN + payload.len() + link::CHECKSUM_LEN_BYTES);
    f.push(link::SYNC);
    f.push(frame_type);
    f.extend_from_slice(&(payload.len() as u16).to_le_bytes());
    f.extend_from_slice(payload);
    let crc = crc16(&f);
    f.extend_from_slice(&crc.to_le_bytes());
    Ok(f)
}

/// Encode one command frame (type `link::TYPE_COMMAND`, payload code + u32 LE value).
pub fn encode_command(cmd: &Command) -> Result<Vec<u8>, ProtocolError> {
    let mut p = [link::PAD_BYTE; link::CMD_PAYLOAD_LEN];
    p[link::CMD_CODE_OFFSET] = cmd.code;
    p[link::CMD_VALUE_OFFSET..link::CMD_VALUE_OFFSET + link::CMD_VALUE_BYTES]
        .copy_from_slice(&cmd.value.to_le_bytes());
    build(link::TYPE_COMMAND, &p)
}

/// Encode one response frame (what the firmware sends; used by host tests).
pub fn encode_response(resp: &Response) -> Result<Vec<u8>, ProtocolError> {
    let mut p = [link::PAD_BYTE; link::RESP_PAYLOAD_LEN];
    p[link::RESP_CODE_OFFSET] = resp.code;
    p[link::RESP_CMD_CODE_OFFSET] = resp.cmd_code;
    p[link::RESP_VALUE_OFFSET..link::RESP_VALUE_OFFSET + link::RESP_VALUE_BYTES]
        .copy_from_slice(&resp.value.to_le_bytes());
    build(link::TYPE_RESPONSE, &p)
}

/// Encode a sample batch (what the firmware sends; used by host tests): x, y, z per sample,
/// 3-byte two's complement little-endian each.
pub fn encode_sample_batch(samples: &[Sample]) -> Result<Vec<u8>, ProtocolError> {
    const LIMIT: i32 = 1 << (8 * link::SAMPLE_AXIS_BYTES - 1);
    let mut p = Vec::with_capacity(samples.len() * 3 * link::SAMPLE_AXIS_BYTES);
    for s in samples {
        for v in [s.x, s.y, s.z] {
            if !(-LIMIT..LIMIT).contains(&v) {
                return Err(ProtocolError::TooLong);
            }
            p.extend_from_slice(&v.to_le_bytes()[..link::SAMPLE_AXIS_BYTES]);
        }
    }
    build(link::TYPE_SAMPLE_BATCH, &p)
}

fn le_u32(b: &[u8]) -> u32 {
    b.iter().rev().fold(0u32, |a, &x| (a << 8) | x as u32)
}

/// Sign-extend an `SAMPLE_AXIS_BYTES`-byte little-endian two's complement value.
fn le_signed(b: &[u8]) -> i32 {
    let shift = 32 - 8 * b.len() as u32;
    ((le_u32(b) << shift) as i32) >> shift
}

/// Decode one frame at the start of `buf`; returns the frame and the bytes consumed.
/// Errors follow the receiver rule in the module doc; the input is never modified.
pub fn decode_frame(buf: &[u8]) -> Result<(Frame, usize), ProtocolError> {
    let hl = link::HEADER_LEN;
    if buf.len() <= link::SYNC_OFFSET {
        return Err(ProtocolError::Short);
    }
    if buf[link::SYNC_OFFSET] != link::SYNC {
        // Drop up to the next candidate sync byte; with none in view, drop everything seen.
        let skip = buf[1..].iter().position(|&b| b == link::SYNC).map_or(buf.len(), |i| i + 1);
        return Err(ProtocolError::Sync(skip));
    }
    if buf.len() < hl {
        return Err(ProtocolError::Short);
    }
    let ftype = buf[link::TYPE_OFFSET];
    let len = u16::from_le_bytes([buf[link::LENGTH_OFFSET], buf[link::LENGTH_OFFSET + 1]]) as usize;
    if len > link::MAX_PAYLOAD {
        return Err(ProtocolError::TooLong);
    }
    if ![link::TYPE_SAMPLE_BATCH, link::TYPE_COMMAND, link::TYPE_RESPONSE].contains(&ftype) {
        return Err(ProtocolError::UnknownType(ftype));
    }
    let total = hl + len + link::CHECKSUM_LEN_BYTES;
    if buf.len() < total {
        return Err(ProtocolError::Short);
    }
    let want = u16::from_le_bytes([buf[hl + len], buf[hl + len + 1]]);
    if crc16(&buf[..hl + len]) != want {
        return Err(ProtocolError::Checksum);
    }
    let p = &buf[hl..hl + len];
    let frame = match ftype {
        link::TYPE_SAMPLE_BATCH => {
            let sb = 3 * link::SAMPLE_AXIS_BYTES;
            if p.len() % sb != 0 {
                return Err(ProtocolError::BadPayload);
            }
            let ab = link::SAMPLE_AXIS_BYTES;
            Frame::SampleBatch(
                p.chunks_exact(sb)
                    .map(|c| Sample { x: le_signed(&c[..ab]), y: le_signed(&c[ab..2 * ab]), z: le_signed(&c[2 * ab..]) })
                    .collect(),
            )
        }
        link::TYPE_COMMAND => {
            if p.len() != link::CMD_PAYLOAD_LEN {
                return Err(ProtocolError::BadPayload);
            }
            Frame::Command(Command {
                code: p[link::CMD_CODE_OFFSET],
                value: le_u32(&p[link::CMD_VALUE_OFFSET..link::CMD_VALUE_OFFSET + link::CMD_VALUE_BYTES]),
            })
        }
        _ => {
            if p.len() != link::RESP_PAYLOAD_LEN {
                return Err(ProtocolError::BadPayload);
            }
            Frame::Response(Response {
                code: p[link::RESP_CODE_OFFSET],
                cmd_code: p[link::RESP_CMD_CODE_OFFSET],
                value: le_u32(&p[link::RESP_VALUE_OFFSET..link::RESP_VALUE_OFFSET + link::RESP_VALUE_BYTES]),
            })
        }
    };
    Ok((frame, total))
}
