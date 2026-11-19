//! PH route checks for hmi/src/config.rs: defaults per its doc comments
//! (PICO355-R1 default ODR 4000 Hz; port, range, filter are TBD = None).
use pico355_hmi::config::{HmiConfig, Range, DEFAULT_ODR_HZ};
use pico355_hmi::tbd;

#[test]
fn default_odr_is_4000_and_mirrors_tbd_module() {
    assert_eq!(DEFAULT_ODR_HZ, 4000);
    assert_eq!(DEFAULT_ODR_HZ, tbd::DEFAULT_ODR_HZ);
}

#[test]
fn default_config_values() {
    let c = HmiConfig::default();
    assert_eq!(c.port, None);
    assert_eq!(c.odr_hz, DEFAULT_ODR_HZ);
    assert_eq!(c.range, None);
    assert_eq!(c.filter, None);
}

#[test]
fn config_clone_and_eq() {
    let mut c = HmiConfig::default();
    assert_eq!(c.clone(), c);
    c.range = Some(Range::G4);
    assert_ne!(c, HmiConfig::default());
}
