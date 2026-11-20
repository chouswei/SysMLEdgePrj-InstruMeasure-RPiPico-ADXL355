//! Route checks for hmi/src/protocol.rs. The frame layout is settled (design decision 2026-11-19,
//! owner go t37u); the byte-exact cases are in golden.rs and command_set.rs. This file keeps the
//! checks that need no layout: argument edges and input immutability.
use pico355_hmi::protocol::{decode_frame, ProtocolError, Sample};

#[test]
fn decode_frame_on_empty_buffer_is_short_not_tbd() {
    assert_eq!(decode_frame(&[]).map(|(_, n)| n), Err(ProtocolError::Short));
}

#[test]
fn decode_frame_does_not_mutate_input() {
    let buf = vec![7u8; 64];
    let copy = buf.clone();
    assert!(decode_frame(&buf).is_err());
    assert_eq!(buf, copy);
}

#[test]
fn sample_default_is_zero() {
    assert_eq!(Sample::default(), Sample { x: 0, y: 0, z: 0 });
}
