//! Route checks for hmi/src/link.rs. Transport is exercised through an in-memory mock;
//! the only real open is of a path that does not exist, which must fail as a serial error
//! (serial settings are decided: design decision 2026-11-19, owner go t37u).
use std::io;

use pico355_hmi::link::{LinkError, SerialTransport, Transport};
use pico355_hmi::protocol::{decode_frame, Frame, Response};

#[derive(Default)]
struct MockTransport {
    to_read: Vec<u8>,
    written: Vec<u8>,
    read_calls: usize,
}

impl Transport for MockTransport {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.read_calls += 1;
        let n = buf.len().min(self.to_read.len());
        buf[..n].copy_from_slice(&self.to_read[..n]);
        self.to_read.drain(..n);
        Ok(n)
    }
    fn write_all(&mut self, buf: &[u8]) -> io::Result<()> {
        self.written.extend_from_slice(buf);
        Ok(())
    }
}

#[test]
fn mock_transport_satisfies_the_trait_contract() {
    let mut t = MockTransport { to_read: vec![1, 2, 3], ..Default::default() };
    let mut buf = [0u8; 8];
    assert_eq!(t.read(&mut buf).unwrap(), 3);
    assert_eq!(&buf[..3], &[1, 2, 3]);
    assert_eq!(t.read(&mut buf).unwrap(), 0);
    t.write_all(&[9, 8]).unwrap();
    assert_eq!(t.written, vec![9, 8]);
}

#[test]
fn golden_bytes_from_mock_transport_decode_to_the_response() {
    // GOLDEN RESP_ACK, same bytes as hmi/tests/golden.rs
    let golden = vec![0xa5u8, 0x03, 0x06, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x0e, 0xbc];
    let mut t = MockTransport { to_read: golden.clone(), ..Default::default() };
    let mut buf = [0u8; 32];
    let n = t.read(&mut buf).unwrap();
    assert_eq!(
        decode_frame(&buf[..n]).unwrap(),
        (Frame::Response(Response { code: 1, cmd_code: 1, value: 0 }), golden.len())
    );
}

#[test]
fn open_of_a_missing_port_is_a_serial_error_not_tbd() {
    match SerialTransport::open("/nonexistent/pico355-test-port") {
        Err(LinkError::Serial(_)) => {}
        Err(other) => panic!("expected LinkError::Serial, got {other:?}"),
        Ok(_) => panic!("expected an error opening a missing port"),
    }
}
