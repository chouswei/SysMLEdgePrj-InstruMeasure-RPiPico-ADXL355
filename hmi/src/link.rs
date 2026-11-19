//! Link to the Pico over USB CDC serial (model SerialConnection, PICO355-R8).
//! Port discovery and serial settings: TODO(TBD) (see `tbd.rs`); the user gives the port path.

use std::io;

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
    pub fn open(path: &str) -> Result<Self, LinkError> {
        let _ = path;
        Err(LinkError::NotImplemented)
    }
}

impl Transport for SerialTransport {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let _ = (&mut self.port, buf);
        Err(io::Error::new(io::ErrorKind::Unsupported, "not implemented"))
    }

    fn write_all(&mut self, buf: &[u8]) -> io::Result<()> {
        let _ = (&mut self.port, buf);
        Err(io::Error::new(io::ErrorKind::Unsupported, "not implemented"))
    }
}
