//! PH route checks for the pico355-hmi binary (hmi/src/main.rs).
use std::process::Command;

fn bin() -> Command {
    Command::new(env!("CARGO_BIN_EXE_pico355-hmi"))
}

#[test]
fn help_exits_zero_and_mentions_pico355() {
    let out = bin().arg("--help").output().expect("run bin");
    assert!(out.status.success(), "status: {:?}", out.status);
    let text = String::from_utf8_lossy(&out.stdout).to_string()
        + &String::from_utf8_lossy(&out.stderr);
    assert!(text.contains("pico355"), "output: {text}");
}

#[test]
fn unknown_argument_exits_nonzero() {
    let out = bin().arg("--definitely-not-an-option").output().expect("run bin");
    assert!(!out.status.success());
}
