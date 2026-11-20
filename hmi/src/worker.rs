//! Serial worker for the GUI (model SerialConnection, PICO355-R8): port discovery and a reader
//! thread that turns link bytes into events, so the GUI thread never blocks on the port.
//!
//! Contract:
//! - The worker reuses `link::Transport` and the `protocol` codec; it adds no byte trait.
//! - Commands become CRC frames via `protocol::encode_command`: `SetOdr(hz)` ->
//!   `CMD_SET_ODR`, `SetRange(code)` -> `CMD_SET_RANGE`, `SetFilter(code)` -> `CMD_SET_FILTER`,
//!   `GetStatus` -> `CMD_GET_STATUS` with value 0.
//! - Responses: `RESP_ACK` -> `Ack`, `RESP_ERROR` -> `Nack`, `RESP_STATUS` -> `Status`.
//! - One `Samples` event per decoded sample-batch frame.
//! - Decode errors follow `ProtocolError::skip` (`Short` waits for more bytes).
//! - A read or write error emits `Error` and then `Disconnected`, and ends the thread.
//! - `wake` (if given) is called after each event is pushed (for a GUI repaint request).
//! - The worker NEVER sends a command on its own (no filter or ODR write on connect).
//!
//! Pico USB IDs: VID 0x2E8A / PID 0x000A (owner brief t43s2), highlighted in the port list.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{mpsc, Arc};
use std::thread::JoinHandle;

use crate::config::link;
use crate::link::{LinkError, SerialTransport, Transport};
use crate::protocol::{decode_frame, encode_command, Command, Frame, Sample};

/// Raspberry Pi USB vendor id of the Pico (owner brief t43s2).
pub const PICO_VID: u16 = 0x2E8A;
/// Pico USB CDC product id (owner brief t43s2).
pub const PICO_PID: u16 = 0x000A;

/// One serial port in the GUI dropdown.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PortInfo {
    pub path: String,
    pub description: String,
    pub is_pico: bool,
}

/// True for the Pico VID/PID pair.
pub fn is_pico(vid: u16, pid: u16) -> bool {
    vid == PICO_VID && pid == PICO_PID
}

/// Sort ports: Pico first, then by path ascending.
pub fn sort_ports(ports: &mut Vec<PortInfo>) {
    ports.sort_by(|a, b| b.is_pico.cmp(&a.is_pico).then_with(|| a.path.cmp(&b.path)));
}

/// Enumerate ports with `serialport::available_ports` (USB vid/pid -> `is_pico`), sorted with
/// `sort_ports`; empty on enumeration failure.
pub fn list_ports() -> Vec<PortInfo> {
    let mut out: Vec<PortInfo> = match serialport::available_ports() {
        Ok(v) => v
            .into_iter()
            .map(|p| match p.port_type {
                serialport::SerialPortType::UsbPort(u) => PortInfo {
                    is_pico: is_pico(u.vid, u.pid),
                    description: match (&u.manufacturer, &u.product) {
                        (Some(m), Some(pr)) => format!("USB {m} {pr} ({:04X}:{:04X})", u.vid, u.pid),
                        (None, Some(pr)) => format!("USB {pr} ({:04X}:{:04X})", u.vid, u.pid),
                        _ => format!("USB ({:04X}:{:04X})", u.vid, u.pid),
                    },
                    path: p.port_name,
                },
                other => PortInfo {
                    is_pico: false,
                    description: match other {
                        serialport::SerialPortType::PciPort => "PCI".to_string(),
                        serialport::SerialPortType::BluetoothPort => "Bluetooth".to_string(),
                        _ => "serial".to_string(),
                    },
                    path: p.port_name,
                },
            })
            .collect(),
        Err(_) => Vec::new(),
    };
    sort_ports(&mut out);
    out
}

/// Commands from the GUI to the worker.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WorkerCommand {
    SetOdr(u32),
    SetRange(u8),
    SetFilter(u8),
    GetStatus,
}

/// Events from the worker to the GUI.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum WorkerEvent {
    Samples(Vec<Sample>),
    Ack { cmd_code: u8, value: u32 },
    Nack { cmd_code: u8, value: u32 },
    Status { overflow_count: u32 },
    Error(String),
    Disconnected,
}

/// Handle to a running reader thread.
pub struct Worker {
    cmd_tx: mpsc::Sender<WorkerCommand>,
    evt_rx: mpsc::Receiver<WorkerEvent>,
    stop: Arc<AtomicBool>,
    handle: Option<JoinHandle<()>>,
}

/// Queue an event and call `wake`; false when the GUI side is gone.
fn emit(
    tx: &mpsc::Sender<WorkerEvent>,
    wake: &Option<Box<dyn Fn() + Send + 'static>>,
    ev: WorkerEvent,
) -> bool {
    let ok = tx.send(ev).is_ok();
    if ok {
        if let Some(w) = wake {
            w();
        }
    }
    ok
}

fn run<T: Transport>(
    mut transport: T,
    cmd_rx: mpsc::Receiver<WorkerCommand>,
    evt_tx: mpsc::Sender<WorkerEvent>,
    wake: Option<Box<dyn Fn() + Send + 'static>>,
    stop: Arc<AtomicBool>,
) {
    let fail = |msg: String| {
        if emit(&evt_tx, &wake, WorkerEvent::Error(msg)) {
            emit(&evt_tx, &wake, WorkerEvent::Disconnected);
        }
    };
    let mut buf: Vec<u8> = Vec::new();
    let mut chunk = [0u8; 1024];
    while !stop.load(Ordering::SeqCst) {
        // Commands first, so a write never waits behind a long read timeout.
        while let Ok(wc) = cmd_rx.try_recv() {
            let cmd = match wc {
                WorkerCommand::SetOdr(v) => Command { code: link::CMD_SET_ODR, value: v },
                WorkerCommand::SetRange(v) => Command { code: link::CMD_SET_RANGE, value: v as u32 },
                WorkerCommand::SetFilter(v) => Command { code: link::CMD_SET_FILTER, value: v as u32 },
                WorkerCommand::GetStatus => Command { code: link::CMD_GET_STATUS, value: 0 },
            };
            let bytes = match encode_command(&cmd) {
                Ok(b) => b,
                Err(e) => {
                    if !emit(&evt_tx, &wake, WorkerEvent::Error(e.to_string())) {
                        return;
                    }
                    continue;
                }
            };
            if let Err(e) = transport.write_all(&bytes) {
                fail(format!("write: {e}"));
                return;
            }
        }
        match transport.read(&mut chunk) {
            Ok(n) => buf.extend_from_slice(&chunk[..n]),
            Err(e) => {
                fail(format!("read: {e}"));
                return;
            }
        }
        // Decode every whole frame now in the buffer.
        let mut pos = 0;
        while pos < buf.len() {
            match decode_frame(&buf[pos..]) {
                Ok((frame, used)) => {
                    pos += used;
                    let ev = match frame {
                        Frame::SampleBatch(s) => Some(WorkerEvent::Samples(s)),
                        Frame::Response(r) => match r.code {
                            link::RESP_ACK => Some(WorkerEvent::Ack { cmd_code: r.cmd_code, value: r.value }),
                            link::RESP_ERROR => Some(WorkerEvent::Nack { cmd_code: r.cmd_code, value: r.value }),
                            link::RESP_STATUS => Some(WorkerEvent::Status { overflow_count: r.value }),
                            _ => None,
                        },
                        Frame::Command(_) => None,
                    };
                    if let Some(ev) = ev {
                        if !emit(&evt_tx, &wake, ev) {
                            return;
                        }
                    }
                }
                Err(e) => match e.skip() {
                    Some(k) => pos += k.min(buf.len() - pos),
                    None => break,
                },
            }
        }
        buf.drain(..pos);
    }
}

impl Worker {
    /// Start the reader thread on `transport`; `wake` is called after each event.
    pub fn spawn<T: Transport + Send + 'static>(
        transport: T,
        wake: Option<Box<dyn Fn() + Send + 'static>>,
    ) -> Worker {
        let (cmd_tx, cmd_rx) = mpsc::channel();
        let (evt_tx, evt_rx) = mpsc::channel();
        let stop = Arc::new(AtomicBool::new(false));
        let stop2 = stop.clone();
        let handle = std::thread::spawn(move || run(transport, cmd_rx, evt_tx, wake, stop2));
        Worker { cmd_tx, evt_rx, stop, handle: Some(handle) }
    }

    /// Open `path` with `link::SerialTransport::open` and spawn the worker on it.
    pub fn connect_serial(
        path: &str,
        wake: Option<Box<dyn Fn() + Send + 'static>>,
    ) -> Result<Worker, LinkError> {
        let t = SerialTransport::open(path)?;
        Ok(Worker::spawn(t, wake))
    }

    /// Queue a command for the worker thread.
    pub fn send(&self, cmd: WorkerCommand) -> Result<(), mpsc::SendError<WorkerCommand>> {
        self.cmd_tx.send(cmd)
    }

    /// Next pending event, if any (never blocks).
    pub fn try_recv(&self) -> Option<WorkerEvent> {
        self.evt_rx.try_recv().ok()
    }

    /// All pending events (never blocks).
    pub fn drain(&self) -> Vec<WorkerEvent> {
        std::iter::from_fn(|| self.evt_rx.try_recv().ok()).collect()
    }

    /// Stop the reader thread and join it.
    pub fn disconnect(mut self) {
        self.stop.store(true, Ordering::SeqCst);
        if let Some(h) = self.handle.take() {
            let _ = h.join();
        }
    }
}
