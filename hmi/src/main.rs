//! pico355-hmi binary: argument parsing and wiring (PICO355-R8, PICO355-R9).
//! HMI form TODO(TBD): placeholder is a plain terminal text view.

use std::process::ExitCode;

use pico355_hmi::config::HmiConfig;

const USAGE: &str = "usage: pico355-hmi [--port <path>] [--odr <hz>] [--help]";

fn main() -> ExitCode {
    let mut cfg = HmiConfig::default();
    let mut args = std::env::args().skip(1);
    while let Some(a) = args.next() {
        match a.as_str() {
            "-h" | "--help" => {
                println!("{USAGE}");
                return ExitCode::SUCCESS;
            }
            "--port" => match args.next() {
                Some(p) => cfg.port = Some(p),
                None => return usage_error("--port needs a value"),
            },
            "--odr" => match args.next().and_then(|v| v.parse().ok()) {
                Some(hz) => cfg.odr_hz = hz,
                None => return usage_error("--odr needs a number"),
            },
            other => return usage_error(&format!("unknown argument: {other}")),
        }
    }
    eprintln!("pico355-hmi: not implemented yet (config: {cfg:?})");
    ExitCode::FAILURE
}

fn usage_error(msg: &str) -> ExitCode {
    eprintln!("pico355-hmi: {msg}\n{USAGE}");
    ExitCode::from(2)
}
