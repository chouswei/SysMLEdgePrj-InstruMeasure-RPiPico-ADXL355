//! Native desktop window (model ConfigUi + LiveDataView, PICO355-R8, PICO355-R9): eframe/egui
//! with the glow backend, stacked X/Y/Z charts in g (`egui_plot`), port list, link controls,
//! status and HDF5 logging.
//!
//! Design decisions (owner t43s2): chart windows 2/5/10 s, status poll every 1000 ms,
//! repaint every 25 ms, and ODR/range/high-pass locked while logging so the file's scale and
//! attributes stay true. The chart vertical axes are in g (`axis_label`), never raw counts.
//! TBD: native file picker (the log path is an editable text field), log rotation.

use eframe::egui;
// Charts use egui_plot; the skeleton only links it.
use egui_plot as _;

/// Chart window choices in seconds (design decision (owner t43s2)).
pub const WINDOW_CHOICES_S: [u32; 3] = [2, 5, 10];
/// ODR choices in Hz for `CMD_SET_ODR`: the firmware driver's accepted values (docs/protocol.md, `CMD_SET_ODR` row).
pub const ODR_CHOICES_HZ: [u32; 6] = [4000, 2000, 1000, 500, 250, 125];
/// Range choices: firmware range code and label (adxl355.h:169-174, see `scale`).
pub const RANGE_CHOICES: [(u8, &str); 3] = [(1, "2 g"), (2, "4 g"), (3, "8 g")];
/// Highest high-pass code for `CMD_SET_FILTER` (`config::link`: code 0..6).
pub const HPF_CODE_MAX: u8 = 6;
/// `CMD_GET_STATUS` poll interval in ms (design decision (owner t43s2)).
pub const POLL_INTERVAL_MS: u64 = 1000;
/// Repaint interval in ms (design decision (owner t43s2)).
pub const REPAINT_INTERVAL_MS: u64 = 25;

/// Controls whose enabled state depends on logging.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Control {
    Port,
    Odr,
    Range,
    HpfApply,
}

/// Chart vertical axis label for axis 0, 1, 2: "X (g)", "Y (g)", "Z (g)".
pub fn axis_label(axis: usize) -> &'static str {
    let _ = axis;
    todo!()
}

/// UTC civil time (year, month, day, hour, minute, second) from Unix seconds.
pub fn civil_from_unix(secs: i64) -> (i32, u32, u32, u32, u32, u32) {
    let _ = secs;
    todo!()
}

/// Default log file name, UTC: `pico355_YYYYMMDD_HHMMSS.h5`.
pub fn default_log_filename(secs: i64) -> String {
    let _ = secs;
    todo!()
}

/// UTC start time string `YYYY-MM-DDTHH:MM:SSZ` (the `started_utc` attribute).
pub fn started_utc_string(secs: i64) -> String {
    let _ = secs;
    todo!()
}

/// True when `control` is disabled: Odr, Range and HpfApply while logging; Port never
/// (design decision (owner t43s2)).
pub fn control_locked(control: Control, logging: bool) -> bool {
    let _ = (control, logging);
    todo!()
}

/// GUI state.
pub struct HmiApp {
    initial_port: Option<String>,
}

impl HmiApp {
    /// New app; `initial_port` preselects a port path (from `--port`).
    pub fn new(initial_port: Option<String>) -> HmiApp {
        HmiApp { initial_port }
    }
}

impl eframe::App for HmiApp {
    /// eframe 0.37 entry point (`ui`, not `update`). Skeleton: one label.
    fn ui(&mut self, ui: &mut egui::Ui, _frame: &mut eframe::Frame) {
        egui::CentralPanel::default().show(ui, |ui| {
            ui.label(format!(
                "pico355-hmi (skeleton), port: {}",
                self.initial_port.as_deref().unwrap_or("none")
            ));
        });
    }
}

/// Open the native window and run until it closes.
pub fn run(initial_port: Option<String>) -> Result<(), String> {
    eframe::run_native(
        "pico355-hmi",
        eframe::NativeOptions::default(),
        Box::new(move |_cc| Ok(Box::new(HmiApp::new(initial_port)))),
    )
    .map_err(|e| format!("window failed: {e}"))
}
