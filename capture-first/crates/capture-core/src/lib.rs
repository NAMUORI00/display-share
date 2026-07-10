//! Platform-agnostic capture / vision / inference contracts.
//! D3D11 texture 구현은 `graphics-d3d11` crate에 둔다 (implementation lives elsewhere).

use std::fmt::Debug;
use std::path::PathBuf;
use std::sync::Arc;
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};
use thiserror::Error;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[non_exhaustive]
pub enum CaptureBackendKind {
    WindowsGraphicsCapture,
    DxgiDuplication,
    /// Not implemented yet / 미구현 — fail-closed at backend start.
    ObsAdapter,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[non_exhaustive]
pub enum CaptureTargetKind {
    Display,
    /// Window capture is declared for future work / 향후 확장용.
    Window,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum GraphicsBackend {
    D3d11,
    Cpu,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum PixelFormat {
    /// Canonical capture format on Windows (DXGI native).
    Bgra8Unorm,
    Rgba8Unorm,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[non_exhaustive]
pub enum InferenceBackendKind {
    DirectMl,
    Cpu,
    /// Reserved for a future CUDA path / 향후 CUDA 경로.
    CudaFuture,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct Size2D {
    pub width: u32,
    pub height: u32,
}

impl Size2D {
    #[must_use]
    pub const fn new(width: u32, height: u32) -> Self {
        Self { width, height }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct RectI {
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
}

impl RectI {
    #[must_use]
    pub const fn new(x: i32, y: i32, width: u32, height: u32) -> Self {
        Self {
            x,
            y,
            width,
            height,
        }
    }

    #[must_use]
    pub fn union(self, other: Self) -> Self {
        let x1 = self.x.min(other.x);
        let y1 = self.y.min(other.y);
        let x2 = (self.x + self.width as i32).max(other.x + other.width as i32);
        let y2 = (self.y + self.height as i32).max(other.y + other.height as i32);
        Self::new(x1, y1, (x2 - x1).max(0) as u32, (y2 - y1).max(0) as u32)
    }
}

/// Operator-facing capture ROI (e.g. 320×320 center crop).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct CaptureRoi {
    pub rect: RectI,
}

impl CaptureRoi {
    #[must_use]
    pub fn centered(frame: Size2D, max_width: u32, max_height: u32) -> Self {
        let width = frame.width.min(max_width);
        let height = frame.height.min(max_height);
        Self {
            rect: RectI::new(
                ((frame.width - width) / 2) as i32,
                ((frame.height - height) / 2) as i32,
                width,
                height,
            ),
        }
    }
}

/// Model input size after preprocess (e.g. YOLO 640×640). Distinct from [`CaptureRoi`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct ModelInputSize {
    pub size: Size2D,
}

impl ModelInputSize {
    #[must_use]
    pub const fn new(width: u32, height: u32) -> Self {
        Self {
            size: Size2D::new(width, height),
        }
    }
}

/// Backward-compatible ROI wrapper used by vision crop APIs.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct RoiSpec {
    pub rect: RectI,
}

impl From<CaptureRoi> for RoiSpec {
    fn from(value: CaptureRoi) -> Self {
        Self { rect: value.rect }
    }
}

impl RoiSpec {
    #[must_use]
    pub const fn centered(width: u32, height: u32) -> Self {
        Self {
            rect: RectI::new(0, 0, width, height),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct CompatibilityPolicy {
    pub no_hook: bool,
    pub obs_adapter_allowed: bool,
}

impl Default for CompatibilityPolicy {
    fn default() -> Self {
        Self {
            no_hook: true,
            obs_adapter_allowed: true,
        }
    }
}

/// Operator-facing product strings.
pub mod identity {
    pub const APP_DISPLAY_NAME: &str = "SmartScreenCapture";
    pub const APP_WINDOW_TITLE: &str = "SmartScreenCapture";
    pub const MONITOR_WINDOW_TITLE: &str = "SmartScreenCapture Preview";
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct CaptureTarget {
    pub id: String,
    pub name: String,
    pub kind: CaptureTargetKind,
    pub primary: bool,
    pub size: Size2D,
    pub refresh_hz: u32,
    pub native_index: usize,
    pub device_name: Option<String>,
    pub adapter_name: Option<String>,
    pub adapter_index: Option<u32>,
    pub output_index: Option<u32>,
    pub available_backends: Vec<CaptureBackendKind>,
    pub preferred_backend: CaptureBackendKind,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct CaptureOptions {
    pub include_cursor: bool,
    pub draw_border: bool,
    pub target_fps: u32,
    pub buffer_depth: usize,
    pub compatibility: CompatibilityPolicy,
}

impl Default for CaptureOptions {
    fn default() -> Self {
        Self {
            include_cursor: false,
            draw_border: false,
            target_fps: 60,
            buffer_depth: 3,
            compatibility: CompatibilityPolicy::default(),
        }
    }
}

impl CaptureOptions {
    /// Normalize options to the currently implemented display-capture backend.
    /// Hooks are never allowed.
    #[must_use]
    pub fn normalize_for_display_capture(mut self) -> Self {
        self.include_cursor = false;
        self.draw_border = false;
        self.compatibility.no_hook = true;
        self
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CpuBuffer {
    pub width: u32,
    pub height: u32,
    pub stride: u32,
    pub pixel_format: PixelFormat,
    pub data: Vec<u8>,
}

impl CpuBuffer {
    #[must_use]
    pub fn empty(size: Size2D, pixel_format: PixelFormat) -> Self {
        let stride = size.width.saturating_mul(4);
        Self {
            width: size.width,
            height: size.height,
            stride,
            pixel_format,
            data: vec![0; stride as usize * size.height as usize],
        }
    }
}

/// Opaque GPU texture contract used by vision without `Any` downcasts.
/// Concrete type: `graphics_d3d11::D3d11TextureHandle`.
pub trait GpuTexture: Debug + Send + Sync {
    fn backend(&self) -> GraphicsBackend;
    fn size(&self) -> Size2D;
    fn pixel_format(&self) -> PixelFormat;
    fn crop(&self, roi: RectI) -> Result<Arc<dyn GpuTexture>, VisionError>;
    fn readback(&self) -> Result<CpuBuffer, VisionError>;
}

pub type SharedGpuTexture = Arc<dyn GpuTexture>;

/// Windows-first frame enum: D3D11 texture or CPU buffer. No `as_any` downcast web.
#[derive(Debug, Clone)]
pub enum CaptureFrame {
    D3d11(SharedGpuTexture),
    Cpu(CpuBuffer),
}

impl CaptureFrame {
    #[must_use]
    pub fn size(&self) -> Size2D {
        match self {
            Self::D3d11(texture) => texture.size(),
            Self::Cpu(buffer) => Size2D::new(buffer.width, buffer.height),
        }
    }

    #[must_use]
    pub fn pixel_format(&self) -> PixelFormat {
        match self {
            Self::D3d11(texture) => texture.pixel_format(),
            Self::Cpu(buffer) => buffer.pixel_format,
        }
    }

    #[must_use]
    pub fn is_gpu(&self) -> bool {
        matches!(self, Self::D3d11(_))
    }
}

#[derive(Debug, Clone)]
pub struct CapturePacket {
    pub frame_number: u64,
    pub timestamp: Instant,
    pub dirty_regions: Vec<RectI>,
    pub frame: CaptureFrame,
}

impl CapturePacket {
    #[must_use]
    pub fn new(frame_number: u64, frame: CaptureFrame, dirty_regions: Vec<RectI>) -> Self {
        Self {
            frame_number,
            timestamp: Instant::now(),
            dirty_regions,
            frame,
        }
    }
}

/// Monitor preview produced on the capture thread (CPU only; no cross-thread D3D11).
#[derive(Debug, Clone)]
pub struct PreviewFrame {
    pub frame_number: u64,
    pub buffer: CpuBuffer,
    /// Uniform scale: preview pixels / capture pixels.
    pub scale: f32,
    pub capture_size: Size2D,
}

/// ROI CPU buffer for HSV/YOLO — produced on the capture thread.
#[derive(Debug, Clone)]
pub struct AnalysisFrame {
    pub frame_number: u64,
    pub capture_size: Size2D,
    pub capture_roi: CaptureRoi,
    pub roi_buffer: CpuBuffer,
}

#[derive(Debug, Clone, Default)]
pub struct CaptureStats {
    pub total_frames: u64,
    pub dropped_frames: u64,
    pub last_latency: Option<Duration>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct PerformanceSnapshot {
    pub fps: f32,
    pub frame_time_ms: f64,
    pub processing_time_ms: f64,
    pub dropped_frames: u64,
    pub total_frames: u64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct HsvRange {
    pub lower: [u8; 3],
    pub upper: [u8; 3],
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct HsvSettings {
    pub enabled: bool,
    pub range: HsvRange,
    pub morphology_kernel_size: u32,
    pub min_contour_area: u32,
}

/// Compact HSV mask summary — not per-pixel YOLO-style boxes.
#[derive(Debug, Clone, PartialEq)]
pub struct HsvDetectedObject {
    /// Bounding box in full capture-frame coordinates.
    pub rect: RectI,
    pub area: f32,
}

/// Compact HSV mask summary — not per-pixel YOLO-style boxes.
#[derive(Debug, Clone, PartialEq, Default)]
pub struct HsvMaskStats {
    pub hit_count: usize,
    pub bbox_union: Option<RectI>,
    /// Contour-based detections (frame coordinates).
    pub objects: Vec<HsvDetectedObject>,
}

/// HSV tuning preview — raw/morph masks at analysis-buffer resolution.
#[derive(Debug, Clone, PartialEq, Default)]
pub struct HsvTuneResult {
    pub stats: HsvMaskStats,
    pub preview_width: u32,
    pub preview_height: u32,
    /// Per-pixel mask before morphology (0 or 255).
    pub raw_mask: Vec<u8>,
    /// Per-pixel mask after dilate (0 or 255).
    pub morph_mask: Vec<u8>,
}

impl HsvTuneResult {
    #[must_use]
    pub fn coverage_pct(&self) -> f32 {
        let total = (self.preview_width * self.preview_height) as f32;
        if total <= 0.0 {
            return 0.0;
        }
        self.stats.hit_count as f32 / total * 100.0
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct InferenceSettings {
    pub model_path: PathBuf,
    pub class_names_path: PathBuf,
    pub input_size: Size2D,
    pub confidence_threshold: f32,
    pub max_detections: usize,
    pub selected_device_id: usize,
    /// Ordered EP names: `directml`, `openvino`, `cpu`, or `auto`.
    pub execution_providers: Vec<String>,
    /// OpenVINO device_type override (`GPU`, `NPU`, `GPU.0`, …). None = try GPU then NPU.
    pub openvino_device_type: Option<String>,
}

impl Default for InferenceSettings {
    fn default() -> Self {
        Self {
            model_path: PathBuf::new(),
            class_names_path: PathBuf::new(),
            input_size: Size2D::new(640, 640),
            confidence_threshold: 0.25,
            max_detections: 100,
            selected_device_id: 0,
            execution_providers: default_execution_providers(),
            openvino_device_type: None,
        }
    }
}

#[must_use]
pub fn default_execution_providers() -> Vec<String> {
    vec![
        "directml".to_owned(),
        "openvino".to_owned(),
        "cpu".to_owned(),
    ]
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum ProviderState {
    Uninitialized,
    DirectMl {
        device_id: usize,
        cpu_fallback: bool,
    },
    OpenVino {
        device_type: String,
    },
    CpuFallback,
    Failed(String),
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct InferenceModelMetadata {
    pub producer: Option<String>,
    pub graph_name: Option<String>,
    pub domain: Option<String>,
    pub description: Option<String>,
    pub graph_description: Option<String>,
    pub version: Option<i64>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct InferenceIoDescriptor {
    pub name: String,
    pub value_type: String,
    pub tensor_shape: Option<Vec<i64>>,
    pub tensor_element_type: Option<String>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct InferenceDiagnostics {
    pub model_path: Option<PathBuf>,
    pub class_names_path: Option<PathBuf>,
    pub class_count: usize,
    pub expected_input_size: Option<Size2D>,
    pub model_metadata: InferenceModelMetadata,
    pub inputs: Vec<InferenceIoDescriptor>,
    pub outputs: Vec<InferenceIoDescriptor>,
    pub validation_notes: Vec<String>,
    pub last_output_shape: Option<Vec<i64>>,
    pub fallback_reason: Option<String>,
    pub last_error: Option<String>,
}

/// Object-detection result (YOLO / similar). Distinct from [`HsvMaskStats`].
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct DetectionResult {
    pub bounding_box: RectI,
    pub confidence_milli: u16,
    pub label: String,
    pub class_id: i32,
}

#[derive(Debug, Clone)]
pub struct ProcessedFrame {
    pub source: CaptureFrame,
    pub roi: RoiSpec,
    /// True only while the frame remains on GPU without CPU readback.
    pub logical_zero_copy: bool,
}

/// Prepared inference input. Prefer CPU NCHW tensor when preprocess completed on CPU;
/// GPU path may keep `source` until DirectML IO binding lands (Track 2).
#[derive(Debug, Clone)]
pub struct TensorInputHandle {
    pub source: ProcessedFrame,
    pub target_size: Size2D,
    pub normalized: bool,
    /// Preprocessed NCHW float tensor when available (CPU fallback / current path).
    pub cpu_nchw: Option<CpuNchwTensor>,
}

#[derive(Debug, Clone)]
pub struct CpuNchwTensor {
    pub width: u32,
    pub height: u32,
    pub data: Vec<f32>,
}

impl CpuNchwTensor {
    #[must_use]
    pub fn shape(&self) -> [usize; 4] {
        [1, 3, self.height as usize, self.width as usize]
    }
}

#[derive(Debug, Error)]
pub enum CaptureError {
    #[error("unsupported capture target: {0}")]
    UnsupportedTarget(String),
    #[error("capture backend is unavailable: {0}")]
    BackendUnavailable(String),
    #[error("capture session has already stopped")]
    SessionStopped,
    #[error("capture failure: {0}")]
    Other(String),
}

#[derive(Debug, Error)]
pub enum VisionError {
    #[error("ROI is outside of the frame bounds")]
    InvalidRoi,
    #[error("vision operation is not supported for this GPU texture yet")]
    UnsupportedGpuOperation,
    #[error("pixel format mismatch: expected {expected:?}, got {actual:?}")]
    PixelFormatMismatch {
        expected: PixelFormat,
        actual: PixelFormat,
    },
    #[error("vision failure: {0}")]
    Other(String),
}

#[derive(Debug, Error)]
pub enum InferenceError {
    #[error("inference backend is not initialized")]
    NotInitialized,
    #[error("model path does not exist: {0}")]
    MissingModel(PathBuf),
    #[error("tensor input is missing preprocessed CPU NCHW data")]
    MissingPreprocessedTensor,
    #[error("inference failure: {0}")]
    Other(String),
}

pub trait CaptureSession: Send + Sync {
    fn try_recv(&self) -> Result<Option<CapturePacket>, CaptureError>;
    fn stop(&self) -> Result<(), CaptureError>;
    fn stats(&self) -> CaptureStats;
    /// Source of truth for the active backend (facade must not lie).
    fn backend_kind(&self) -> CaptureBackendKind;

    /// Gate Monitor preview readback on the capture thread (~20 FPS when enabled).
    fn set_preview_enabled(&self, _enabled: bool) {}
    fn try_recv_preview(&self) -> Result<Option<PreviewFrame>, CaptureError> {
        Ok(None)
    }

    /// Gate ROI CPU production for HSV/YOLO (capture-thread readback only).
    fn set_analysis_enabled(&self, _enabled: bool) {}
    fn try_recv_analysis(&self) -> Result<Option<AnalysisFrame>, CaptureError> {
        Ok(None)
    }
}

pub trait CaptureBackend: Send + Sync {
    fn enumerate_targets(&self) -> Result<Vec<CaptureTarget>, CaptureError>;
    fn start(
        &self,
        target: &CaptureTarget,
        options: CaptureOptions,
    ) -> Result<Box<dyn CaptureSession>, CaptureError>;
    fn supports_zero_copy(&self) -> bool;
}

pub trait VisionPipeline: Send + Sync {
    fn crop_roi(&self, frame: &CaptureFrame, roi: RoiSpec) -> Result<ProcessedFrame, VisionError>;
    /// Must perform real preprocess or return [`VisionError::UnsupportedGpuOperation`].
    /// Silent stub that only sets `normalized: true` is forbidden.
    fn prepare_for_inference(
        &self,
        frame: &ProcessedFrame,
        target_size: Size2D,
    ) -> Result<TensorInputHandle, VisionError>;
}

pub trait InferenceBackend: Send + Sync {
    fn backend_name(&self) -> &'static str;
    fn initialize(&mut self, settings: InferenceSettings) -> Result<ProviderState, InferenceError>;
    fn provider_state(&self) -> ProviderState;
    fn infer(&mut self, input: &TensorInputHandle) -> Result<Vec<DetectionResult>, InferenceError>;
    fn last_error(&self) -> Option<String>;
    fn diagnostics(&self) -> InferenceDiagnostics;
}

#[must_use]
pub fn backend_kind_label(kind: CaptureBackendKind) -> &'static str {
    match kind {
        CaptureBackendKind::WindowsGraphicsCapture => "WGC",
        CaptureBackendKind::DxgiDuplication => "DXGI",
        CaptureBackendKind::ObsAdapter => "OBS",
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn normalize_for_display_capture_forces_supported_defaults() {
        let options = CaptureOptions {
            include_cursor: true,
            draw_border: true,
            target_fps: 30,
            buffer_depth: 2,
            compatibility: CompatibilityPolicy {
                no_hook: false,
                obs_adapter_allowed: true,
            },
        }
        .normalize_for_display_capture();

        assert!(!options.include_cursor);
        assert!(!options.draw_border);
        assert!(options.compatibility.no_hook);
    }
}
