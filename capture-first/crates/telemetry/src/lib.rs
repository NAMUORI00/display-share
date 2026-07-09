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
    last_frame_at: Option<Instant>,
    last_capture_dropped: u64,
}

impl Default for TelemetryHub {
    fn default() -> Self {
        Self {
            inner: Mutex::new(TelemetryState {
                snapshot: PerformanceSnapshot::default(),
                started_at: Instant::now(),
                last_frame_at: None,
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

    pub fn on_frame(&self) {
        self.on_frame_with_processing(None);
    }

    pub fn on_frame_with_processing(&self, processing_time_ms: Option<f64>) {
        let mut state = self.inner.lock();
        let now = Instant::now();
        state.snapshot.total_frames += 1;
        if let Some(ms) = processing_time_ms {
            state.snapshot.processing_time_ms = ms;
        } else {
            state.snapshot.processing_time_ms = state
                .last_frame_at
                .map(|last| now.duration_since(last).as_secs_f64() * 1000.0)
                .unwrap_or(0.0);
        }
        state.last_frame_at = Some(now);

        let elapsed = now.duration_since(state.started_at).as_secs_f32();
        if elapsed > 0.0 {
            state.snapshot.fps = state.snapshot.total_frames as f32 / elapsed;
            state.snapshot.frame_time_ms = 1000.0 / state.snapshot.fps.max(0.001) as f64;
        }
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
