//! Native desktop window (model ConfigUi + LiveDataView, PICO355-R8, PICO355-R9): eframe/egui
//! with the glow backend, stacked X/Y/Z charts in g (`egui_plot`), port list, link controls,
//! status and HDF5 logging.
//!
//! Design decisions (owner t43s2): chart windows 2/5/10 s, status poll every 1000 ms,
//! repaint every 25 ms, and ODR/range/high-pass locked while logging so the file's scale and
//! attributes stay true. The chart vertical axes are in g (`axis_label`), never raw counts.
//! TBD: native file picker (the log path is an editable text field), log rotation.

use std::collections::VecDeque;
use std::path::Path;
use std::time::{Instant, SystemTime, UNIX_EPOCH};

use eframe::egui;
use egui::Color32;
use egui_plot::{Line, Plot, PlotPoints};

use crate::config::link;
use crate::h5log::{H5Logger, LogMeta, LoggedSample};
use crate::samples::{EnvPoint, SampleStore, DEFAULT_CAPACITY};
use crate::scale;
use crate::worker::{self, PortInfo, Worker, WorkerCommand, WorkerEvent};

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
    match axis {
        0 => "X (g)",
        1 => "Y (g)",
        _ => "Z (g)",
    }
}

/// Single trace color for axis 0, 1, 2 (X, Y, Z): crisp red, green, blue. Any other index
/// falls back to the Z color (matching `axis_label`).
pub fn axis_color(axis: usize) -> Color32 {
    match axis {
        0 => Color32::from_rgb(230, 40, 40),
        1 => Color32::from_rgb(20, 170, 60),
        _ => Color32::from_rgb(40, 90, 240),
    }
}

/// One continuous oscilloscope polyline from min/max buckets: a bucket with `min_g == max_g`
/// gives one point `[t_s, min_g]`, otherwise `[t_s, min_g]` then `[t_s, max_g]`.
pub fn envelope_trace(env: &[EnvPoint]) -> Vec<[f64; 2]> {
    let mut out = Vec::with_capacity(env.len() * 2);
    for p in env {
        out.push([p.t_s, p.min_g]);
        if p.min_g != p.max_g {
            out.push([p.t_s, p.max_g]);
        }
    }
    out
}

/// UTC civil time (year, month, day, hour, minute, second) from Unix seconds.
pub fn civil_from_unix(secs: i64) -> (i32, u32, u32, u32, u32, u32) {
    let days = secs.div_euclid(86_400);
    let sod = secs.rem_euclid(86_400);
    // civil-from-days (proleptic Gregorian), days since 1970-01-01
    let z = days + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    let y = yoe + era * 400 + if m <= 2 { 1 } else { 0 };
    (
        y as i32,
        m as u32,
        d as u32,
        (sod / 3600) as u32,
        ((sod % 3600) / 60) as u32,
        (sod % 60) as u32,
    )
}

/// Default log file name, UTC: `pico355_YYYYMMDD_HHMMSS.h5`.
pub fn default_log_filename(secs: i64) -> String {
    let (y, mo, d, h, mi, s) = civil_from_unix(secs);
    format!("pico355_{y:04}{mo:02}{d:02}_{h:02}{mi:02}{s:02}.h5")
}

/// UTC start time string `YYYY-MM-DDTHH:MM:SSZ` (the `started_utc` attribute).
pub fn started_utc_string(secs: i64) -> String {
    let (y, mo, d, h, mi, s) = civil_from_unix(secs);
    format!("{y:04}-{mo:02}-{d:02}T{h:02}:{mi:02}:{s:02}Z")
}

/// True when `control` is disabled: Odr, Range and HpfApply while logging; Port never
/// (design decision (owner t43s2)).
pub fn control_locked(control: Control, logging: bool) -> bool {
    match control {
        Control::Port => false,
        Control::Odr | Control::Range | Control::HpfApply => logging,
    }
}

/// Settings commands sent and not yet ACKed/NACKed, one FIFO per command code. The firmware
/// ACK carries value 0 (firmware/src/command.c:142), so an ACKed setting is the value that was
/// sent, taken oldest-first, never `Ack.value`.
#[derive(Debug, Default)]
pub struct PendingSettings {
    odr: VecDeque<u32>,
    range: VecDeque<u32>,
    filter: VecDeque<u32>,
}

impl PendingSettings {
    pub fn new() -> PendingSettings {
        PendingSettings::default()
    }

    fn queue(&mut self, code: u8) -> Option<&mut VecDeque<u32>> {
        match code {
            link::CMD_SET_ODR => Some(&mut self.odr),
            link::CMD_SET_RANGE => Some(&mut self.range),
            link::CMD_SET_FILTER => Some(&mut self.filter),
            _ => None,
        }
    }

    /// Record a sent command; `GetStatus` is not recorded.
    pub fn record(&mut self, cmd: &WorkerCommand) {
        match cmd {
            WorkerCommand::SetOdr(v) => self.odr.push_back(*v),
            WorkerCommand::SetRange(v) => self.range.push_back(*v as u32),
            WorkerCommand::SetFilter(v) => self.filter.push_back(*v as u32),
            WorkerCommand::GetStatus => {}
        }
    }

    /// Oldest sent value for `cmd_code` (on ACK), or None.
    pub fn take_acked(&mut self, cmd_code: u8) -> Option<u32> {
        self.queue(cmd_code).and_then(|q| q.pop_front())
    }

    /// Discard the oldest sent value for `cmd_code` (on NACK).
    pub fn drop_nacked(&mut self, cmd_code: u8) {
        if let Some(q) = self.queue(cmd_code) {
            q.pop_front();
        }
    }
}

fn now_unix() -> i64 {
    SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_secs() as i64).unwrap_or(0)
}

/// Plot points per chart (envelope buckets).
const PLOT_BUCKETS: usize = 800;
/// Default range code and ODR sent on connect (design decision (owner t43s2)).
const DEFAULT_RANGE_CODE: u8 = scale::RANGE_CODE_2G;
const DEFAULT_ODR_HZ_CONNECT: u32 = 4000;

/// GUI state.
pub struct HmiApp {
    initial_port: Option<String>,
    ports: Vec<PortInfo>,
    selected_port: Option<String>,
    custom_path: String,
    worker: Option<Worker>,
    store: SampleStore,
    started: Instant,
    window_idx: usize,
    paused: bool,
    fixed_y: bool,
    sel_odr: u32,
    sel_range: u8,
    sel_hpf: u8,
    /// ODR used to time-stamp incoming samples (last ACK, else the connect default).
    active_odr: u32,
    /// Range used for the g scale (last ACK, else the selected range at connect).
    active_range: u8,
    range_acked: bool,
    hpf_applied: Option<u8>,
    last_msg: String,
    overflow: Option<u32>,
    last_poll: Option<Instant>,
    rx_index: u64,
    logger: Option<H5Logger>,
    log_path: String,
    log_msg: String,
    wake_ctx: Option<egui::Context>,
    pending: PendingSettings,
}

impl HmiApp {
    /// New app; `initial_port` preselects a port path (from `--port`).
    pub fn new(initial_port: Option<String>) -> HmiApp {
        let ports = worker::list_ports();
        HmiApp {
            selected_port: initial_port.clone(),
            initial_port,
            ports,
            custom_path: String::new(),
            worker: None,
            store: SampleStore::new(DEFAULT_CAPACITY),
            started: Instant::now(),
            window_idx: 1,
            paused: false,
            fixed_y: false,
            sel_odr: DEFAULT_ODR_HZ_CONNECT,
            sel_range: DEFAULT_RANGE_CODE,
            sel_hpf: 0,
            active_odr: DEFAULT_ODR_HZ_CONNECT,
            active_range: DEFAULT_RANGE_CODE,
            range_acked: false,
            hpf_applied: None,
            last_msg: String::from("-"),
            overflow: None,
            last_poll: None,
            rx_index: 0,
            logger: None,
            log_path: default_log_filename(now_unix()),
            log_msg: String::new(),
            wake_ctx: None,
            pending: PendingSettings::new(),
        }
    }

    fn connected(&self) -> bool {
        self.worker.is_some()
    }

    fn target_path(&self) -> Option<String> {
        let c = self.custom_path.trim();
        if !c.is_empty() {
            Some(c.to_string())
        } else {
            self.selected_port.clone()
        }
    }

    fn send(&mut self, cmd: WorkerCommand) {
        if let Some(w) = &self.worker {
            self.pending.record(&cmd);
            if w.send(cmd).is_err() {
                self.last_msg = String::from("ERR: worker ended");
            }
        }
    }

    fn connect(&mut self) {
        let Some(path) = self.target_path() else {
            self.last_msg = String::from("ERR: no port selected");
            return;
        };
        let ctx = self.wake_ctx.clone();
        let wake: Option<Box<dyn Fn() + Send + 'static>> =
            ctx.map(|c| Box::new(move || c.request_repaint()) as Box<dyn Fn() + Send + 'static>);
        match Worker::connect_serial(&path, wake) {
            Ok(w) => {
                self.worker = Some(w);
                self.store.clear();
                self.rx_index = 0;
                self.overflow = None;
                self.hpf_applied = None;
                self.pending = PendingSettings::new();
                self.range_acked = false;
                self.active_range = self.sel_range;
                self.active_odr = self.sel_odr;
                self.last_msg = format!("connected {path}");
                // Design decision (owner t43s2): SetRange and SetOdr on connect, never SetFilter.
                self.send(WorkerCommand::SetRange(self.sel_range));
                self.send(WorkerCommand::SetOdr(self.sel_odr));
                self.send(WorkerCommand::GetStatus);
                self.last_poll = Some(Instant::now());
            }
            Err(e) => self.last_msg = format!("ERR: connect failed: {e:?}"),
        }
    }

    fn disconnect(&mut self) {
        self.stop_logging();
        if let Some(w) = self.worker.take() {
            w.disconnect();
        }
        self.last_msg = String::from("disconnected");
    }

    fn meta(&self) -> LogMeta {
        LogMeta {
            odr_hz: self.active_odr,
            range_code: self.active_range,
            hpf_code: self.hpf_applied,
            fifo_overflow_count: self.overflow.unwrap_or(0),
            started_utc: started_utc_string(now_unix()),
        }
    }

    fn start_logging(&mut self) {
        let p = self.log_path.trim().to_string();
        if p.is_empty() {
            self.log_msg = String::from("ERR: empty log path");
            return;
        }
        match H5Logger::start(Path::new(&p), self.meta()) {
            Ok(l) => {
                if let Some(o) = self.overflow {
                    l.set_overflow_count(o);
                }
                self.logger = Some(l);
                self.log_msg = format!("logging to {p}");
            }
            Err(e) => self.log_msg = format!("ERR: {e}"),
        }
    }

    fn stop_logging(&mut self) {
        if let Some(l) = self.logger.take() {
            self.log_msg = match l.stop() {
                Ok(s) => format!("saved {} samples, {} bytes: {}", s.samples, s.bytes, s.path.display()),
                Err(e) => format!("ERR: {e}"),
            };
            self.log_path = default_log_filename(now_unix());
        }
    }

    fn handle_events(&mut self) {
        let events = match &self.worker {
            Some(w) => w.drain(),
            None => return,
        };
        let mut gone = false;
        for ev in events {
            match ev {
                WorkerEvent::Samples(batch) => {
                    let first = self.rx_index;
                    self.rx_index += batch.len() as u64;
                    if let Some(l) = &self.logger {
                        let v: Vec<LoggedSample> = batch
                            .iter()
                            .enumerate()
                            .map(|(i, s)| LoggedSample { index: first + i as u64, raw: *s })
                            .collect();
                        if l.push(v).is_err() {
                            self.log_msg = String::from("ERR: log writer ended");
                        }
                    }
                    if !self.paused {
                        let now_s = self.started.elapsed().as_secs_f64();
                        self.store.push_batch(&batch, self.active_range, self.active_odr, now_s);
                    }
                }
                WorkerEvent::Ack { cmd_code, value } => {
                    self.last_msg = format!("ACK cmd 0x{cmd_code:02X} value {value}");
                    // The ACK value is always 0: apply the value that was sent.
                    if let Some(sent) = self.pending.take_acked(cmd_code) {
                        match cmd_code {
                            link::CMD_SET_RANGE => {
                                if scale::range_g(sent as u8).is_some() {
                                    self.active_range = sent as u8;
                                    self.range_acked = true;
                                }
                            }
                            link::CMD_SET_ODR => self.active_odr = sent,
                            link::CMD_SET_FILTER => self.hpf_applied = Some(sent as u8),
                            _ => {}
                        }
                    }
                }
                WorkerEvent::Nack { cmd_code, value } => {
                    self.pending.drop_nacked(cmd_code);
                    self.last_msg = format!("ERR (nack) cmd 0x{cmd_code:02X} value {value}");
                }
                WorkerEvent::Status { overflow_count } => {
                    self.overflow = Some(overflow_count);
                    if let Some(l) = &self.logger {
                        l.set_overflow_count(overflow_count);
                    }
                }
                WorkerEvent::Error(e) => self.last_msg = format!("ERR: {e}"),
                WorkerEvent::Disconnected => gone = true,
            }
        }
        if gone {
            self.stop_logging();
            if let Some(w) = self.worker.take() {
                w.disconnect();
            }
            self.last_msg = String::from("ERR: link lost, disconnected");
        }
    }

    fn poll_status(&mut self) {
        if !self.connected() {
            return;
        }
        let due = self
            .last_poll
            .map(|t| t.elapsed().as_millis() as u64 >= POLL_INTERVAL_MS)
            .unwrap_or(true);
        if due {
            self.send(WorkerCommand::GetStatus);
            self.last_poll = Some(Instant::now());
        }
    }

    fn controls_ui(&mut self, ui: &mut egui::Ui) {
        let logging = self.logger.is_some();
        let connected = self.connected();
        ui.horizontal_wrapped(|ui| {
            ui.add_enabled_ui(!control_locked(Control::Port, logging) && !connected, |ui| {
                let label = self.selected_port.clone().unwrap_or_else(|| String::from("(select port)"));
                egui::ComboBox::from_id_salt("port").selected_text(label).show_ui(ui, |ui| {
                    for p in &self.ports {
                        let text = if p.is_pico {
                            egui::RichText::new(format!("{} {} (Pico)", p.path, p.description))
                                .strong()
                                .color(egui::Color32::from_rgb(40, 160, 60))
                        } else {
                            egui::RichText::new(format!("{} {}", p.path, p.description))
                        };
                        ui.selectable_value(&mut self.selected_port, Some(p.path.clone()), text);
                    }
                });
                if ui.button("Refresh").clicked() {
                    self.ports = worker::list_ports();
                }
                ui.label("custom:");
                ui.add(egui::TextEdit::singleline(&mut self.custom_path).desired_width(110.0));
            });
            if connected {
                if ui.button("Disconnect").clicked() {
                    self.disconnect();
                }
            } else if ui.button("Connect").clicked() {
                self.connect();
            }
        });
        ui.horizontal_wrapped(|ui| {
            ui.label("ODR");
            ui.add_enabled_ui(!control_locked(Control::Odr, logging), |ui| {
                egui::ComboBox::from_id_salt("odr").selected_text(format!("{} Hz", self.sel_odr)).show_ui(ui, |ui| {
                    for hz in ODR_CHOICES_HZ {
                        ui.selectable_value(&mut self.sel_odr, hz, format!("{hz} Hz"));
                    }
                });
                if connected && ui.button("Set ODR").clicked() {
                    self.send(WorkerCommand::SetOdr(self.sel_odr));
                }
            });
            ui.label("Range");
            ui.add_enabled_ui(!control_locked(Control::Range, logging), |ui| {
                let txt = RANGE_CHOICES.iter().find(|(c, _)| *c == self.sel_range).map(|(_, l)| *l).unwrap_or("?");
                egui::ComboBox::from_id_salt("range").selected_text(txt).show_ui(ui, |ui| {
                    for (code, label) in RANGE_CHOICES {
                        ui.selectable_value(&mut self.sel_range, code, label);
                    }
                });
                if connected && ui.button("Set range").clicked() {
                    self.send(WorkerCommand::SetRange(self.sel_range));
                }
            });
            ui.label("High-pass");
            ui.add_enabled_ui(!control_locked(Control::HpfApply, logging), |ui| {
                ui.add(egui::DragValue::new(&mut self.sel_hpf).range(0..=HPF_CODE_MAX));
                if connected && ui.button("Apply").clicked() {
                    self.send(WorkerCommand::SetFilter(self.sel_hpf));
                }
            });
        });
        ui.horizontal_wrapped(|ui| {
            ui.label("Window");
            for (i, s) in WINDOW_CHOICES_S.iter().enumerate() {
                ui.selectable_value(&mut self.window_idx, i, format!("{s} s"));
            }
            ui.checkbox(&mut self.paused, "Pause");
            ui.checkbox(&mut self.fixed_y, "Fixed y (+/- range)");
            ui.label(format!("scale: {} g{}", scale::range_g(self.active_range).unwrap_or(0),
                if self.range_acked || !connected { "" } else { " (assumed, no range ACK yet)" }));
        });
        ui.horizontal_wrapped(|ui| {
            let rate = self.store.measured_rate_hz().map(|r| format!("{r:.0} Hz")).unwrap_or_else(|| String::from("-"));
            ui.label(format!("rate {rate}"));
            ui.label(format!("samples {}", self.rx_index));
            let ov = self.overflow.map(|o| o.to_string()).unwrap_or_else(|| String::from("-"));
            ui.label(format!("FIFO overflow {ov}"));
            if connected && ui.button("Refresh status").clicked() {
                self.send(WorkerCommand::GetStatus);
                self.last_poll = Some(Instant::now());
            }
            ui.label(format!("last: {}", self.last_msg));
        });
        ui.horizontal_wrapped(|ui| {
            ui.label("Log file");
            ui.add_enabled(!logging, egui::TextEdit::singleline(&mut self.log_path).desired_width(240.0));
            if logging {
                if ui.button("Stop logging").clicked() {
                    self.stop_logging();
                }
            } else if ui.button("Start logging").clicked() {
                self.start_logging();
            }
            if let Some(l) = &self.logger {
                ui.label(format!("{} samples, {} bytes", l.samples_written(), l.bytes_written()));
            }
            ui.label(&self.log_msg);
        });
    }

    fn plots_ui(&mut self, ui: &mut egui::Ui) {
        let window_s = WINDOW_CHOICES_S[self.window_idx.min(WINDOW_CHOICES_S.len() - 1)] as f64;
        let stats = self.store.stats(window_s);
        let range = scale::range_g(self.active_range).unwrap_or(2) as f64;
        let t_last = self.store.window(window_s).last().map(|s| s.t_s);
        let h = ((ui.available_height() - 40.0) / 3.0).max(90.0);
        for axis in 0..3 {
            let env = self.store.envelope(window_s, PLOT_BUCKETS, axis);
            let trace: PlotPoints = envelope_trace(&env).into_iter().collect();
            let mut plot = Plot::new(format!("plot{axis}"))
                .height(h)
                .y_axis_label(axis_label(axis))
                .allow_drag(false)
                .allow_zoom(false)
                .allow_scroll(false);
            if self.fixed_y {
                plot = plot.auto_bounds(false);
            }
            let fixed = self.fixed_y;
            plot.show(ui, |p| {
                if fixed {
                    p.set_plot_bounds_y(-range..=range);
                }
                if let Some(t) = t_last {
                    p.set_plot_bounds_x((t - window_s)..=t);
                }
                p.line(Line::new(axis_label(axis), trace).color(axis_color(axis)));
            });
            match &stats {
                Some(s) => ui.label(format!(
                    "{}: live {:+.5}  mean {:+.5}  RMS {:.5}",
                    axis_label(axis),
                    s.last_g[axis],
                    s.mean_g[axis],
                    s.rms_g[axis]
                )),
                None => ui.label(format!("{}: no data", axis_label(axis))),
            };
        }
    }
}

impl eframe::App for HmiApp {
    /// eframe 0.37 entry point (`ui`, not `update`).
    fn ui(&mut self, ui: &mut egui::Ui, _frame: &mut eframe::Frame) {
        if self.wake_ctx.is_none() {
            self.wake_ctx = Some(ui.ctx().clone());
            if let Some(p) = &self.initial_port {
                self.selected_port = Some(p.clone());
            }
        }
        self.handle_events();
        self.poll_status();
        egui::CentralPanel::default().show(ui, |ui| {
            egui::ScrollArea::vertical().show(ui, |ui| {
                self.controls_ui(ui);
                ui.separator();
                self.plots_ui(ui);
            });
        });
        ui.ctx().request_repaint_after(std::time::Duration::from_millis(REPAINT_INTERVAL_MS));
    }

    /// Closing the window finalizes the log and stops the worker.
    fn on_exit(&mut self, _gl: Option<&eframe::glow::Context>) {
        self.stop_logging();
        if let Some(w) = self.worker.take() {
            w.disconnect();
        }
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
