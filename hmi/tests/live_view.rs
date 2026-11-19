//! PH route checks for hmi/src/live_view.rs: render N samples as text to a writer.
//! Form is TODO(TBD) (plain terminal text), so only form-independent properties are checked.
use pico355_hmi::live_view::render;
use pico355_hmi::protocol::Sample;

fn samples() -> Vec<Sample> {
    vec![
        Sample { x: 1, y: -2, z: 3 },
        Sample { x: 40001, y: -50002, z: 60003 },
        Sample { x: 0, y: 0, z: 0 },
    ]
}

#[test]
fn render_writes_valid_text_containing_every_value() {
    let mut out: Vec<u8> = Vec::new();
    render(&mut out, &samples()).expect("render ok");
    let text = String::from_utf8(out).expect("utf-8 text");
    for v in ["40001", "-50002", "60003"] {
        assert!(text.contains(v), "missing {v} in {text:?}");
    }
}

#[test]
fn render_is_deterministic() {
    let (mut a, mut b) = (Vec::new(), Vec::new());
    render(&mut a, &samples()).expect("render a");
    render(&mut b, &samples()).expect("render b");
    assert!(!a.is_empty());
    assert_eq!(a, b);
}

#[test]
fn render_empty_slice_is_ok() {
    let mut out: Vec<u8> = Vec::new();
    render(&mut out, &[]).expect("render ok");
}
