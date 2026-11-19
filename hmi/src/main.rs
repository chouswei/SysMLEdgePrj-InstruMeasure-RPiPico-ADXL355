//! pico355-hmi binary: argument parsing and wiring (PICO355-R8, PICO355-R9).
//! HMI form TODO(TBD): placeholder is a plain terminal text view.
//! Log destination and format TODO(TBD): no log flag in the CLI contract yet, so samples go to a sink.

use std::io::{self, Write};
use std::process::ExitCode;

use pico355_hmi::config::HmiConfig;
use pico355_hmi::link::{SerialTransport, Transport};
use pico355_hmi::logger::Logger;
use pico355_hmi::protocol::{decode_frame, encode_command, Command, Frame, ProtocolError};
use pico355_hmi::{live_view, tbd};

const USAGE: &str = "usage: pico355-hmi [--port <path>] [--odr <hz>] [--help]";

/// Local read chunk size for the serial transport (an implementation buffer, not a protocol value).
const READ_CHUNK: usize = 4096;

fn main() -> ExitCode {
    let cfg = match parse_args(std::env::args().skip(1)) {
        Ok(Some(cfg)) => cfg,
        Ok(None) => return ExitCode::SUCCESS,
        Err(code) => return code,
    };
    let Some(port) = cfg.port.clone() else {
        return usage_error("--port is required (port discovery TODO(TBD))");
    };
    let mut link = match SerialTransport::open(&port) {
        Ok(link) => link,
        Err(e) => return fail(&format!("cannot open {port}: {e}")),
    };
    let stdout = io::stdout();
    let mut out = stdout.lock();
    let mut logger = Logger::new(io::sink());
    match run(&mut link, &cfg, &mut out, &mut logger) {
        Ok(()) => ExitCode::SUCCESS,
        Err(msg) => fail(&msg),
    }
}

/// Parse the arguments. `Ok(None)` = help was printed; `Err` = exit code to return.
fn parse_args(args: impl Iterator<Item = String>) -> Result<Option<HmiConfig>, ExitCode> {
    let mut cfg = HmiConfig::default();
    let mut args = args;
    while let Some(a) = args.next() {
        match a.as_str() {
            "-h" | "--help" => {
                println!("{USAGE}");
                return Ok(None);
            }
            "--port" => match args.next() {
                Some(p) => cfg.port = Some(p),
                None => return Err(usage_error("--port needs a value")),
            },
            "--odr" => match args.next().and_then(|v| v.parse().ok()) {
                Some(hz) => cfg.odr_hz = hz,
                None => return Err(usage_error("--odr needs a number")),
            },
            other => return Err(usage_error(&format!("unknown argument: {other}"))),
        }
    }
    Ok(Some(cfg))
}

/// Open-link flow: apply settings, then feed received sample batches to the live view and the logger.
fn run<T: Transport, W: Write, L: Write>(
    link: &mut T,
    cfg: &HmiConfig,
    out: &mut W,
    logger: &mut Logger<L>,
) -> Result<(), String> {
    apply_settings(link, cfg)?;
    let mut pending: Vec<u8> = Vec::new();
    let mut chunk = vec![0u8; READ_CHUNK];
    let mut log_ok = true;
    loop {
        let n = link.read(&mut chunk).map_err(|e| format!("read failed: {e}"))?;
        pending.extend_from_slice(&chunk[..n]);
        loop {
            match decode_frame(&pending) {
                Ok((frame, used)) => {
                    pending.drain(..used);
                    match frame {
                        Frame::SampleBatch(samples) => {
                            live_view::render(out, &samples)
                                .map_err(|e| format!("live view failed: {e}"))?;
                            if log_ok {
                                if let Err(e) = logger.log_batch(&samples) {
                                    eprintln!("pico355-hmi: logging stopped: {e}");
                                    log_ok = false;
                                }
                            }
                        }
                        Frame::Response(resp) => {
                            writeln!(out, "response: {resp:?}")
                                .map_err(|e| format!("output failed: {e}"))?;
                        }
                    }
                }
                Err(ProtocolError::Short) => break,
                Err(ProtocolError::Sync(skip)) => {
                    let skip = skip.max(1).min(pending.len());
                    pending.drain(..skip);
                }
                Err(e) => return Err(format!("protocol error: {e}")),
            }
        }
    }
}

/// Send the configured settings. Only the ODR has a decided command code path; range and filter
/// have no decided command encoding yet, so they stop here with the TBD error.
fn apply_settings<T: Transport>(link: &mut T, cfg: &HmiConfig) -> Result<(), String> {
    if cfg.range.is_some() || cfg.filter.is_some() {
        return Err("range and filter commands TBD: no command encoding decided".to_string());
    }
    let code = tbd::PROTO_CMD_SET_ODR
        .ok_or_else(|| "command set TBD (tbd::PROTO_CMD_SET_ODR): cannot set output data rate".to_string())?;
    let frame = encode_command(&Command { code, value: cfg.odr_hz })
        .map_err(|e| format!("encode set-ODR command: {e}"))?;
    link.write_all(&frame).map_err(|e| format!("write failed: {e}"))
}

fn usage_error(msg: &str) -> ExitCode {
    eprintln!("pico355-hmi: {msg}\n{USAGE}");
    ExitCode::from(2)
}

fn fail(msg: &str) -> ExitCode {
    eprintln!("pico355-hmi: {msg}");
    ExitCode::FAILURE
}
