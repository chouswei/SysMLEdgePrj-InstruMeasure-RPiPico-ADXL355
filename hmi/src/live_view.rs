//! Live data view (model LiveDataView / ShowLiveData, PICO355-R9).
//! HMI form TODO(TBD): placeholder is a plain terminal text view; refresh rate TODO(TBD).

use std::io::{self, Write};

use crate::protocol::Sample;

/// Render `samples` as text lines to `out`.
pub fn render<W: Write>(out: &mut W, samples: &[Sample]) -> io::Result<()> {
    let _ = (out, samples);
    Err(io::Error::new(io::ErrorKind::Unsupported, "not implemented"))
}
