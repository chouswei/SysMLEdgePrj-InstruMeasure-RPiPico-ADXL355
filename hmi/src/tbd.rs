//! TBD constants of the HMI that stay open. The settled wire-protocol and serial constants
//! moved to `config::link` (design decision 2026-11-19, owner go t37u); what is left here is
//! still TBD: log format, HMI form beyond the plain text view, port discovery, refresh rate.
//!
//! Requirements: PICO355-R8, PICO355-R9 (HMI).
//!
//! TBD rule (mirror of config.h `CFG_TBD` / `CFG_IS_TBD`): a not-yet-decided value is `None`.
//! Code that needs a `None` value returns its `Tbd` error before doing any work.

/// Default output data rate in Hz: confirmed, PICO355-R1 (model defaultStreamRate 4000 Hz).
pub const DEFAULT_ODR_HZ: u32 = 4000;

// ---- HMI TBDs ----
/// TODO(TBD) HMI form (GUI or CLI): placeholder default is a plain terminal text view.
pub const HMI_FORM: &str = "terminal-text"; // TODO(TBD) HMI form placeholder
/// TODO(TBD) log format and storage: placeholder name only, no layout decided (`None`).
pub const LOG_FORMAT: Option<&str> = None; // TODO(TBD) log format
/// TODO(TBD) port discovery: none; the user passes the port path.
pub const PORT_DISCOVERY: bool = false; // TODO(TBD) port discovery
/// TODO(TBD) live view refresh rate (model LiveDataView); `None` = not decided.
pub const LIVE_VIEW_REFRESH_HZ: Option<u32> = None; // TODO(TBD) display form and refresh rate
