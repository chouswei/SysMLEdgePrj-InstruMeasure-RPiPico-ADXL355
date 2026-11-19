//! PH route checks for hmi/src/protocol.rs. While every tbd::PROTO_* is None
//! (docs/protocol.md "TBD rule") encode/decode return Err(ProtocolError::Tbd) and
//! produce no bytes. No round-trip test exists until the frame format is decided.
use pico355_hmi::protocol::{decode_frame, encode_command, Command, ProtocolError, Sample};
use pico355_hmi::tbd;

#[test]
fn tbd_state_is_the_precondition_of_these_tests() {
    assert!(tbd::PROTO_SYNC.is_none());
    assert!(tbd::PROTO_HEADER_LEN.is_none());
    assert!(tbd::PROTO_LENGTH_FIELD_BYTES.is_none());
    assert!(tbd::PROTO_CHECKSUM_KIND.is_none());
    assert!(tbd::PROTO_MAX_PAYLOAD.is_none());
    assert!(tbd::PROTO_TYPE_SAMPLE_BATCH.is_none());
    assert!(tbd::PROTO_TYPE_COMMAND.is_none());
    assert!(tbd::PROTO_TYPE_RESPONSE.is_none());
    assert!(tbd::PROTO_CMD_SET_ODR.is_none());
    assert!(tbd::PROTO_CMD_SET_RANGE.is_none());
    assert!(tbd::PROTO_CMD_SET_FILTER.is_none());
}

#[test]
fn encode_command_returns_tbd_and_no_bytes() {
    let cmd = Command { code: 0, value: tbd::DEFAULT_ODR_HZ };
    assert_eq!(encode_command(&cmd), Err(ProtocolError::Tbd));
}

#[test]
fn decode_frame_returns_tbd_for_arbitrary_bytes() {
    let buf = [0u8, 1, 2, 3, 0xFF, 0xA5];
    assert_eq!(decode_frame(&buf).map(|(_, n)| n), Err(ProtocolError::Tbd));
}

#[test]
fn decode_frame_returns_tbd_for_empty_buffer() {
    assert_eq!(decode_frame(&[]).map(|(_, n)| n), Err(ProtocolError::Tbd));
}

#[test]
fn decode_frame_does_not_mutate_or_consume_input() {
    let buf = vec![7u8; 64];
    let copy = buf.clone();
    assert!(decode_frame(&buf).is_err());
    assert_eq!(buf, copy);
}

#[test]
fn sample_default_is_zero() {
    assert_eq!(Sample::default(), Sample { x: 0, y: 0, z: 0 });
}
