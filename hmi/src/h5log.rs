//! HDF5 sample logger (model DataLogger / LogSamples, PICO355-R9), written with the pure-Rust
//! `hdf5-pure` crate from a background writer thread so the GUI never blocks on disk.
//!
//! File schema (readable by h5py):
//! - `/accel_g/x_g`, `/accel_g/y_g`, `/accel_g/z_g`: f64, acceleration in g (see `scale`).
//! - `/raw_counts/x`, `/raw_counts/y`, `/raw_counts/z`: i32, sign-extended 20-bit raw counts.
//! - `/sample_index`: u64, the push index of each sample.
//! - Every dataset is rank 1, maxshape unlimited, chunked `CHUNK_LEN`, created empty; the
//!   crate appends rank-1 only, hence one dataset per axis.
//! - Root attributes: `odr_hz`, `range_g`, `scale_g_per_lsb` (cited, adxl355.h:62-65 and
//!   adxl355.c:18 via `scale::g_per_lsb`), `scale_lsb_per_g` (DERIVED reciprocal),
//!   `hpf_code` (i32; -1 = high-pass not applied by this HMI, design decision (owner t43s2)),
//!   `fifo_overflow_count`, `units` = "g", `sensor` = "ADXL355", `started_utc`.
//!
//! Threading contract: `start` creates the file, the empty datasets and the attributes, then
//! spawns the writer thread. `push` never blocks (unbounded channel). The writer appends each
//! batch and then updates `samples_written` and `bytes_written` (bytes = file length). `stop`
//! drains the channel, writes the final attributes (overflow count), closes the file and joins
//! the thread. Dropping a logger without `stop` finalizes best-effort.

use std::path::{Path, PathBuf};

// The writer uses hdf5_pure; the skeleton only links it.
use hdf5_pure as _;

use crate::protocol::Sample;

/// Group of the g datasets.
pub const GROUP_ACCEL_G: &str = "accel_g";
/// X in g (f64).
pub const DS_X_G: &str = "x_g";
/// Y in g (f64).
pub const DS_Y_G: &str = "y_g";
/// Z in g (f64).
pub const DS_Z_G: &str = "z_g";
/// Group of the raw count datasets.
pub const GROUP_RAW: &str = "raw_counts";
/// X raw counts (i32).
pub const DS_RAW_X: &str = "x";
/// Y raw counts (i32).
pub const DS_RAW_Y: &str = "y";
/// Z raw counts (i32).
pub const DS_RAW_Z: &str = "z";
/// Sample index at the file root (u64).
pub const DS_SAMPLE_INDEX: &str = "sample_index";

/// Root attribute: ODR in Hz.
pub const ATTR_ODR_HZ: &str = "odr_hz";
/// Root attribute: full-scale range in g (2, 4, 8).
pub const ATTR_RANGE_G: &str = "range_g";
/// Root attribute: g per LSB for the range (cited, see `scale`).
pub const ATTR_SCALE_G_PER_LSB: &str = "scale_g_per_lsb";
/// Root attribute: LSB per g (DERIVED reciprocal).
pub const ATTR_SCALE_LSB_PER_G: &str = "scale_lsb_per_g";
/// Root attribute: high-pass code (i32, -1 = not applied).
pub const ATTR_HPF_CODE: &str = "hpf_code";
/// Root attribute: FIFO overflow count from the last status.
pub const ATTR_FIFO_OVERFLOW_COUNT: &str = "fifo_overflow_count";
/// Root attribute: units of the g datasets (value "g").
pub const ATTR_UNITS: &str = "units";
/// Root attribute: sensor name (value "ADXL355").
pub const ATTR_SENSOR: &str = "sensor";
/// Root attribute: start time, UTC `YYYY-MM-DDTHH:MM:SSZ`.
pub const ATTR_STARTED_UTC: &str = "started_utc";

/// Chunk length of every dataset (the preflight writer used 4096).
pub const CHUNK_LEN: usize = 4096;

/// Settings recorded in the file attributes; taken from the ACKed device state.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LogMeta {
    pub odr_hz: u32,
    /// Range code 1..=3 (`scale::RANGE_CODE_*`).
    pub range_code: u8,
    /// High-pass code; `None` = not applied by this HMI (attribute -1).
    pub hpf_code: Option<u8>,
    pub fifo_overflow_count: u32,
    /// UTC start time `YYYY-MM-DDTHH:MM:SSZ`.
    pub started_utc: String,
}

/// One sample to log with its push index.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct LoggedSample {
    pub index: u64,
    pub raw: Sample,
}

/// Result of a stopped log.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LogSummary {
    pub samples: u64,
    pub bytes: u64,
    pub path: PathBuf,
}

/// Logger errors.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum LogError {
    Io(String),
    Hdf5(String),
    /// The writer thread has ended.
    Closed,
    /// `LogMeta` not usable (for example an invalid range code).
    BadMeta(String),
}

impl std::fmt::Display for LogError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "log error: {self:?}")
    }
}

impl std::error::Error for LogError {}

/// Handle to a running HDF5 log (writer thread behind it).
pub struct H5Logger {
    path: PathBuf,
}

impl H5Logger {
    /// Create the file at `path` with empty datasets and the attributes from `meta`, then spawn
    /// the writer thread.
    pub fn start(path: &Path, meta: LogMeta) -> Result<H5Logger, LogError> {
        let _ = (path, meta);
        todo!()
    }

    /// Queue samples for the writer; never blocks. `Closed` when the writer has ended.
    pub fn push(&self, samples: Vec<LoggedSample>) -> Result<(), LogError> {
        let _ = samples;
        todo!()
    }

    /// Record the latest FIFO overflow count (written to the attribute on stop).
    pub fn set_overflow_count(&self, count: u32) {
        let _ = count;
        todo!()
    }

    /// Samples appended to the file so far.
    pub fn samples_written(&self) -> u64 {
        todo!()
    }

    /// File length in bytes after the last append.
    pub fn bytes_written(&self) -> u64 {
        todo!()
    }

    /// File path.
    pub fn path(&self) -> &Path {
        &self.path
    }

    /// Drain, write the final attributes, close the file and join the writer.
    pub fn stop(self) -> Result<LogSummary, LogError> {
        todo!()
    }
}

impl Drop for H5Logger {
    /// Best-effort finalize when `stop` was not called (skeleton: nothing to finalize yet).
    fn drop(&mut self) {}
}
