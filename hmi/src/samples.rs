//! Live sample store for the GUI charts (model LiveDataView, PICO355-R9): a ring buffer of
//! samples in g with the time base, plot envelope, window statistics and measured rate.
//!
//! Semantics (the contract later code and tests rely on):
//! - `index` counts pushes since the last `clear` (first accepted sample = 0).
//! - `t_s = index / odr_hz` (sample time from the ODR in force when the batch was pushed).
//! - The ring keeps at most `capacity` samples and drops the oldest.
//! - `window(secs)` = stored samples with `t_s > last t_s - secs`.
//! - `envelope(secs, buckets, axis)` splits `window(secs)` into chunks of `ceil(n / buckets)`
//!   consecutive samples and returns one `EnvPoint` per chunk (`t_s` of the chunk's first
//!   sample, min and max g of that axis); `n <= buckets` gives one point per sample;
//!   `axis > 2` or `buckets == 0` gives an empty vector.
//! - `stats(secs)`: count, last, mean and RMS in g per axis over `window(secs)`. RMS is
//!   `sqrt(mean(g^2))` about zero, not mean-removed (design decision (owner t43s2)).
//!   `None` when the window is empty.
//! - `measured_rate_hz()`: host arrival time base. Take the batches whose arrival time `now_s`
//!   lies inside the trailing `RATE_WINDOW_S` (ending at the newest batch); rate = samples in
//!   every such batch after the oldest one / (newest arrival - oldest arrival). `None` with
//!   fewer than 2 batches or zero elapsed time.

use std::collections::VecDeque;

use crate::protocol::Sample;
use crate::scale::sample_to_g;

/// Ring capacity: DERIVED 10 s chart window x 4000 Hz default ODR = 40 000 samples.
pub const DEFAULT_CAPACITY: usize = 40_000;

/// Trailing window in seconds for the measured rate (design decision (owner t43s2)).
pub const RATE_WINDOW_S: f64 = 1.0;

/// One stored sample: push index, sample time, raw counts and g.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct TimedSample {
    pub index: u64,
    pub t_s: f64,
    pub raw: Sample,
    pub g: [f64; 3],
}

/// One plot envelope point: chunk start time, min and max g of one axis.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct EnvPoint {
    pub t_s: f64,
    pub min_g: f64,
    pub max_g: f64,
}

/// Window statistics per axis [x, y, z], in g.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Stats {
    pub n: usize,
    pub last_g: [f64; 3],
    pub mean_g: [f64; 3],
    pub rms_g: [f64; 3],
}

/// Ring buffer of samples in g plus batch arrival history for the rate.
#[derive(Debug, Clone)]
pub struct SampleStore {
    capacity: usize,
    samples: VecDeque<TimedSample>,
    pushed: u64,
    /// Accepted batches as (host arrival s, sample count), oldest first, trimmed to the rate window.
    batches: VecDeque<(f64, usize)>,
}

impl SampleStore {
    /// Empty store holding at most `capacity` samples.
    pub fn new(capacity: usize) -> SampleStore {
        SampleStore { capacity, samples: VecDeque::new(), pushed: 0, batches: VecDeque::new() }
    }

    /// Push one decoded batch scaled with `range_code` (see `scale`) at `odr_hz`; `now_s` is the
    /// host arrival time in seconds. Returns the accepted count; 0 (nothing stored) for an
    /// invalid range code or `odr_hz == 0`.
    pub fn push_batch(&mut self, batch: &[Sample], range_code: u8, odr_hz: u32, now_s: f64) -> usize {
        if odr_hz == 0 || batch.is_empty() || crate::scale::g_per_lsb(range_code).is_none() {
            return 0;
        }
        let odr = odr_hz as f64;
        for raw in batch {
            let g = match sample_to_g(raw, range_code) {
                Some(g) => g,
                None => unreachable!("range code checked above"),
            };
            let index = self.pushed;
            self.samples.push_back(TimedSample { index, t_s: index as f64 / odr, raw: *raw, g });
            while self.samples.len() > self.capacity {
                self.samples.pop_front();
            }
            self.pushed += 1;
        }
        self.batches.push_back((now_s, batch.len()));
        while matches!(self.batches.front(), Some(&(t, _)) if t < now_s - RATE_WINDOW_S) {
            self.batches.pop_front();
        }
        batch.len()
    }

    /// Samples currently stored.
    pub fn len(&self) -> usize {
        self.samples.len()
    }

    /// True when no sample is stored.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Maximum samples stored.
    pub fn capacity(&self) -> usize {
        self.capacity
    }

    /// Samples accepted since the last `clear` (including dropped ones).
    pub fn total_pushed(&self) -> u64 {
        self.pushed
    }

    /// Drop all samples and batch history; the index restarts at 0.
    pub fn clear(&mut self) {
        self.samples.clear();
        self.pushed = 0;
        self.batches.clear();
    }

    /// Samples with `t_s > last t_s - secs`, oldest first.
    pub fn window(&self, secs: f64) -> Vec<TimedSample> {
        let last = match self.samples.back() {
            Some(s) => s.t_s,
            None => return Vec::new(),
        };
        let limit = last - secs;
        let mut out: Vec<TimedSample> =
            self.samples.iter().rev().take_while(|s| s.t_s > limit).copied().collect();
        out.reverse();
        out
    }

    /// Min/max envelope of one axis over `window(secs)` (see module doc).
    pub fn envelope(&self, secs: f64, buckets: usize, axis: usize) -> Vec<EnvPoint> {
        if buckets == 0 || axis > 2 {
            return Vec::new();
        }
        let window = self.window(secs);
        if window.is_empty() {
            return Vec::new();
        }
        let chunk = window.len().div_ceil(buckets);
        window
            .chunks(chunk)
            .map(|c| {
                let (mut lo, mut hi) = (f64::INFINITY, f64::NEG_INFINITY);
                for s in c {
                    lo = lo.min(s.g[axis]);
                    hi = hi.max(s.g[axis]);
                }
                EnvPoint { t_s: c[0].t_s, min_g: lo, max_g: hi }
            })
            .collect()
    }

    /// Statistics over `window(secs)`; `None` when empty.
    pub fn stats(&self, secs: f64) -> Option<Stats> {
        let window = self.window(secs);
        let last = window.last()?;
        let n = window.len();
        let mut sum = [0.0f64; 3];
        let mut sq = [0.0f64; 3];
        for s in &window {
            for a in 0..3 {
                sum[a] += s.g[a];
                sq[a] += s.g[a] * s.g[a];
            }
        }
        let nf = n as f64;
        Some(Stats {
            n,
            last_g: last.g,
            mean_g: [sum[0] / nf, sum[1] / nf, sum[2] / nf],
            rms_g: [(sq[0] / nf).sqrt(), (sq[1] / nf).sqrt(), (sq[2] / nf).sqrt()],
        })
    }

    /// Measured sample rate in Hz from host arrival times (see module doc).
    pub fn measured_rate_hz(&self) -> Option<f64> {
        let newest = self.batches.back()?.0;
        let in_window: Vec<&(f64, usize)> =
            self.batches.iter().filter(|(t, _)| *t >= newest - RATE_WINDOW_S).collect();
        if in_window.len() < 2 {
            return None;
        }
        let (oldest_t, _) = *in_window[0];
        let elapsed = newest - oldest_t;
        if elapsed <= 0.0 {
            return None;
        }
        let counted: usize = in_window[1..].iter().map(|(_, n)| *n).sum();
        Some(counted as f64 / elapsed)
    }
}
