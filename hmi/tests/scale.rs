//! PR guard checks for hmi/src/scale.rs (real in A1, so these pass). Values only from the cited
//! no-OS lines (pin 4557b1f5: adxl355.h:62-65, :169-174, adxl355.c:18, :920-930); LSB per g is
//! the derived reciprocal.
use pico355_hmi::protocol::Sample;
use pico355_hmi::scale::*;

const TOL: f64 = 1e-12;

#[test]
fn g_per_lsb_per_range() {
    assert!((g_per_lsb(1).unwrap() - 3.9e-6).abs() < TOL);
    assert!((g_per_lsb(2).unwrap() - 7.8e-6).abs() < TOL);
    assert!((g_per_lsb(3).unwrap() - 15.6e-6).abs() < TOL);
}

#[test]
fn invalid_range_codes_are_none() {
    for c in [0u8, 4, 9, 255] {
        assert_eq!(g_per_lsb(c), None);
        assert_eq!(lsb_per_g(c), None);
        assert_eq!(range_g(c), None);
        assert_eq!(raw_to_g(1, c), None);
        assert_eq!(sample_to_g(&Sample { x: 1, y: 2, z: 3 }, c), None);
    }
}

#[test]
fn range_codes_and_full_scale() {
    assert_eq!((RANGE_CODE_2G, RANGE_CODE_4G, RANGE_CODE_8G), (1, 2, 3));
    assert_eq!(range_g(1), Some(2));
    assert_eq!(range_g(2), Some(4));
    assert_eq!(range_g(3), Some(8));
}

#[test]
fn raw_to_g_scales_and_keeps_sign() {
    assert!((raw_to_g(1000, 1).unwrap() - 1000.0 * 3.9e-6).abs() < TOL);
    assert!((raw_to_g(-1000, 1).unwrap() + 1000.0 * 3.9e-6).abs() < TOL);
    assert!(raw_to_g(-1, 2).unwrap() < 0.0);
    assert_eq!(raw_to_g(0, 3), Some(0.0));
}

#[test]
fn lsb_per_g_is_derived_reciprocal() {
    // Derived value, not a cited one: only the product with g_per_lsb is checked.
    for c in 1..=3u8 {
        let p = lsb_per_g(c).unwrap() * g_per_lsb(c).unwrap();
        assert!((p - 1.0).abs() < 1e-9, "code {c}: {p}");
    }
}

#[test]
fn sample_to_g_maps_xyz() {
    let g = sample_to_g(&Sample { x: 1000, y: -2000, z: 3000 }, 2).unwrap();
    assert!((g[0] - 1000.0 * 7.8e-6).abs() < TOL);
    assert!((g[1] + 2000.0 * 7.8e-6).abs() < TOL);
    assert!((g[2] - 3000.0 * 7.8e-6).abs() < TOL);
}

#[test]
fn twenty_bit_extremes_map_without_overflow() {
    let lo = raw_to_g(-524_288, 3).unwrap();
    let hi = raw_to_g(524_287, 3).unwrap();
    assert!((lo + 524_288.0 * 15.6e-6).abs() < TOL);
    assert!((hi - 524_287.0 * 15.6e-6).abs() < TOL);
    let g = sample_to_g(&Sample { x: -524_288, y: 524_287, z: 0 }, 1).unwrap();
    assert!(g[0] < 0.0 && g[1] > 0.0 && g[2] == 0.0);
}
