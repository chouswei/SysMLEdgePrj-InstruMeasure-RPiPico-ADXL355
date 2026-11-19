//! Command set and encode paths (design decision 2026-11-19, owner go t37u):
//! each command encodes and decodes on the HMI side; response and batch encode to the golden bytes.
use pico355_hmi::config::link;
use pico355_hmi::protocol::{decode_frame, encode_command, encode_response, encode_sample_batch, Command, Frame, Response, Sample};

#[test]
fn every_command_round_trips() {
    for code in [link::CMD_SET_ODR, link::CMD_SET_RANGE, link::CMD_SET_FILTER] {
        for value in [0u32, 1, 4000, 0xFFFF_FFFF] {
            let cmd = Command { code, value };
            let bytes = encode_command(&cmd).expect("encode");
            let (frame, used) = decode_frame(&bytes).expect("decode");
            assert_eq!(used, bytes.len());
            assert_eq!(frame, Frame::Command(cmd));
        }
    }
}

#[test]
fn command_codes_are_distinct() {
    let c = [link::CMD_SET_ODR, link::CMD_SET_RANGE, link::CMD_SET_FILTER, link::CMD_GET_STATUS];
    for i in 0..c.len() { for j in (i + 1)..c.len() { assert!(c[i] != c[j]); } }
    let r = [link::RESP_ACK, link::RESP_ERROR, link::RESP_STATUS];
    assert!(r[0] != r[1] && r[1] != r[2] && r[0] != r[2]);
}

#[test]
fn response_and_batch_encode_to_the_golden_bytes() {
    // same bytes as GOLDEN RESP_ACK and GOLDEN BATCH in hmi/tests/golden.rs
    let ack = encode_response(&Response { code: link::RESP_ACK, cmd_code: link::CMD_SET_ODR, value: 0 }).unwrap();
    assert_eq!(ack, vec![0xa5,0x03,0x06,0x00,0x01,0x01,0x00,0x00,0x00,0x00,0x0e,0xbc]);
    let batch = encode_sample_batch(&[Sample { x: 1, y: -2, z: 3 }, Sample { x: -524288, y: 524287, z: 0 }]).unwrap();
    assert_eq!(batch, vec![0xa5,0x01,0x12,0x00,0x01,0x00,0x00,0xfe,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0xf8,0xff,0xff,0x07,0x00,0x00,0x00,0xb1,0xb3]);
}

#[test]
fn batch_round_trip_at_the_maximum_batch() {
    let n = link::MAX_PAYLOAD / (3 * link::SAMPLE_AXIS_BYTES);
    let s: Vec<Sample> = (0..n as i32).map(|i| Sample { x: i * 8191 - 262144, y: -i * 4099, z: if i % 2 == 0 { -524288 } else { 524287 } }).collect();
    let bytes = encode_sample_batch(&s).unwrap();
    assert_eq!(decode_frame(&bytes).unwrap(), (Frame::SampleBatch(s), bytes.len()));
}
