//! Sample logger (model DataLogger / LogSamples, PICO355-R9). Log format and storage: TODO(TBD).
//!
//! While `tbd::LOG_FORMAT` is `None` (format not decided), `log_batch` writes no bytes and
//! returns `ErrorKind::Unsupported` naming the TBD format. It never invents a format.

use std::io::{self, Write};

use crate::protocol::Sample;
use crate::tbd;

/// Writes samples to any writer (a file in the binary).
pub struct Logger<W: Write> {
    out: W,
}

impl<W: Write> Logger<W> {
    /// Wrap a writer.
    pub fn new(out: W) -> Self {
        Self { out }
    }

    /// Append one batch in the log format (TODO(TBD) `tbd::LOG_FORMAT`).
    /// Returns `Unsupported` and writes nothing while the format is TBD.
    pub fn log_batch(&mut self, samples: &[Sample]) -> io::Result<()> {
        let _ = samples; // not written while the format is TBD
        match tbd::LOG_FORMAT {
            None => Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "log format TBD: tbd::LOG_FORMAT is not decided (TODO(TBD) log format)",
            )),
            // Format decided later: the encoder is added with the format; nothing invented here.
            Some(_) => Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "log encoder not implemented yet (TODO(TBD) log format)",
            )),
        }
    }

    /// Return the inner writer.
    pub fn into_inner(self) -> W {
        self.out
    }
}

/// Open a log file at `path` (create or truncate). No format is needed to create the file.
pub fn open_file(path: &std::path::Path) -> io::Result<Logger<std::fs::File>> {
    std::fs::File::create(path).map(Logger::new)
}
