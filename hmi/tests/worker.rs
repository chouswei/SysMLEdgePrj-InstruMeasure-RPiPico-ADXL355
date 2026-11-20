//! PR red tests for hmi/src/worker.rs with a mock `link::Transport` speaking the real CRC frames.
use std::collections::VecDeque;
use std::io;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::{mpsc, Arc, Mutex};
use std::time::{Duration, Instant};

use pico355_hmi::config::link;
use pico355_hmi::link::Transport;
use pico355_hmi::protocol::{encode_command, encode_response, encode_sample_batch, Command, Response, Sample};
use pico355_hmi::worker::*;

enum Item {
    Data(Vec<u8>),
    Err,
}

#[derive(Clone, Default)]
struct Shared {
    rx: Arc<Mutex<VecDeque<Item>>>,
    written: Arc<Mutex<Vec<u8>>>,
}

struct Mock(Shared);

impl Transport for Mock {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let item = self.0.rx.lock().unwrap().pop_front();
        match item {
            Some(Item::Data(mut d)) => {
                let n = d.len().min(buf.len());
                buf[..n].copy_from_slice(&d[..n]);
                if n < d.len() {
                    let rest = d.split_off(n);
                    self.0.rx.lock().unwrap().push_front(Item::Data(rest));
                }
                Ok(n)
            }
            Some(Item::Err) => Err(io::Error::new(io::ErrorKind::BrokenPipe, "mock read error")),
            None => {
                std::thread::sleep(Duration::from_millis(2));
                Ok(0)
            }
        }
    }
    fn write_all(&mut self, buf: &[u8]) -> io::Result<()> {
        self.0.written.lock().unwrap().extend_from_slice(buf);
        Ok(())
    }
}

fn feed(sh: &Shared, bytes: Vec<u8>) {
    sh.rx.lock().unwrap().push_back(Item::Data(bytes));
}

/// Collect events until `pred` says enough or 3 s pass.
fn collect(w: &Worker, pred: impl Fn(&[WorkerEvent]) -> bool) -> Vec<WorkerEvent> {
    let mut all = Vec::new();
    let t0 = Instant::now();
    while t0.elapsed() < Duration::from_secs(3) {
        all.extend(w.drain());
        if pred(&all) {
            break;
        }
        std::thread::sleep(Duration::from_millis(5));
    }
    all
}

fn samples(n: i32) -> Vec<Sample> {
    (0..n).map(|i| Sample { x: i - 3, y: -i * 100, z: 524_287 - i }).collect()
}

fn resp(code: u8, cmd_code: u8, value: u32) -> Vec<u8> {
    encode_response(&Response { code, cmd_code, value }).unwrap()
}

#[test]
fn samples_event_per_frame() {
    let sh = Shared::default();
    let w = Worker::spawn(Mock(sh.clone()), None);
    feed(&sh, encode_sample_batch(&samples(4)).unwrap());
    feed(&sh, encode_sample_batch(&samples(2)).unwrap());
    let ev = collect(&w, |e| e.len() >= 2);
    assert_eq!(ev, vec![WorkerEvent::Samples(samples(4)), WorkerEvent::Samples(samples(2))]);
}

#[test]
fn ack_nack_status_mapping() {
    let sh = Shared::default();
    let w = Worker::spawn(Mock(sh.clone()), None);
    feed(&sh, resp(link::RESP_ACK, link::CMD_SET_RANGE, 2));
    feed(&sh, resp(link::RESP_ERROR, link::CMD_SET_ODR, 3000));
    feed(&sh, resp(link::RESP_STATUS, link::CMD_GET_STATUS, 7));
    let ev = collect(&w, |e| e.len() >= 3);
    assert_eq!(
        ev,
        vec![
            WorkerEvent::Ack { cmd_code: link::CMD_SET_RANGE, value: 2 },
            WorkerEvent::Nack { cmd_code: link::CMD_SET_ODR, value: 3000 },
            WorkerEvent::Status { overflow_count: 7 },
        ]
    );
}

#[test]
fn each_command_writes_exactly_encode_command_bytes() {
    let sh = Shared::default();
    let w = Worker::spawn(Mock(sh.clone()), None);
    let cases = [
        (WorkerCommand::SetOdr(1000), Command { code: link::CMD_SET_ODR, value: 1000 }),
        (WorkerCommand::SetRange(3), Command { code: link::CMD_SET_RANGE, value: 3 }),
        (WorkerCommand::SetFilter(4), Command { code: link::CMD_SET_FILTER, value: 4 }),
        (WorkerCommand::GetStatus, Command { code: link::CMD_GET_STATUS, value: 0 }),
    ];
    for (wc, c) in cases {
        sh.written.lock().unwrap().clear();
        w.send(wc).unwrap();
        let want = encode_command(&c).unwrap();
        let t0 = Instant::now();
        while sh.written.lock().unwrap().len() < want.len() && t0.elapsed() < Duration::from_secs(3) {
            std::thread::sleep(Duration::from_millis(5));
        }
        std::thread::sleep(Duration::from_millis(30));
        assert_eq!(*sh.written.lock().unwrap(), want, "{wc:?}");
    }
}

#[test]
fn no_write_without_a_command() {
    let sh = Shared::default();
    let w = Worker::spawn(Mock(sh.clone()), None);
    feed(&sh, encode_sample_batch(&samples(3)).unwrap());
    feed(&sh, resp(link::RESP_ACK, link::CMD_SET_FILTER, 1));
    let _ = collect(&w, |e| e.len() >= 2);
    std::thread::sleep(Duration::from_millis(100));
    assert!(sh.written.lock().unwrap().is_empty(), "worker must never write on its own");
}

#[test]
fn garbage_between_frames_resyncs() {
    let sh = Shared::default();
    let w = Worker::spawn(Mock(sh.clone()), None);
    let mut bytes = vec![0x00, 0x13, 0x77, 0x42];
    bytes.extend(encode_sample_batch(&samples(2)).unwrap());
    bytes.extend([0x01, 0x02, 0x03]);
    bytes.extend(resp(link::RESP_ACK, link::CMD_SET_ODR, 500));
    feed(&sh, bytes);
    let ev = collect(&w, |e| e.len() >= 2);
    assert_eq!(
        ev,
        vec![WorkerEvent::Samples(samples(2)), WorkerEvent::Ack { cmd_code: link::CMD_SET_ODR, value: 500 }]
    );
}

#[test]
fn frame_split_across_reads_is_reassembled() {
    let sh = Shared::default();
    let w = Worker::spawn(Mock(sh.clone()), None);
    let f = encode_sample_batch(&samples(5)).unwrap();
    let (a, b) = f.split_at(7);
    feed(&sh, a.to_vec());
    feed(&sh, b.to_vec());
    let ev = collect(&w, |e| !e.is_empty());
    assert_eq!(ev, vec![WorkerEvent::Samples(samples(5))]);
}

#[test]
fn read_error_gives_error_then_disconnected() {
    let sh = Shared::default();
    let w = Worker::spawn(Mock(sh.clone()), None);
    sh.rx.lock().unwrap().push_back(Item::Err);
    let ev = collect(&w, |e| e.iter().any(|x| *x == WorkerEvent::Disconnected));
    assert_eq!(ev.len(), 2, "{ev:?}");
    assert!(matches!(ev[0], WorkerEvent::Error(_)), "{ev:?}");
    assert_eq!(ev[1], WorkerEvent::Disconnected);
}

#[test]
fn disconnect_joins_the_thread() {
    let sh = Shared::default();
    let w = Worker::spawn(Mock(sh), None);
    let (tx, rx) = mpsc::channel();
    std::thread::spawn(move || {
        w.disconnect();
        let _ = tx.send(());
    });
    assert!(rx.recv_timeout(Duration::from_secs(3)).is_ok(), "disconnect must return");
}

#[test]
fn wake_callback_is_called_per_event() {
    let sh = Shared::default();
    let n = Arc::new(AtomicUsize::new(0));
    let n2 = n.clone();
    let w = Worker::spawn(Mock(sh.clone()), Some(Box::new(move || {
        n2.fetch_add(1, Ordering::SeqCst);
    })));
    feed(&sh, encode_sample_batch(&samples(2)).unwrap());
    feed(&sh, resp(link::RESP_ACK, link::CMD_SET_ODR, 250));
    let _ = collect(&w, |e| e.len() >= 2);
    assert!(n.load(Ordering::SeqCst) >= 2);
}

#[test]
fn connect_serial_missing_port_is_err() {
    assert!(Worker::connect_serial("/nonexistent/pico355-port", None).is_err());
}

#[test]
fn pico_ids() {
    assert_eq!((PICO_VID, PICO_PID), (0x2E8A, 0x000A));
    assert!(is_pico(0x2E8A, 0x000A));
    assert!(!is_pico(0x2E8A, 0x000B));
    assert!(!is_pico(0x0403, 0x6001));
    assert!(!is_pico(0x000A, 0x2E8A));
}

#[test]
fn sort_ports_pico_first_then_path() {
    let p = |path: &str, pico: bool| PortInfo { path: path.into(), description: String::new(), is_pico: pico };
    let mut v = vec![p("COM3", false), p("COM9", true), p("COM1", false), p("COM5", true)];
    sort_ports(&mut v);
    let paths: Vec<&str> = v.iter().map(|x| x.path.as_str()).collect();
    assert_eq!(paths, ["COM5", "COM9", "COM1", "COM3"]);
}
