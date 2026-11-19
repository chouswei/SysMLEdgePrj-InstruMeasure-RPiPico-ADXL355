//! HMI settings (model ConfigUi / EditSensorConfig, PICO355-R8; defaults per PICO355-R1, PICO355-R2).
//! Filter settings exposed: TODO(TBD). Default range: TODO(TBD) (model AccelRange).

/// Default output data rate (PICO355-R1 defaultStreamRate 4000 Hz), from `tbd.rs`.
pub const DEFAULT_ODR_HZ: u32 = crate::tbd::DEFAULT_ODR_HZ;

/// Measurement range, mirrors model enum AccelRange.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Range {
    G2,
    G4,
    G8,
}

/// HMI settings.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct HmiConfig {
    /// Serial port path given by the user (port discovery TODO(TBD)).
    pub port: Option<String>,
    /// Output data rate in Hz.
    pub odr_hz: u32,
    /// Range; `None` = TODO(TBD) default range.
    pub range: Option<Range>,
    /// Filter setting; `None` = TODO(TBD) filter settings exposed.
    pub filter: Option<u8>,
}

impl Default for HmiConfig {
    fn default() -> Self {
        Self { port: None, odr_hz: DEFAULT_ODR_HZ, range: None, filter: None }
    }
}
