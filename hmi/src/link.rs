//! Link to the Pico over USB CDC serial (model SerialConnection, PICO355-R8).
//! Serial settings are a design decision (2026-11-19, owner go t37u), see `config::link`:
//! `SERIAL_BAUD` (ignored by CDC), 8N1, no flow control, read timeout, DTR asserted.
//! Port discovery stays TODO(TBD) (see `tbd.rs`); the user gives the port path.

use std::io::{self, Read, Write};
use std::time::Duration;

use crate::config::link;

/// Byte transport to the firmware; implemented by the serial port and by test doubles.
pub trait Transport {
    /// Read available bytes into `buf`; returns the count (0 = none before timeout).
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize>;
    /// Write all of `buf`.
    fn write_all(&mut self, buf: &[u8]) -> io::Result<()>;
}

/// Link errors.
#[derive(Debug)]
pub enum LinkError {
    Io(io::Error),
    Serial(serialport::Error),
    /// Serial settings still TODO(TBD).
    Tbd,
}

impl std::fmt::Display for LinkError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "link error: {self:?}")
    }
}

impl std::error::Error for LinkError {}

/// USB CDC serial transport (model action openSerialConnection).
pub struct SerialTransport {
    port: Box<dyn serialport::SerialPort>,
}

impl SerialTransport {
    /// Open the port at `path` (for example /dev/ttyACM0 or COM5) with the settings in `config::link`:
    /// 8N1, no flow control, `READ_TIMEOUT_MS` read timeout, DTR asserted.
    pub fn open(path: &str) -> Result<Self, LinkError> {
        let mut port = serialport::new(path, link::SERIAL_BAUD)
            .timeout(Duration::from_millis(link::READ_TIMEOUT_MS))
            .open()
            .map_err(LinkError::Serial)?;
        if link::ASSERT_DTR {
            // The Pico USB CDC only transmits while the host has DTR set.
            port.write_data_terminal_ready(true).map_err(LinkError::Serial)?;
        }
        Ok(Self { port })
    }
}

impl Transport for SerialTransport {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        match self.port.read(buf) {
            Ok(n) => Ok(n),
            // Read timeout with no bytes available: the trait contract returns 0.
            Err(e) if e.kind() == io::ErrorKind::TimedOut => Ok(0),
            Err(e) => Err(e),
        }
    }

    fn write_all(&mut self, buf: &[u8]) -> io::Result<()> {
        self.port.write_all(buf)
    }
}
