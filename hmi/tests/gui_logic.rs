//! PR red tests for the pure helpers and constants of hmi/src/gui.rs. No window is opened.
use pico355_hmi::gui::*;

#[test]
fn constants_equal_contract() {
    assert_eq!(WINDOW_CHOICES_S, [2, 5, 10]);
    assert_eq!(ODR_CHOICES_HZ, [4000, 2000, 1000, 500, 250, 125]);
    assert_eq!(RANGE_CHOICES, [(1, "2 g"), (2, "4 g"), (3, "8 g")]);
    assert_eq!(HPF_CODE_MAX, 6);
    assert_eq!(POLL_INTERVAL_MS, 1000);
    assert_eq!(REPAINT_INTERVAL_MS, 25);
}

#[test]
fn axis_labels_are_in_g() {
    assert_eq!(axis_label(0), "X (g)");
    assert_eq!(axis_label(1), "Y (g)");
    assert_eq!(axis_label(2), "Z (g)");
}

/// Independent method: days-from-civil (proleptic Gregorian) used as the inverse, then the
/// round trip is checked against `civil_from_unix`.
fn days_from_civil(y: i64, m: i64, d: i64) -> i64 {
    let y = if m <= 2 { y - 1 } else { y };
    let era = y.div_euclid(400);
    let yoe = y - era * 400;
    let mp = (m + 9) % 12;
    let doy = (153 * mp + 2) / 5 + d - 1;
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    era * 146_097 + doe - 719_468
}

fn unix_from_civil(y: i64, mo: i64, d: i64, h: i64, mi: i64, s: i64) -> i64 {
    days_from_civil(y, mo, d) * 86_400 + h * 3600 + mi * 60 + s
}

#[test]
fn civil_from_unix_epoch() {
    assert_eq!(civil_from_unix(0), (1970, 1, 1, 0, 0, 0));
}

#[test]
fn civil_from_unix_known_dates() {
    for (y, mo, d, h, mi, s) in [
        (2000, 2, 29, 23, 59, 59),
        (2024, 12, 31, 0, 0, 1),
        (2026, 11, 20, 7, 55, 3),
        (1999, 3, 1, 12, 0, 0),
        (2100, 3, 1, 0, 0, 0),
    ] {
        let secs = unix_from_civil(y, mo, d, h, mi, s);
        assert_eq!(
            civil_from_unix(secs),
            (y as i32, mo as u32, d as u32, h as u32, mi as u32, s as u32),
            "secs {secs}"
        );
    }
    // 1_000_000_000 is 2001-09-09 01:46:40 UTC (well-known value)
    assert_eq!(civil_from_unix(1_000_000_000), (2001, 9, 9, 1, 46, 40));
}

#[test]
fn default_log_filename_shape() {
    assert_eq!(default_log_filename(0), "pico355_19700101_000000.h5");
    let secs = unix_from_civil(2026, 11, 20, 7, 5, 9);
    assert_eq!(default_log_filename(secs), "pico355_20261120_070509.h5");
}

#[test]
fn started_utc_string_shape() {
    assert_eq!(started_utc_string(0), "1970-01-01T00:00:00Z");
    let secs = unix_from_civil(2026, 11, 20, 7, 5, 9);
    assert_eq!(started_utc_string(secs), "2026-11-20T07:05:09Z");
}

#[test]
fn control_locked_table() {
    for c in [Control::Odr, Control::Range, Control::HpfApply] {
        assert!(control_locked(c, true), "{c:?} locked while logging");
        assert!(!control_locked(c, false), "{c:?} free when not logging");
    }
    assert!(!control_locked(Control::Port, true));
    assert!(!control_locked(Control::Port, false));
}
