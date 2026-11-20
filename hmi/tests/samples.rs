//! PR red tests for hmi/src/samples.rs (SampleStore). Contract = the module doc of samples.rs.
use pico355_hmi::protocol::Sample;
use pico355_hmi::samples::*;

const S1: f64 = 3.9e-6; // g per LSB at range code 1 (adxl355.h:62-65)

fn batch(range: std::ops::Range<i32>) -> Vec<Sample> {
    range.map(|i| Sample { x: i * 1000, y: -i * 1000, z: i }).collect()
}

#[test]
fn constants() {
    assert_eq!(DEFAULT_CAPACITY, 40_000);
    assert_eq!(RATE_WINDOW_S, 1.0);
}

#[test]
fn new_store_is_empty_with_capacity() {
    let st = SampleStore::new(10);
    assert_eq!(st.capacity(), 10);
    assert_eq!(st.len(), 0);
    assert!(st.is_empty());
    assert_eq!(st.total_pushed(), 0);
}

#[test]
fn ring_evicts_oldest_at_capacity() {
    let mut st = SampleStore::new(10);
    assert_eq!(st.push_batch(&batch(0..25), 1, 1000, 0.0), 25);
    assert_eq!(st.len(), 10);
    assert_eq!(st.total_pushed(), 25);
    let w = st.window(1000.0);
    assert_eq!(w.len(), 10);
    assert_eq!(w[0].index, 15);
    assert_eq!(w[9].index, 24);
}

#[test]
fn index_continues_across_batches_and_time_base() {
    let mut st = SampleStore::new(100);
    st.push_batch(&batch(0..5), 1, 1000, 0.0);
    st.push_batch(&batch(5..10), 1, 1000, 0.1);
    let w = st.window(1000.0);
    assert_eq!(w.len(), 10);
    for (i, s) in w.iter().enumerate() {
        assert_eq!(s.index, i as u64);
        assert!((s.t_s - i as f64 / 1000.0).abs() < 1e-12);
        assert_eq!(s.raw, Sample { x: i as i32 * 1000, y: -(i as i32) * 1000, z: i as i32 });
        assert!((s.g[0] - i as f64 * 1000.0 * S1).abs() < 1e-12);
        assert!((s.g[1] + i as f64 * 1000.0 * S1).abs() < 1e-12);
    }
}

#[test]
fn window_length_at_4000_hz_for_2_s() {
    let mut st = SampleStore::new(DEFAULT_CAPACITY);
    st.push_batch(&batch(0..10_000), 1, 4000, 0.0);
    // last t = 9999/4000; t > last - 2 s  <=>  index >= 2000 -> 8000 samples
    let w = st.window(2.0);
    assert_eq!(w.len(), 8000);
    assert_eq!(w[0].index, 2000);
    assert_eq!(w.last().unwrap().index, 9999);
}

#[test]
fn g_scale_follows_range_of_each_batch() {
    let mut st = SampleStore::new(10);
    st.push_batch(&[Sample { x: 1000, y: 0, z: 0 }], 1, 1000, 0.0);
    st.push_batch(&[Sample { x: 1000, y: 0, z: 0 }], 3, 1000, 0.1);
    let w = st.window(10.0);
    assert!((w[0].g[0] - 1000.0 * 3.9e-6).abs() < 1e-12);
    assert!((w[1].g[0] - 1000.0 * 15.6e-6).abs() < 1e-12);
}

#[test]
fn envelope_bucket_count_min_max() {
    let mut st = SampleStore::new(1000);
    st.push_batch(&batch(0..100), 1, 1000, 0.0);
    let env = st.envelope(1000.0, 10, 0);
    assert_eq!(env.len(), 10);
    for (k, p) in env.iter().enumerate() {
        let lo = (k * 10) as f64 * 1000.0 * S1;
        let hi = (k * 10 + 9) as f64 * 1000.0 * S1;
        assert!((p.min_g - lo).abs() < 1e-12, "bucket {k}");
        assert!((p.max_g - hi).abs() < 1e-12, "bucket {k}");
        assert!((p.t_s - (k * 10) as f64 / 1000.0).abs() < 1e-12);
    }
    // axis 1 is the negated ramp: min/max swap sign
    let env1 = st.envelope(1000.0, 10, 1);
    assert!((env1[0].max_g - 0.0).abs() < 1e-12);
    assert!((env1[0].min_g + 9000.0 * S1).abs() < 1e-12);
}

#[test]
fn envelope_edge_cases() {
    let mut st = SampleStore::new(1000);
    st.push_batch(&batch(0..5), 1, 1000, 0.0);
    // n <= buckets: one point per sample
    assert_eq!(st.envelope(1000.0, 50, 0).len(), 5);
    assert!(st.envelope(1000.0, 0, 0).is_empty());
    assert!(st.envelope(1000.0, 10, 3).is_empty());
    let empty = SampleStore::new(5);
    assert!(empty.envelope(1.0, 10, 0).is_empty());
}

#[test]
fn stats_constant_in_g() {
    let mut st = SampleStore::new(100);
    let b: Vec<Sample> = (0..20).map(|_| Sample { x: 1000, y: -1000, z: 0 }).collect();
    st.push_batch(&b, 1, 1000, 0.0);
    let s = st.stats(100.0).unwrap();
    let g = 1000.0 * S1;
    assert_eq!(s.n, 20);
    assert!((s.mean_g[0] - g).abs() < 1e-12 && (s.mean_g[1] + g).abs() < 1e-12 && s.mean_g[2] == 0.0);
    assert!((s.rms_g[0] - g).abs() < 1e-12 && (s.rms_g[1] - g).abs() < 1e-12 && s.rms_g[2] == 0.0);
    assert!((s.last_g[0] - g).abs() < 1e-12);
}

#[test]
fn stats_alternating_pattern() {
    let mut st = SampleStore::new(100);
    let b: Vec<Sample> =
        (0..20).map(|i| Sample { x: if i % 2 == 0 { 1000 } else { -1000 }, y: 0, z: 0 }).collect();
    st.push_batch(&b, 1, 1000, 0.0);
    let s = st.stats(100.0).unwrap();
    let g = 1000.0 * S1;
    assert!(s.mean_g[0].abs() < 1e-12, "mean {}", s.mean_g[0]);
    assert!((s.rms_g[0] - g).abs() < 1e-12, "rms about zero, not mean-removed");
    assert!((s.last_g[0] + g).abs() < 1e-12);
}

#[test]
fn stats_none_when_empty() {
    assert!(SampleStore::new(5).stats(1.0).is_none());
}

#[test]
fn rate_none_then_some_from_arrival_times() {
    let mut st = SampleStore::new(10_000);
    assert_eq!(st.measured_rate_hz(), None);
    st.push_batch(&batch(0..100), 1, 4000, 0.1);
    assert_eq!(st.measured_rate_hz(), None, "one batch");
    st.push_batch(&batch(0..100), 1, 4000, 0.5);
    st.push_batch(&batch(0..100), 1, 4000, 0.9);
    // samples after the oldest batch (200) / (0.9 - 0.1) s
    let r = st.measured_rate_hz().unwrap();
    assert!((r - 250.0).abs() < 1e-6, "rate {r}");
}

#[test]
fn rate_uses_trailing_window_only() {
    let mut st = SampleStore::new(10_000);
    st.push_batch(&batch(0..1000), 1, 4000, 0.0);
    st.push_batch(&batch(0..50), 1, 4000, 5.0);
    st.push_batch(&batch(0..50), 1, 4000, 5.5);
    let r = st.measured_rate_hz().unwrap();
    assert!((r - 100.0).abs() < 1e-6, "rate {r}");
}

#[test]
fn rate_none_for_zero_elapsed() {
    let mut st = SampleStore::new(1000);
    st.push_batch(&batch(0..10), 1, 4000, 2.0);
    st.push_batch(&batch(0..10), 1, 4000, 2.0);
    assert_eq!(st.measured_rate_hz(), None);
}

#[test]
fn bad_range_or_odr_accepts_nothing() {
    let mut st = SampleStore::new(10);
    assert_eq!(st.push_batch(&batch(0..5), 0, 1000, 0.0), 0);
    assert_eq!(st.push_batch(&batch(0..5), 4, 1000, 0.0), 0);
    assert_eq!(st.push_batch(&batch(0..5), 1, 0, 0.0), 0);
    assert_eq!(st.len(), 0);
    assert_eq!(st.total_pushed(), 0);
}

#[test]
fn clear_resets_everything() {
    let mut st = SampleStore::new(10);
    st.push_batch(&batch(0..5), 1, 1000, 0.0);
    st.push_batch(&batch(0..5), 1, 1000, 0.5);
    st.clear();
    assert_eq!(st.len(), 0);
    assert_eq!(st.total_pushed(), 0);
    assert_eq!(st.measured_rate_hz(), None);
    st.push_batch(&batch(0..2), 1, 1000, 1.0);
    let w = st.window(10.0);
    assert_eq!(w[0].index, 0, "index restarts at 0");
}
