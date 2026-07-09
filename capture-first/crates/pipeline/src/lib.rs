//! Frame pipeline orchestrator — ROI crop, single-readback policy, HSV/YOLO routing.
//! Keeps `smartscreencapture` as a thin eframe shell (no MainInterface god-class).

use capture_core::{
    CaptureFrame, CaptureRoi, CpuBuffer, DetectionResult, HsvMaskStats, HsvSettings,
    InferenceBackend, InferenceSettings, ModelInputSize, ProviderState, Size2D, TensorInputHandle,
    VisionPipeline,
};
use config::RuntimeBindings;
use thiserror::Error;
use vision_gpu::GpuVisionPipeline;

/// Default operator capture ROI (320×320 center crop contract).
pub const DEFAULT_CAPTURE_ROI: Size2D = Size2D::new(320, 320);

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
    pub yolo_detections: Vec<DetectionResult>,
    pub preview_buffer: Option<CpuBuffer>,
    pub logical_zero_copy: bool,
}

impl PipelineReport {
    #[must_use]
    pub fn hsv_hit_count(&self) -> usize {
        self.hsv.hit_count
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
    pub preview_enabled: bool,
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
        if frame_size.width == 0 || frame_size.height == 0 {
            return Ok(PipelineReport {
                hsv: HsvMaskStats::default(),
                yolo_detections: Vec::new(),
                preview_buffer: None,
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

        let needs_cpu =
            settings.preview_enabled || settings.hsv.enabled || inference_ready;
        let cpu_buffer = if needs_cpu {
            Some(self.vision.to_cpu_buffer(&processed.source)?)
        } else {
            None
        };

        // After any readback, zero-copy flag must be false.
        let logical_zero_copy = processed.logical_zero_copy && cpu_buffer.is_none();

        let hsv = if let Some(buffer) = cpu_buffer.as_ref() {
            self.vision.detect_hsv_stats(buffer, &settings.hsv)?
        } else {
            HsvMaskStats::default()
        };

        let yolo_detections = if inference_ready {
            if let Some(backend) = inference {
                let tensor_input = if let Some(buffer) = cpu_buffer.as_ref() {
                    // Reuse single readback: build TensorInputHandle from CPU buffer.
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
            yolo_detections,
            preview_buffer: if settings.preview_enabled {
                cpu_buffer
            } else {
                None
            },
            logical_zero_copy,
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
            capture_roi_max: DEFAULT_CAPTURE_ROI,
            model_input: ModelInputSize::new(640, 640),
        };
        assert_eq!(settings.capture_roi_max, Size2D::new(320, 320));
        assert_eq!(settings.model_input.size, Size2D::new(640, 640));
    }
}
