//! Live data view (model LiveDataView / ShowLiveData, PICO355-R9).
//! HMI form TODO(TBD): placeholder is a plain terminal text view; refresh rate TODO(TBD).

use std::io::{self, Write};

use crate::protocol::Sample;

/// Render `samples` as text lines to `out`.
pub fn render<W: Write>(out: &mut W, samples: &[Sample]) -> io::Result<()> {
    // One line per sample: "x y z" in raw decimal counts (no units, no scaling).
    for s in samples {
        writeln!(out, "{} {} {}", s.x, s.y, s.z)?;
    }
    Ok(())
}
