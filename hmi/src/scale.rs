//! Raw count to g conversion for the ADXL355 (PICO355-R9: live view and log in g).
//!
//! Source: Analog Devices no-OS driver, pin 4557b1f58f8a233bd116dbb02d8d5c447d3699b6,
//! `drivers/accel/adxl355/`:
//! - adxl355.h:62-65: at +/-2 g with 20-bit resolution the scale is 3.9 ug/LSB; the 4 g range
//!   uses a multiplier of 2 and the 8 g range a multiplier of 4.
//! - adxl355.h:169-174: `enum adxl355_range` { 2G = 1, 4G = 2, 8G = 3 } (the range code the
//!   firmware takes in `CMD_SET_RANGE`).
//! - adxl355.c:18: `adxl355_scale_mul[4] = {0, 1, 2, 4}`, indexed by the range code.
//! - adxl355.c:920-930: sign extension at bit 19, then raw * scale * mul. The sign extension is
//!   already done by the wire codec (`protocol::Sample` holds sign-extended values), so this
//!   module only multiplies.
//!
//! Any LSB per g value is DERIVED here as the reciprocal of the cited g per LSB; it is not a
//! cited number.

use crate::protocol::Sample;

/// Range code for +/-2 g (adxl355.h:169-174 `ADXL355_RANGE_2G`).
pub const RANGE_CODE_2G: u8 = 1;
/// Range code for +/-4 g (adxl355.h:169-174 `ADXL355_RANGE_4G`).
pub const RANGE_CODE_4G: u8 = 2;
/// Range code for +/-8 g (adxl355.h:169-174 `ADXL355_RANGE_8G`).
pub const RANGE_CODE_8G: u8 = 3;

/// g per LSB at the +/-2 g range: 3.9 ug/LSB (adxl355.h:62-65).
pub const G_PER_LSB_2G: f64 = 3.9e-6;

/// Range multiplier table indexed by range code (adxl355.c:18 `{0, 1, 2, 4}`).
const SCALE_MUL: [u32; 4] = [0, 1, 2, 4];

/// Multiplier for a valid range code (1..=3), `None` otherwise.
fn scale_mul(range_code: u8) -> Option<u32> {
    match range_code {
        RANGE_CODE_2G..=RANGE_CODE_8G => Some(SCALE_MUL[range_code as usize]),
        _ => None,
    }
}

/// g per LSB for a range code: 3.9e-6 x {1, 2, 4} for codes {1, 2, 3}
/// (adxl355.h:62-65, adxl355.c:18). `None` unless the code is 1..=3.
pub fn g_per_lsb(range_code: u8) -> Option<f64> {
    scale_mul(range_code).map(|m| G_PER_LSB_2G * m as f64)
}

/// LSB per g for a range code. DERIVED: the reciprocal of [`g_per_lsb`], not a cited value.
pub fn lsb_per_g(range_code: u8) -> Option<f64> {
    g_per_lsb(range_code).map(|g| 1.0 / g)
}

/// Full-scale range in g for a range code: 2, 4, 8 (adxl355.h:169-174 enum names).
pub fn range_g(range_code: u8) -> Option<u32> {
    match range_code {
        RANGE_CODE_2G => Some(2),
        RANGE_CODE_4G => Some(4),
        RANGE_CODE_8G => Some(8),
        _ => None,
    }
}

/// One sign-extended raw count to g for a range code (adxl355.c:926-930, in g not m/s^2).
pub fn raw_to_g(raw: i32, range_code: u8) -> Option<f64> {
    g_per_lsb(range_code).map(|s| raw as f64 * s)
}

/// One sample to [x, y, z] in g for a range code; `None` for an invalid range code.
pub fn sample_to_g(sample: &Sample, range_code: u8) -> Option<[f64; 3]> {
    let s = g_per_lsb(range_code)?;
    Some([sample.x as f64 * s, sample.y as f64 * s, sample.z as f64 * s])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cited_scale_per_range() {
        assert_eq!(g_per_lsb(1), Some(3.9e-6));
        assert_eq!(g_per_lsb(2), Some(3.9e-6 * 2.0));
        assert_eq!(g_per_lsb(3), Some(3.9e-6 * 4.0));
        assert_eq!(g_per_lsb(0), None);
        assert_eq!(g_per_lsb(4), None);
        assert_eq!(range_g(3), Some(8));
        assert_eq!(raw_to_g(-1, 9), None);
        let g = sample_to_g(&Sample { x: 256_410, y: -256_410, z: 0 }, 1).unwrap();
        assert!((g[0] - 1.0).abs() < 1e-5 && (g[1] + 1.0).abs() < 1e-5 && g[2] == 0.0);
        assert!((lsb_per_g(1).unwrap() * 3.9e-6 - 1.0).abs() < 1e-12);
    }
}
