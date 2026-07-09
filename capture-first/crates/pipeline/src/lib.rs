//! Frame pipeline orchestrator — ROI crop, single-readback policy, HSV/YOLO routing.
//! Keeps the app shell thin (no MainInterface god-class).

use capture_core::{
    AnalysisFrame, CaptureFrame, CaptureRoi, CpuBuffer, DetectionResult, HsvMaskStats, HsvSettings,
    HsvTuneResult, InferenceBackend, InferenceSettings, ModelInputSize, ProcessedFrame,
    ProviderState, Size2D, TensorInputHandle, VisionPipeline,
};
use config::RuntimeBindings;
use thiserror::Error;
use vision_gpu::GpuVisionPipeline;

/// Default operator capture ROI (320×320 center crop contract).
pub const DEFAULT_CAPTURE_ROI: Size2D = Size2D::new(320, 320);

/// Long-edge cap for full-frame UI preview readback.
/// Keep small — preview is operator UX, not the capture hot path.
pub const PREVIEW_MAX_LONG_EDGE: u32 = 960;

#[derive(Debug, Error)]
pub enum PipelineError {
    #[error(transparent)]
    Vision(#[from] capture_core::VisionError),
    #[error(transparent)]
    Inference(#[from] capture_core::InferenceError),
    #[error("pipeline failure: {0}")]
    Other(String),
}

#[derive(Debug, Clone)]
pub struct PipelineReport {
    pub hsv: HsvMaskStats,
    pub hsv_tune: Option<HsvTuneResult>,
    pub yolo_detections: Vec<DetectionResult>,
    /// Full-frame preview (downscaled), not ROI-only.
    pub preview_buffer: Option<CpuBuffer>,
    pub capture_size: Size2D,
    pub preview_scale: f32,
    pub capture_roi: CaptureRoi,
    pub model_input: Size2D,
    pub logical_zero_copy: bool,
}

impl PipelineReport {
    #[must_use]
    pub fn hsv_hit_count(&self) -> usize {
        if !self.hsv.objects.is_empty() {
            self.hsv.objects.len()
        } else if self.hsv.hit_count > 0 {
            self.hsv.hit_count
        } else {
            0
        }
    }

    #[must_use]
    pub fn yolo_detection_count(&self) -> usize {
        self.yolo_detections.len()
    }
}

#[derive(Debug, Clone)]
pub struct FramePipelineSettings {
    pub hsv: HsvSettings,
    pub inference: InferenceSettings,
    pub yolo_enabled: bool,
    /// Feature toggle: preview is allowed when the operator enables it.
    pub preview_enabled: bool,
    /// Per-frame gate: actually produce a full-frame preview buffer this tick.
    pub want_preview_buffer: bool,
    pub capture_roi_max: Size2D,
    pub model_input: ModelInputSize,
}

impl From<&RuntimeBindings> for FramePipelineSettings {
    fn from(bindings: &RuntimeBindings) -> Self {
        Self {
            hsv: bindings.hsv.clone(),
            inference: bindings.inference.clone(),
            yolo_enabled: bindings.yolo_enabled,
            preview_enabled: bindings.preview_enabled,
            // Default: produce preview whenever the feature is on; app may override to throttle.
            want_preview_buffer: bindings.preview_enabled,
            capture_roi_max: DEFAULT_CAPTURE_ROI,
            model_input: bindings.model_input,
        }
    }
}

pub struct FramePipeline {
    vision: GpuVisionPipeline,
}

impl Default for FramePipeline {
    fn default() -> Self {
        Self {
            vision: GpuVisionPipeline,
        }
    }
}

impl FramePipeline {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    pub fn process(
        &self,
        frame: &CaptureFrame,
        settings: &FramePipelineSettings,
        inference: Option<&mut dyn InferenceBackend>,
    ) -> Result<PipelineReport, PipelineError> {
        let frame_size = frame.size();
        let empty_roi = CaptureRoi::centered(frame_size, 0, 0);
        if frame_size.width == 0 || frame_size.height == 0 {
            return Ok(PipelineReport {
                hsv: HsvMaskStats::default(),
                hsv_tune: None,
                yolo_detections: Vec::new(),
                preview_buffer: None,
                capture_size: frame_size,
                preview_scale: 1.0,
                capture_roi: empty_roi,
                model_input: settings.model_input.size,
                logical_zero_copy: frame.is_gpu(),
            });
        }

        let capture_roi = CaptureRoi::centered(
            frame_size,
            settings.capture_roi_max.width,
            settings.capture_roi_max.height,
        );
        let processed = self.vision.crop_capture_roi(frame, capture_roi)?;

        let inference_ready = settings.yolo_enabled
            && inference.as_ref().is_some_and(|backend| {
                !matches!(
                    backend.provider_state(),
                    ProviderState::Uninitialized | ProviderState::Failed(_)
                )
            });

        // ROI CPU readback for HSV / YOLO preprocess.
        let needs_roi_cpu = settings.hsv.enabled || inference_ready;
        let roi_cpu = if needs_roi_cpu {
            Some(self.vision.to_cpu_buffer(&processed.source)?)
        } else {
            None
        };

        // Full-frame preview is independent of ROI analysis readback.
        // `want_preview_buffer` lets the app throttle expensive readback (~20fps).
        let (preview_buffer, preview_scale) =
            if settings.preview_enabled && settings.want_preview_buffer {
                let full = self.vision.to_cpu_buffer(frame)?;
                let (scaled, scale) = self
                    .vision
                    .downscale_for_preview(&full, PREVIEW_MAX_LONG_EDGE)?;
                (Some(scaled), scale)
            } else {
                (None, 1.0)
            };

        let logical_zero_copy =
            processed.logical_zero_copy && roi_cpu.is_none() && preview_buffer.is_none();

        let hsv_tune = if let Some(buffer) = roi_cpu.as_ref() {
            if settings.hsv.enabled {
                Some(self.vision.detect_hsv_tune(buffer, &settings.hsv, frame_size)?)
            } else {
                None
            }
        } else {
            None
        };
        let hsv = hsv_tune
            .as_ref()
            .map(|t| t.stats.clone())
            .unwrap_or_default();

        let yolo_detections = if inference_ready {
            if let Some(backend) = inference {
                let tensor_input = if let Some(buffer) = roi_cpu.as_ref() {
                    let tensor = vision_gpu::cpu_preprocess_nchw(buffer, settings.model_input.size)?;
                    let mut source = processed.clone();
                    source.source = CaptureFrame::Cpu(buffer.clone());
                    source.logical_zero_copy = false;
                    TensorInputHandle {
                        source,
                        target_size: settings.model_input.size,
                        normalized: true,
                        cpu_nchw: Some(tensor),
                    }
                } else {
                    self.vision
                        .prepare_for_inference(&processed, settings.model_input.size)?
                };
                backend.infer(&tensor_input)?
            } else {
                Vec::new()
            }
        } else {
            Vec::new()
        };

        Ok(PipelineReport {
            hsv,
            hsv_tune,
            yolo_detections,
            preview_buffer,
            capture_size: frame_size,
            preview_scale,
            capture_roi,
            model_input: settings.model_input.size,
            logical_zero_copy,
        })
    }

    /// CPU-only HSV/YOLO path. ROI buffer must already be produced on the capture thread.
    /// Never touches D3D11 — safe to call from the vision worker.
    pub fn process_analysis_roi(
        &self,
        analysis: &AnalysisFrame,
        settings: &FramePipelineSettings,
        inference: Option<&mut dyn InferenceBackend>,
    ) -> Result<PipelineReport, PipelineError> {
        let frame_size = analysis.capture_size;
        if frame_size.width == 0 || frame_size.height == 0 {
            return Ok(PipelineReport {
                hsv: HsvMaskStats::default(),
                hsv_tune: None,
                yolo_detections: Vec::new(),
                preview_buffer: None,
                capture_size: frame_size,
                preview_scale: 1.0,
                capture_roi: analysis.capture_roi,
                model_input: settings.model_input.size,
                logical_zero_copy: false,
            });
        }

        let inference_ready = settings.yolo_enabled
            && inference.as_ref().is_some_and(|backend| {
                !matches!(
                    backend.provider_state(),
                    ProviderState::Uninitialized | ProviderState::Failed(_)
                )
            });

        let needs_roi_cpu = settings.hsv.enabled || inference_ready;
        let hsv_tune = if needs_roi_cpu && settings.hsv.enabled {
            Some(self.vision.detect_hsv_tune(
                &analysis.roi_buffer,
                &settings.hsv,
                analysis.capture_size,
            )?)
        } else {
            None
        };
        let hsv = hsv_tune
            .as_ref()
            .map(|t| t.stats.clone())
            .unwrap_or_default();

        let yolo_detections = if inference_ready {
            if let Some(backend) = inference {
                let tensor =
                    vision_gpu::cpu_preprocess_nchw(&analysis.roi_buffer, settings.model_input.size)?;
                let source = ProcessedFrame {
                    source: CaptureFrame::Cpu(analysis.roi_buffer.clone()),
                    roi: analysis.capture_roi.into(),
                    logical_zero_copy: false,
                };
                let tensor_input = TensorInputHandle {
                    source,
                    target_size: settings.model_input.size,
                    normalized: true,
                    cpu_nchw: Some(tensor),
                };
                backend.infer(&tensor_input)?
            } else {
                Vec::new()
            }
        } else {
            Vec::new()
        };

        Ok(PipelineReport {
            hsv,
            hsv_tune,
            yolo_detections,
            preview_buffer: None,
            capture_size: frame_size,
            preview_scale: 1.0,
            capture_roi: analysis.capture_roi,
            model_input: settings.model_input.size,
            logical_zero_copy: false,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use capture_core::{HsvRange, PixelFormat};
    use config::AppConfig;

    #[test]
    fn headless_pipeline_covers_roi_hsv_and_preview() {
        let mut config = AppConfig::default();
        config.vision_algorithms.hsv_tracking.enabled = true;
        config.vision_algorithms.hsv_tracking.lower_bound = [0, 200, 200];
        config.vision_algorithms.hsv_tracking.upper_bound = [10, 255, 255];
        config.vision_algorithms.hsv_tracking.min_contour_area = 1;
        config.vision_algorithms.yolo26_detection.enabled = false;

        let bindings = RuntimeBindings::from_config(&config, true);
        let settings = FramePipelineSettings::from(&bindings);

        let mut frame = CpuBuffer::empty(Size2D::new(640, 640), PixelFormat::Bgra8Unorm);
        let pixel_x = 320usize;
        let pixel_y = 320usize;
        let offset = pixel_y * frame.stride as usize + pixel_x * 4;
        frame.data[offset] = 0;
        frame.data[offset + 1] = 0;
        frame.data[offset + 2] = 255;
        frame.data[offset + 3] = 255;

        let pipeline = FramePipeline::new();
        let report = pipeline
            .process(&CaptureFrame::Cpu(frame), &settings, None)
            .expect("pipeline should succeed");

        assert!(report.hsv_hit_count() >= 1);
        assert_eq!(report.yolo_detection_count(), 0);
        assert!(report.preview_buffer.is_some());
        assert_eq!(report.capture_size, Size2D::new(640, 640));
        assert!((report.preview_scale - 1.0).abs() < f32::EPSILON);
        assert!(!report.logical_zero_copy);
    }

    #[test]
    fn capture_roi_and_model_input_are_distinct() {
        let settings = FramePipelineSettings {
            hsv: HsvSettings {
                enabled: false,
                range: HsvRange {
                    lower: [0, 0, 0],
                    upper: [0, 0, 0],
                },
                morphology_kernel_size: 3,
                min_contour_area: 100,
            },
            inference: InferenceSettings {
                model_path: Default::default(),
                class_names_path: Default::default(),
                input_size: Size2D::new(640, 640),
                confidence_threshold: 0.25,
                max_detections: 100,
                selected_device_id: 0,
                execution_providers: capture_core::default_execution_providers(),
                openvino_device_type: None,
            },
            yolo_enabled: false,
            preview_enabled: false,
            want_preview_buffer: false,
            capture_roi_max: DEFAULT_CAPTURE_ROI,
            model_input: ModelInputSize::new(640, 640),
        };
        assert_eq!(settings.capture_roi_max, Size2D::new(320, 320));
        assert_eq!(settings.model_input.size, Size2D::new(640, 640));
    }

    #[test]
    fn want_preview_buffer_false_skips_full_frame_readback() {
        let mut config = AppConfig::default();
        config.vision_algorithms.hsv_tracking.enabled = true;
        config.vision_algorithms.hsv_tracking.lower_bound = [0, 200, 200];
        config.vision_algorithms.hsv_tracking.upper_bound = [10, 255, 255];
        config.vision_algorithms.hsv_tracking.min_contour_area = 1;
        config.vision_algorithms.yolo26_detection.enabled = false;

        let bindings = RuntimeBindings::from_config(&config, true);
        let mut settings = FramePipelineSettings::from(&bindings);
        settings.want_preview_buffer = false;

        let mut frame = CpuBuffer::empty(Size2D::new(640, 640), PixelFormat::Bgra8Unorm);
        let offset = 320usize * frame.stride as usize + 320usize * 4;
        frame.data[offset] = 0;
        frame.data[offset + 1] = 0;
        frame.data[offset + 2] = 255;
        frame.data[offset + 3] = 255;

        let report = FramePipeline::new()
            .process(&CaptureFrame::Cpu(frame), &settings, None)
            .expect("pipeline should succeed");

        assert!(report.hsv_hit_count() >= 1);
        assert!(report.preview_buffer.is_none());
    }
}
