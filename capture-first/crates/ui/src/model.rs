use std::path::PathBuf;

use capture_core::{
    CaptureBackendKind, CaptureRoi, CaptureTarget, DetectionResult, HsvMaskStats,
    InferenceDiagnostics, PerformanceSnapshot, ProviderState, Size2D,
};
use config::AppConfig;
use egui::ColorImage;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum HsvPreviewMaskMode {
    #[default]
    RawMask,
    MorphMask,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UiCommand {
    RefreshTargets,
    StartCapture,
    StopCapture,
    LoadModel,
    SaveSettings,
    LoadHsvSettings,
    SaveHsvSettings,
}

/// Display and draft edit state. The app persists the draft on SaveSettings.
#[derive(Debug, Clone)]
pub struct UiModel {
    pub targets: Vec<CaptureTarget>,
    pub selected_target: usize,
    pub capture_running: bool,
    pub performance: PerformanceSnapshot,
    pub provider_state: ProviderState,
    pub backend_label: String,
    pub active_backend: Option<CaptureBackendKind>,
    pub last_error: Option<String>,
    pub logs: Vec<String>,
    pub config: AppConfig,
    pub preview_frame: Option<ColorImage>,
    pub preview_frame_version: u64,
    pub preview_enabled: bool,
    pub preview_scale: f32,
    pub capture_frame_size: Option<Size2D>,
    pub capture_roi: Option<CaptureRoi>,
    pub model_input_size: Option<Size2D>,
    pub hsv: HsvMaskStats,
    pub yolo_detections: Vec<DetectionResult>,
    pub inference_diagnostics: InferenceDiagnostics,
    pub config_path: Option<PathBuf>,
    pub hsv_tune_source: Option<ColorImage>,
    pub hsv_tune_raw: Option<ColorImage>,
    pub hsv_tune_morph: Option<ColorImage>,
    pub hsv_tune_overlay: Option<ColorImage>,
    pub hsv_tune_version: u64,
    pub hsv_coverage_pct: f32,
    pub hsv_mask_overlay_enabled: bool,
    pub hsv_tune_dirty: bool,
    pub hsv_preview_mask_mode: HsvPreviewMaskMode,
}

impl Default for UiModel {
    fn default() -> Self {
        Self {
            targets: Vec::new(),
            selected_target: 0,
            capture_running: false,
            performance: PerformanceSnapshot::default(),
            provider_state: ProviderState::Uninitialized,
            backend_label: "Display".to_owned(),
            active_backend: None,
            last_error: None,
            logs: Vec::new(),
            config: AppConfig::default(),
            preview_frame: None,
            preview_frame_version: 0,
            preview_enabled: true,
            preview_scale: 1.0,
            capture_frame_size: None,
            capture_roi: None,
            model_input_size: None,
            hsv: HsvMaskStats::default(),
            yolo_detections: Vec::new(),
            inference_diagnostics: InferenceDiagnostics::default(),
            config_path: None,
            hsv_tune_source: None,
            hsv_tune_raw: None,
            hsv_tune_morph: None,
            hsv_tune_overlay: None,
            hsv_tune_version: 0,
            hsv_coverage_pct: 0.0,
            hsv_mask_overlay_enabled: false,
            hsv_tune_dirty: false,
            hsv_preview_mask_mode: HsvPreviewMaskMode::RawMask,
        }
    }
}

impl UiModel {
    #[must_use]
    pub fn hsv_hit_count(&self) -> usize {
        if !self.hsv.objects.is_empty() {
            self.hsv.objects.len()
        } else {
            self.hsv.hit_count
        }
    }

    #[must_use]
    pub fn yolo_detection_count(&self) -> usize {
        self.yolo_detections.len()
    }
}
