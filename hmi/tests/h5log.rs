//! PR red tests for hmi/src/h5log.rs: write a real .h5, then verify it with h5py through
//! tools/checks/h5verify.py. Python: env PICO355_H5PY_PYTHON, default
//! /workspace/pico355/hmi-gui/h5venv/bin/python. If that python is absent the verify step prints
//! SKIP to stderr and passes (CI has no h5py); if present, a non-zero exit MUST fail the test.
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::{Duration, Instant};

use pico355_hmi::h5log::*;
use pico355_hmi::protocol::Sample;

const DEFAULT_PY: &str = "/workspace/pico355/hmi-gui/h5venv/bin/python";

fn tmp_path(name: &str) -> PathBuf {
    let dir = std::env::temp_dir().join(format!("pico355-h5log-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    dir.join(name)
}

fn meta() -> LogMeta {
    LogMeta { odr_hz: 4000, range_code: 2, hpf_code: Some(3), fifo_overflow_count: 0, started_utc: "2026-11-20T07:55:03Z".into() }
}

fn sample(i: u64) -> LoggedSample {
    let v = (i as i32 % 2000) * 100 - 100_000;
    LoggedSample { index: i, raw: Sample { x: v, y: -v, z: i as i32 } }
}

fn batch(from: u64, n: u64) -> Vec<LoggedSample> {
    (from..from + n).map(sample).collect()
}

/// Run h5verify.py; `None` = python absent (SKIP printed), `Some(success)` otherwise.
fn verify(path: &Path, n: u64, range: u8, odr: u32, hpf: i32, overflow: u32) -> Option<bool> {
    let py = std::env::var("PICO355_H5PY_PYTHON").unwrap_or_else(|_| DEFAULT_PY.to_string());
    if !Path::new(&py).exists() {
        eprintln!("SKIP: h5py python not found at {py}; h5 verification not run");
        return None;
    }
    let script = Path::new(env!("CARGO_MANIFEST_DIR")).join("../tools/checks/h5verify.py");
    let out = Command::new(&py)
        .arg("-I")
        .arg(&script)
        .arg(path)
        .arg(n.to_string())
        .arg(range.to_string())
        .arg(odr.to_string())
        .arg(hpf.to_string())
        .arg(overflow.to_string())
        .output()
        .expect("run h5verify.py");
    eprintln!("h5verify: {} {}", String::from_utf8_lossy(&out.stdout), String::from_utf8_lossy(&out.stderr));
    Some(out.status.success())
}

fn wait_written(lg: &H5Logger, n: u64) {
    let t0 = Instant::now();
    while lg.samples_written() < n && t0.elapsed() < Duration::from_secs(10) {
        std::thread::sleep(Duration::from_millis(10));
    }
}

#[test]
fn constants_and_schema_names() {
    assert_eq!(CHUNK_LEN, 4096);
    assert_eq!((GROUP_ACCEL_G, DS_X_G, DS_Y_G, DS_Z_G), ("accel_g", "x_g", "y_g", "z_g"));
    assert_eq!((GROUP_RAW, DS_RAW_X, DS_SAMPLE_INDEX), ("raw_counts", "x", "sample_index"));
    assert_eq!(ATTR_SCALE_LSB_PER_G, "scale_lsb_per_g");
    assert_eq!(ATTR_FIFO_OVERFLOW_COUNT, "fifo_overflow_count");
    assert_eq!(ATTR_STARTED_UTC, "started_utc");
}

#[test]
fn write_stop_summary_and_h5py_verify() {
    let path = tmp_path("main.h5");
    let _ = std::fs::remove_file(&path);
    let lg = H5Logger::start(&path, meta()).expect("start");
    assert_eq!(lg.path(), path.as_path());
    let mut last = 0u64;
    for k in 0..10u64 {
        lg.push(batch(k * 1000, 1000)).expect("push");
        let w = lg.samples_written();
        assert!(w >= last, "samples_written must be monotone: {w} < {last}");
        last = w;
    }
    lg.set_overflow_count(5);
    let sum = lg.stop().expect("stop");
    assert_eq!(sum.samples, 10_000);
    assert_eq!(sum.path, path);
    let len = std::fs::metadata(&path).expect("file exists").len();
    assert!(len > 0);
    assert_eq!(sum.bytes, len, "summary bytes == file length");
    if let Some(ok) = verify(&path, 10_000, 2, 4000, 3, 5) {
        assert!(ok, "h5verify.py failed");
    }
}

#[test]
fn samples_written_monotone_and_reaches_pushed() {
    let path = tmp_path("mono.h5");
    let lg = H5Logger::start(&path, meta()).expect("start");
    lg.push(batch(0, 5000)).unwrap();
    lg.push(batch(5000, 5000)).unwrap();
    let mut prev = 0;
    let t0 = Instant::now();
    while t0.elapsed() < Duration::from_secs(10) {
        let w = lg.samples_written();
        assert!(w >= prev);
        prev = w;
        if w == 10_000 {
            break;
        }
        std::thread::sleep(Duration::from_millis(5));
    }
    assert_eq!(prev, 10_000);
    assert!(lg.bytes_written() > 0);
    lg.stop().unwrap();
}

#[test]
fn drop_finalizes_the_file() {
    let path = tmp_path("drop.h5");
    let _ = std::fs::remove_file(&path);
    {
        let lg = H5Logger::start(&path, meta()).expect("start");
        lg.push(batch(0, 3000)).unwrap();
        wait_written(&lg, 3000);
        // dropped without stop
    }
    assert!(std::fs::metadata(&path).expect("file exists").len() > 0);
    if let Some(ok) = verify(&path, 3000, 2, 4000, 3, 0) {
        assert!(ok, "file written before Drop must be a valid h5 file");
    }
}

#[test]
fn bad_meta_and_bad_path_are_errors() {
    let mut m = meta();
    m.range_code = 0;
    assert!(matches!(H5Logger::start(&tmp_path("badmeta.h5"), m), Err(LogError::BadMeta(_))));
    let bad = Path::new("/nonexistent-dir-pico355/x.h5");
    assert!(H5Logger::start(bad, meta()).is_err());
}

#[test]
fn no_hpf_is_minus_one() {
    let path = tmp_path("nohpf.h5");
    let mut m = meta();
    m.hpf_code = None;
    m.range_code = 3;
    m.odr_hz = 1000;
    let lg = H5Logger::start(&path, m).unwrap();
    lg.push(batch(0, 100)).unwrap();
    let sum = lg.stop().unwrap();
    assert_eq!(sum.samples, 100);
    if let Some(ok) = verify(&path, 100, 3, 1000, -1, 0) {
        assert!(ok, "h5verify.py failed");
    }
}

/// The verify step must bite: a wrong expected value must give a non-zero exit.
#[test]
fn verify_step_bites_on_wrong_expected_value() {
    let path = tmp_path("bite.h5");
    let lg = H5Logger::start(&path, meta()).unwrap();
    lg.push(batch(0, 500)).unwrap();
    lg.stop().unwrap();
    if let Some(ok) = verify(&path, 501, 2, 4000, 3, 0) {
        assert!(!ok, "h5verify.py accepted a wrong sample count");
    }
}
