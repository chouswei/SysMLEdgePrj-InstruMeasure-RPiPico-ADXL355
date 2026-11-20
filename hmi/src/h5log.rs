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
use std::sync::atomic::{AtomicU32, AtomicU64, Ordering};
use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::Arc;
use std::thread::JoinHandle;

use hdf5_pure::{AttrValue, Dataset, File, FileBuilder, MaxExtent};

use crate::protocol::Sample;
use crate::scale;

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

/// Counters shared between the handle and the writer thread.
#[derive(Default)]
struct Shared {
    samples: AtomicU64,
    bytes: AtomicU64,
    overflow: AtomicU32,
}

/// Handle to a running HDF5 log (writer thread behind it).
pub struct H5Logger {
    path: PathBuf,
    shared: Arc<Shared>,
    tx: Option<Sender<Vec<LoggedSample>>>,
    writer: Option<JoinHandle<Result<(), LogError>>>,
}

fn h5_err(e: impl std::fmt::Display) -> LogError {
    LogError::Hdf5(e.to_string())
}

fn file_len(path: &Path) -> u64 {
    std::fs::metadata(path).map(|m| m.len()).unwrap_or(0)
}

/// The seven datasets the writer appends to.
struct Columns {
    x_g: Dataset,
    y_g: Dataset,
    z_g: Dataset,
    raw_x: Dataset,
    raw_y: Dataset,
    raw_z: Dataset,
    index: Dataset,
}

impl Columns {
    fn open(f: &File) -> Result<Columns, LogError> {
        let ds = |name: String| f.dataset(&name).map_err(h5_err);
        Ok(Columns {
            x_g: ds(format!("{GROUP_ACCEL_G}/{DS_X_G}"))?,
            y_g: ds(format!("{GROUP_ACCEL_G}/{DS_Y_G}"))?,
            z_g: ds(format!("{GROUP_ACCEL_G}/{DS_Z_G}"))?,
            raw_x: ds(format!("{GROUP_RAW}/{DS_RAW_X}"))?,
            raw_y: ds(format!("{GROUP_RAW}/{DS_RAW_Y}"))?,
            raw_z: ds(format!("{GROUP_RAW}/{DS_RAW_Z}"))?,
            index: ds(DS_SAMPLE_INDEX.to_string())?,
        })
    }

    /// Append one batch; g = raw * g_per_lsb of the range the file was created with.
    fn append(&mut self, batch: &[LoggedSample], g_per_lsb: f64) -> Result<(), LogError> {
        if batch.is_empty() {
            return Ok(());
        }
        let n = batch.len();
        let (mut gx, mut gy, mut gz) = (Vec::with_capacity(n), Vec::with_capacity(n), Vec::with_capacity(n));
        let (mut rx, mut ry, mut rz) = (Vec::with_capacity(n), Vec::with_capacity(n), Vec::with_capacity(n));
        let mut idx = Vec::with_capacity(n);
        for s in batch {
            gx.push(s.raw.x as f64 * g_per_lsb);
            gy.push(s.raw.y as f64 * g_per_lsb);
            gz.push(s.raw.z as f64 * g_per_lsb);
            rx.push(s.raw.x);
            ry.push(s.raw.y);
            rz.push(s.raw.z);
            idx.push(s.index);
        }
        self.x_g.append(&gx).map_err(h5_err)?;
        self.y_g.append(&gy).map_err(h5_err)?;
        self.z_g.append(&gz).map_err(h5_err)?;
        self.raw_x.append(&rx).map_err(h5_err)?;
        self.raw_y.append(&ry).map_err(h5_err)?;
        self.raw_z.append(&rz).map_err(h5_err)?;
        self.index.append(&idx).map_err(h5_err)?;
        Ok(())
    }
}

/// Writer thread body: append every batch, then write the final overflow attribute and close.
/// Stops at the first append error but still finalizes what was written.
fn writer_loop(
    f: File,
    mut cols: Columns,
    rx: Receiver<Vec<LoggedSample>>,
    shared: Arc<Shared>,
    path: PathBuf,
    g_per_lsb: f64,
) -> Result<(), LogError> {
    let mut written = 0u64;
    let mut err = None;
    for batch in rx {
        if let Err(e) = cols.append(&batch, g_per_lsb) {
            err = Some(e);
            break;
        }
        written += batch.len() as u64;
        shared.samples.store(written, Ordering::SeqCst);
        shared.bytes.store(file_len(&path), Ordering::SeqCst);
    }
    // Release the dataset handles before the file is finalized and closed.
    drop(cols);
    let finalized = (|| -> Result<(), LogError> {
        let overflow = shared.overflow.load(Ordering::SeqCst) as u64;
        f.root().set_attr(ATTR_FIFO_OVERFLOW_COUNT, AttrValue::U64(overflow)).map_err(h5_err)?;
        f.commit().map_err(h5_err)?;
        f.close().map_err(h5_err)?;
        Ok(())
    })();
    shared.bytes.store(file_len(&path), Ordering::SeqCst);
    err.map_or(finalized, Err)
}

impl H5Logger {
    /// Create the file at `path` with empty datasets and the attributes from `meta`, then spawn
    /// the writer thread.
    pub fn start(path: &Path, meta: LogMeta) -> Result<H5Logger, LogError> {
        let bad = || LogError::BadMeta(format!("range code {} is not 1..=3", meta.range_code));
        let range_g = scale::range_g(meta.range_code).ok_or_else(bad)?;
        let g_per_lsb = scale::g_per_lsb(meta.range_code).ok_or_else(bad)?;
        let lsb_per_g = scale::lsb_per_g(meta.range_code).ok_or_else(bad)?;

        let mut b = FileBuilder::new();
        b.set_attr(ATTR_SENSOR, AttrValue::AsciiString("ADXL355".into()));
        b.set_attr(ATTR_UNITS, AttrValue::AsciiString("g".into()));
        b.set_attr(ATTR_ODR_HZ, AttrValue::U64(meta.odr_hz as u64));
        b.set_attr(ATTR_RANGE_G, AttrValue::U64(range_g as u64));
        b.set_attr(ATTR_SCALE_G_PER_LSB, AttrValue::F64(g_per_lsb));
        b.set_attr(ATTR_SCALE_LSB_PER_G, AttrValue::F64(lsb_per_g));
        b.set_attr(ATTR_HPF_CODE, AttrValue::I32(meta.hpf_code.map_or(-1, i32::from)));
        b.set_attr(ATTR_FIFO_OVERFLOW_COUNT, AttrValue::U64(meta.fifo_overflow_count as u64));
        b.set_attr(ATTR_STARTED_UTC, AttrValue::AsciiString(meta.started_utc.clone()));

        let chunk = [CHUNK_LEN as u64];
        let unlimited = [MaxExtent::Unlimited];
        let mut g = b.create_group(GROUP_ACCEL_G);
        for name in [DS_X_G, DS_Y_G, DS_Z_G] {
            g.create_dataset(name).with_f64_data(&[]).with_shape(&[0]).with_maxshape(&unlimited).with_chunks(&chunk);
        }
        b.add_group(g.finish());
        let mut r = b.create_group(GROUP_RAW);
        for name in [DS_RAW_X, DS_RAW_Y, DS_RAW_Z] {
            r.create_dataset(name).with_i32_data(&[]).with_shape(&[0]).with_maxshape(&unlimited).with_chunks(&chunk);
        }
        b.add_group(r.finish());
        b.create_dataset(DS_SAMPLE_INDEX).with_u64_data(&[]).with_shape(&[0]).with_maxshape(&unlimited).with_chunks(&chunk);
        b.write(path).map_err(h5_err)?;

        let f = File::open_rw(path).map_err(h5_err)?;
        let cols = Columns::open(&f)?;
        let shared = Arc::new(Shared::default());
        shared.bytes.store(file_len(path), Ordering::SeqCst);
        shared.overflow.store(meta.fifo_overflow_count, Ordering::SeqCst);
        let (tx, rx) = mpsc::channel();
        let writer = {
            let shared = Arc::clone(&shared);
            let path = path.to_path_buf();
            std::thread::spawn(move || writer_loop(f, cols, rx, shared, path, g_per_lsb))
        };
        Ok(H5Logger { path: path.to_path_buf(), shared, tx: Some(tx), writer: Some(writer) })
    }

    /// Queue samples for the writer; never blocks. `Closed` when the writer has ended.
    pub fn push(&self, samples: Vec<LoggedSample>) -> Result<(), LogError> {
        let tx = self.tx.as_ref().ok_or(LogError::Closed)?;
        tx.send(samples).map_err(|_| LogError::Closed)
    }

    /// Record the latest FIFO overflow count (written to the attribute on stop).
    pub fn set_overflow_count(&self, count: u32) {
        self.shared.overflow.store(count, Ordering::SeqCst);
    }

    /// Samples appended to the file so far.
    pub fn samples_written(&self) -> u64 {
        self.shared.samples.load(Ordering::SeqCst)
    }

    /// File length in bytes after the last append.
    pub fn bytes_written(&self) -> u64 {
        self.shared.bytes.load(Ordering::SeqCst)
    }

    /// File path.
    pub fn path(&self) -> &Path {
        &self.path
    }

    /// Close the queue and join the writer; idempotent (used by `stop` and `Drop`).
    fn finish(&mut self) -> Result<(), LogError> {
        self.tx.take();
        match self.writer.take() {
            Some(handle) => handle.join().unwrap_or_else(|_| Err(LogError::Hdf5("writer thread panicked".into()))),
            None => Ok(()),
        }
    }

    /// Drain, write the final attributes, close the file and join the writer.
    pub fn stop(mut self) -> Result<LogSummary, LogError> {
        self.finish()?;
        Ok(LogSummary {
            samples: self.shared.samples.load(Ordering::SeqCst),
            bytes: file_len(&self.path),
            path: self.path.clone(),
        })
    }
}

impl Drop for H5Logger {
    /// Best-effort finalize when `stop` was not called: queued samples are written, the overflow
    /// attribute is set and the file closed; errors are discarded.
    fn drop(&mut self) {
        let _ = self.finish();
    }
}
