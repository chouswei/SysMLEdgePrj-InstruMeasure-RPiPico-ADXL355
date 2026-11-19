//! PH route checks for hmi/src/logger.rs while tbd::LOG_FORMAT is None (steer h_005hdDzmBawk):
//! log_batch writes NO bytes and returns Err(io::Error) of ErrorKind::Unsupported whose message
//! names LOG_FORMAT and TBD; open_file may create the file and returns Ok. No format is asserted.
use std::io::ErrorKind;

use pico355_hmi::logger::{open_file, Logger};
use pico355_hmi::protocol::Sample;
use pico355_hmi::tbd;

fn batch() -> Vec<Sample> {
    vec![Sample { x: 1, y: 2, z: 3 }, Sample { x: 4, y: 5, z: 6 }]
}

#[test]
fn log_batch_writes_no_bytes_and_reports_tbd_format() {
    assert!(tbd::LOG_FORMAT.is_none());
    let mut lg = Logger::new(Vec::<u8>::new());
    let err = lg.log_batch(&batch()).expect_err("must be Err while format is TBD");
    assert_eq!(err.kind(), ErrorKind::Unsupported);
    let msg = err.to_string();
    assert!(msg.contains("LOG_FORMAT"), "message: {msg}");
    assert!(msg.contains("TBD"), "message: {msg}");
    assert!(lg.into_inner().is_empty());
}

#[test]
fn repeated_log_batch_still_writes_nothing() {
    let mut lg = Logger::new(Vec::<u8>::new());
    for _ in 0..3 {
        assert!(lg.log_batch(&batch()).is_err());
    }
    assert!(lg.into_inner().is_empty());
}

#[test]
fn open_file_on_temp_path_returns_ok_and_log_batch_still_errors() {
    let path = std::env::temp_dir().join(format!("pico355-ph-logger-{}.log", std::process::id()));
    let mut lg = open_file(&path).expect("open_file ok");
    let err = lg.log_batch(&batch()).expect_err("Err while format TBD");
    assert_eq!(err.kind(), ErrorKind::Unsupported);
    drop(lg);
    let _ = std::fs::remove_file(&path);
}
