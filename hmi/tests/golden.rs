//! Golden frames (design decision 2026-11-19, owner go t37u): the same literal bytes as
//! firmware/tests/test_protocol.c (lines starting "GOLDEN <name>:" are diffed by tools).
//! HMI side: encode/decode round trip, checksum failure, truncation, sync loss, unknown type.
use pico355_hmi::protocol::{decode_frame, encode_command, Command, Frame, ProtocolError, Response, Sample};

// GOLDEN BATCH: a5 01 12 00 01 00 00 fe ff ff 03 00 00 00 00 f8 ff ff 07 00 00 00 b1 b3
const G_BATCH: [u8; 24] = [0xa5,0x01,0x12,0x00,0x01,0x00,0x00,0xfe,0xff,0xff,0x03,0x00,0x00,0x00,0x00,0xf8,0xff,0xff,0x07,0x00,0x00,0x00,0xb1,0xb3];
// GOLDEN CMD_ODR: a5 02 05 00 01 a0 0f 00 00 e7 ed
const G_CMD_ODR: [u8; 11] = [0xa5,0x02,0x05,0x00,0x01,0xa0,0x0f,0x00,0x00,0xe7,0xed];
// GOLDEN CMD_RANGE: a5 02 05 00 02 03 00 00 00 ae 5e
const G_CMD_RANGE: [u8; 11] = [0xa5,0x02,0x05,0x00,0x02,0x03,0x00,0x00,0x00,0xae,0x5e];
// GOLDEN CMD_FILTER: a5 02 05 00 03 06 00 00 00 ba 48
const G_CMD_FILTER: [u8; 11] = [0xa5,0x02,0x05,0x00,0x03,0x06,0x00,0x00,0x00,0xba,0x48];
// GOLDEN RESP_ACK: a5 03 06 00 01 01 00 00 00 00 0e bc
const G_RESP_ACK: [u8; 12] = [0xa5,0x03,0x06,0x00,0x01,0x01,0x00,0x00,0x00,0x00,0x0e,0xbc];
// GOLDEN RESP_ERROR: a5 03 06 00 02 02 01 00 00 00 88 ea
const G_RESP_ERROR: [u8; 12] = [0xa5,0x03,0x06,0x00,0x02,0x02,0x01,0x00,0x00,0x00,0x88,0xea];
// GOLDEN UNKNOWN_TYPE: a5 7f 01 00 00 94 b9
const G_UNKNOWN_TYPE: [u8; 7] = [0xa5,0x7f,0x01,0x00,0x00,0x94,0xb9];

fn all_golden() -> Vec<(&'static str, Vec<u8>)> {
    vec![
        ("BATCH", G_BATCH.to_vec()),
        ("CMD_ODR", G_CMD_ODR.to_vec()),
        ("CMD_RANGE", G_CMD_RANGE.to_vec()),
        ("CMD_FILTER", G_CMD_FILTER.to_vec()),
        ("RESP_ACK", G_RESP_ACK.to_vec()),
        ("RESP_ERROR", G_RESP_ERROR.to_vec()),
    ]
}

#[test]
fn golden_batch_decodes_to_the_samples() {
    let (frame, used) = decode_frame(&G_BATCH).expect("decode batch");
    assert_eq!(used, G_BATCH.len());
    assert_eq!(
        frame,
        Frame::SampleBatch(vec![Sample { x: 1, y: -2, z: 3 }, Sample { x: -524288, y: 524287, z: 0 }])
    );
}

#[test]
fn golden_responses_decode() {
    let (f, n) = decode_frame(&G_RESP_ACK).expect("ack");
    assert_eq!((f, n), (Frame::Response(Response { code: 1, cmd_code: 1, value: 0 }), G_RESP_ACK.len()));
    let (f, n) = decode_frame(&G_RESP_ERROR).expect("error");
    assert_eq!((f, n), (Frame::Response(Response { code: 2, cmd_code: 2, value: 1 }), G_RESP_ERROR.len()));
}

#[test]
fn golden_commands_encode_to_the_golden_bytes() {
    assert_eq!(encode_command(&Command { code: 1, value: 4000 }).expect("odr"), G_CMD_ODR.to_vec());
    assert_eq!(encode_command(&Command { code: 2, value: 3 }).expect("range"), G_CMD_RANGE.to_vec());
    assert_eq!(encode_command(&Command { code: 3, value: 6 }).expect("filter"), G_CMD_FILTER.to_vec());
}

#[test]
fn checksum_fail_on_any_flipped_payload_or_crc_byte() {
    for (name, g) in all_golden() {
        for i in 4..g.len() {
            let mut b = g.clone();
            b[i] ^= 0x01;
            assert_eq!(decode_frame(&b).map(|(_, n)| n), Err(ProtocolError::Checksum), "{name} byte {i}");
        }
    }
}

#[test]
fn truncated_golden_is_short_at_every_length() {
    for (name, g) in all_golden() {
        for k in 0..g.len() {
            assert_eq!(decode_frame(&g[..k]).map(|(_, n)| n), Err(ProtocolError::Short), "{name} cut at {k}");
        }
    }
}

#[test]
fn unknown_frame_type_is_rejected() {
    assert_eq!(decode_frame(&G_UNKNOWN_TYPE).map(|(_, n)| n), Err(ProtocolError::UnknownType(0x7f)));
}

/// Receiver loop as main.rs runs it: drop what the error says, retry, until a frame decodes.
fn scan(buf: &[u8]) -> Option<(usize, Frame)> {
    let mut pos = 0;
    while pos < buf.len() {
        match decode_frame(&buf[pos..]) {
            Ok((f, _)) => return Some((pos, f)),
            Err(ProtocolError::Short) => return None,
            Err(ProtocolError::Sync(n)) => pos += n.max(1),
            Err(_) => pos += 1,
        }
    }
    None
}

#[test]
fn sync_loss_resyncs_to_the_frame_after_garbage() {
    let junk = [0x00u8, 0xA5, 0x7F, 0xA5, 0xA5, 0x01, 0x02];
    for k in 0..=junk.len() {
        let mut b = junk[..k].to_vec();
        b.extend_from_slice(&G_RESP_ACK);
        let (off, f) = scan(&b).unwrap_or_else(|| panic!("no frame after {k} junk bytes"));
        assert_eq!(off, k);
        assert_eq!(f, Frame::Response(Response { code: 1, cmd_code: 1, value: 0 }));
    }
}

#[test]
fn pure_garbage_reports_sync_with_a_positive_skip() {
    match decode_frame(&[0x11u8; 8]) {
        Err(ProtocolError::Sync(n)) => assert!(n > 0 && n <= 8),
        other => panic!("expected Sync, got {other:?}"),
    }
}
