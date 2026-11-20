//! PR guards for the pico355-hmi binary arguments (green on the skeleton: main.rs is real).
use std::process::Command;

fn run(args: &[&str]) -> (std::process::ExitStatus, String) {
    let out = Command::new(env!("CARGO_BIN_EXE_pico355-hmi")).args(args).output().expect("run bin");
    let text = String::from_utf8_lossy(&out.stdout).to_string() + &String::from_utf8_lossy(&out.stderr);
    (out.status, text)
}

#[test]
fn help_mentions_cli() {
    let (st, text) = run(&["--help"]);
    assert!(st.success());
    assert!(text.contains("--cli"), "output: {text}");
}

#[test]
fn cli_without_port_exits_2() {
    let (st, _) = run(&["--cli"]);
    assert_eq!(st.code(), Some(2));
}

#[test]
fn cli_with_missing_port_fails_with_open_error() {
    let (st, text) = run(&["--cli", "--port", "/nonexistent/pico355-port"]);
    assert!(!st.success());
    assert!(text.to_lowercase().contains("cannot open"), "output: {text}");
}

#[test]
fn unknown_argument_fails() {
    let (st, _) = run(&["--definitely-not-an-option"]);
    assert!(!st.success());
}
