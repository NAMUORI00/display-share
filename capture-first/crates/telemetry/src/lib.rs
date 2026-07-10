//! Telemetry hub — single aggregator for capture stats + processing time.

use std::time::Instant;

use capture_core::{CaptureStats, PerformanceSnapshot};
use parking_lot::Mutex;

#[derive(Debug)]
pub struct TelemetryHub {
    inner: Mutex<TelemetryState>,
}

#[derive(Debug)]
struct TelemetryState {
    snapshot: PerformanceSnapshot,
    started_at: Instant,
    last_capture_dropped: u64,
}

impl Default for TelemetryHub {
    fn default() -> Self {
        Self {
            inner: Mutex::new(TelemetryState {
                snapshot: PerformanceSnapshot::default(),
                started_at: Instant::now(),
                last_capture_dropped: 0,
            }),
        }
    }
}

impl TelemetryHub {
    #[must_use]
    pub fn snapshot(&self) -> PerformanceSnapshot {
        self.inner.lock().snapshot.clone()
    }

    pub fn on_frames(&self, count: u64) {
        if count == 0 {
            return;
        }

        let mut state = self.inner.lock();
        let now = Instant::now();
        state.snapshot.total_frames += count;
        update_rates(&mut state, now);
    }

    /// Update processing latency without counting another capture frame.
    pub fn set_processing_ms(&self, processing_time_ms: f64) {
        self.inner.lock().snapshot.processing_time_ms = processing_time_ms;
    }

    pub fn on_drop(&self, count: u64) {
        let mut state = self.inner.lock();
        state.snapshot.dropped_frames += count;
    }

    /// Ingest capture-session stats; only newly dropped frames are counted once.
    pub fn ingest_capture_stats(&self, stats: &CaptureStats) {
        let mut state = self.inner.lock();
        if stats.dropped_frames > state.last_capture_dropped {
            state.snapshot.dropped_frames += stats.dropped_frames - state.last_capture_dropped;
        }
        state.last_capture_dropped = stats.dropped_frames;
    }
}

fn update_rates(state: &mut TelemetryState, now: Instant) {
    let elapsed = now.duration_since(state.started_at).as_secs_f32();
    if elapsed > 0.0 {
        state.snapshot.fps = state.snapshot.total_frames as f32 / elapsed;
        state.snapshot.frame_time_ms = 1000.0 / state.snapshot.fps.max(0.001) as f64;
    }
}

#[cfg(test)]
mod tests {
    use super::TelemetryHub;

    #[test]
    fn records_frame_batches_with_one_update() {
        let telemetry = TelemetryHub::default();

        telemetry.on_frames(5);

        let snapshot = telemetry.snapshot();
        assert_eq!(snapshot.total_frames, 5);
        assert_eq!(snapshot.dropped_frames, 0);
    }
}
