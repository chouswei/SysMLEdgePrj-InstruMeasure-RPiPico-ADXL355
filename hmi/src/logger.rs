//! Sample logger (model DataLogger / LogSamples, PICO355-R9). Log format and storage: TODO(TBD).

use std::io::{self, Write};

use crate::protocol::Sample;

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
    pub fn log_batch(&mut self, samples: &[Sample]) -> io::Result<()> {
        let _ = (&mut self.out, samples);
        Err(io::Error::new(io::ErrorKind::Unsupported, "not implemented"))
    }

    /// Return the inner writer.
    pub fn into_inner(self) -> W {
        self.out
    }
}

/// Open a log file at `path` (create or truncate).
pub fn open_file(path: &std::path::Path) -> io::Result<Logger<std::fs::File>> {
    let _ = path;
    Err(io::Error::new(io::ErrorKind::Unsupported, "not implemented"))
}
