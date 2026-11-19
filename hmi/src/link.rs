//! Link to the Pico over USB CDC serial (model SerialConnection, PICO355-R8).
//! Port discovery and serial settings: TODO(TBD) (see `tbd.rs`); the user gives the port path.

use std::io::{self, Read, Write};

use crate::tbd;

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
    NotImplemented,
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
    /// Open the port at `path` (for example /dev/ttyACM0 or COM3).
    /// Returns `LinkError::Tbd` before touching any port while `tbd::SERIAL_BAUD` is None.
    pub fn open(path: &str) -> Result<Self, LinkError> {
        // TODO(TBD) serial settings: baud value comes from tbd.rs; None = not decided.
        let baud = tbd::SERIAL_BAUD.ok_or(LinkError::Tbd)?;
        // TODO(TBD) serial settings: no read timeout is set, so a run loop that sees 0-byte
        // reads can busy-spin; set one here once the serial-settings decision exists.
        let port = serialport::new(path, baud).open().map_err(LinkError::Serial)?;
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
