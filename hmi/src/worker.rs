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

use std::sync::mpsc;

use crate::link::{LinkError, Transport};
use crate::protocol::Sample;

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
    let _ = (vid, pid);
    todo!()
}

/// Sort ports: Pico first, then by path ascending.
pub fn sort_ports(ports: &mut Vec<PortInfo>) {
    let _ = ports;
    todo!()
}

/// Enumerate ports with `serialport::available_ports` (USB vid/pid -> `is_pico`), sorted with
/// `sort_ports`; empty on enumeration failure.
pub fn list_ports() -> Vec<PortInfo> {
    todo!()
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
}

impl Worker {
    /// Start the reader thread on `transport`; `wake` is called after each event.
    pub fn spawn<T: Transport + Send + 'static>(
        transport: T,
        wake: Option<Box<dyn Fn() + Send + 'static>>,
    ) -> Worker {
        let _ = (transport, wake);
        todo!()
    }

    /// Open `path` with `link::SerialTransport::open` and spawn the worker on it.
    pub fn connect_serial(
        path: &str,
        wake: Option<Box<dyn Fn() + Send + 'static>>,
    ) -> Result<Worker, LinkError> {
        let _ = (path, wake);
        todo!()
    }

    /// Queue a command for the worker thread.
    pub fn send(&self, cmd: WorkerCommand) -> Result<(), mpsc::SendError<WorkerCommand>> {
        let _ = (&self.cmd_tx, cmd);
        todo!()
    }

    /// Next pending event, if any (never blocks).
    pub fn try_recv(&self) -> Option<WorkerEvent> {
        let _ = &self.evt_rx;
        todo!()
    }

    /// All pending events (never blocks).
    pub fn drain(&self) -> Vec<WorkerEvent> {
        todo!()
    }

    /// Stop the reader thread and join it.
    pub fn disconnect(self) {
        todo!()
    }
}
