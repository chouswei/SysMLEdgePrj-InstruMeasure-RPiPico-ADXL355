//! PH route checks for hmi/src/link.rs. Transport is exercised through an in-memory
//! mock only; no real serial port is opened. While tbd::SERIAL_BAUD is None,
//! SerialTransport::open returns LinkError::Tbd before touching any port.
use std::io;

use pico355_hmi::link::{LinkError, SerialTransport, Transport};
use pico355_hmi::protocol::{decode_frame, ProtocolError};
use pico355_hmi::tbd;

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
fn bytes_from_mock_transport_decode_to_tbd_error() {
    let mut t = MockTransport { to_read: vec![0xAA; 16], ..Default::default() };
    let mut buf = [0u8; 16];
    let n = t.read(&mut buf).unwrap();
    assert_eq!(decode_frame(&buf[..n]).map(|(_, c)| c), Err(ProtocolError::Tbd));
}

#[test]
fn open_returns_tbd_while_serial_settings_are_tbd() {
    assert!(tbd::SERIAL_BAUD.is_none());
    match SerialTransport::open("/nonexistent/pico355-test-port") {
        Err(LinkError::Tbd) => {}
        Err(other) => panic!("expected LinkError::Tbd, got {other:?}"),
        Ok(_) => panic!("expected LinkError::Tbd, got Ok"),
    }
}
