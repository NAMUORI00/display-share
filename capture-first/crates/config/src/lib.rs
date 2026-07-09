use std::fs;
use std::path::{Path, PathBuf};

use capture_core::{
    CaptureBackendPreference, CaptureOptions, CaptureRoi, CompatibilityPolicy, HsvRange,
    HsvSettings, InferenceSettings, ModelInputSize, Size2D,
};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum ConfigError {
    #[error("failed to read config file: {0}")]
    Io(#[from] std::io::Error),
    #[error("failed to parse config file: {0}")]
    Json(#[from] serde_json::Error),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProductionSystem {
    pub name: String,
    pub version: String,
    pub purpose: String,
    pub mode: String,
}

impl Default for ProductionSystem {
    fn default() -> Self {
        Self {
            name: "Professional Screen Capture & Computer Vision System".to_owned(),
            version: "1.0.0".to_owned(),
            purpose: "High-performance real-time screen capture and computer vision processing"
                .to_owned(),
            mode: "production_mode".to_owned(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AnalyticsConfig {
    /// Ignored until Track X telemetry wiring / 텔레메트리 연동 전까지 미사용.
    pub enabled: bool,
    /// Ignored until Track X / 미사용.
    pub max_history_size: usize,
    /// Ignored until Track X / 미사용.
    pub fps_calculation_window_sec: f32,
}

impl Default for AnalyticsConfig {
    fn default() -> Self {
        Self {
            enabled: true,
            max_history_size: 1000,
            fps_calculation_window_sec: 5.0,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GuiConfig {
    pub show_performance_metrics: bool,
    /// Ignored until UI scale wiring / UI 스케일 연동 전까지 미사용.
    pub ui_scale: f32,
    /// Ignored until overlay wiring / 오버레이 연동 전까지 미사용.
    pub show_metrics_overlay: bool,
}

impl Default for GuiConfig {
    fn default() -> Self {
        Self {
            show_performance_metrics: true,
            ui_scale: 1.0,
            show_metrics_overlay: true,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PerformanceConfig {
    pub target_fps: u32,
    /// Ignored until multithreaded pump / 멀티스레드 펌프 전까지 미사용.
    pub enable_multithreading: bool,
    /// Ignored until multithreaded pump / 미사용.
    pub max_processing_threads: usize,
    /// Mapped to [`CaptureOptions::buffer_depth`] via [`RuntimeBindings`].
    pub frame_buffer_size: usize,
    /// Ignored until GPU accel toggle / GPU 가속 토글 전까지 미사용.
    pub enable_gpu_acceleration: bool,
}

impl Default for PerformanceConfig {
    fn default() -> Self {
        Self {
            target_fps: 60,
            enable_multithreading: true,
            max_processing_threads: 4,
            frame_buffer_size: 5,
            enable_gpu_acceleration: true,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CompatibilityConfig {
    pub no_hook: bool,
    pub obs_adapter_allowed: bool,
}

impl Default for CompatibilityConfig {
    fn default() -> Self {
        Self {
            no_hook: true,
            obs_adapter_allowed: true,
        }
    }
}

impl From<&CompatibilityConfig> for CompatibilityPolicy {
    fn from(value: &CompatibilityConfig) -> Self {
        Self {
            no_hook: value.no_hook,
            obs_adapter_allowed: value.obs_adapter_allowed,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HsvTrackingConfig {
    pub enabled: bool,
    pub lower_bound: [u8; 3],
    pub upper_bound: [u8; 3],
    pub morphology_kernel_size: u32,
    pub min_contour_area: u32,
}

impl Default for HsvTrackingConfig {
    fn default() -> Self {
        Self {
            enabled: true,
            lower_bound: [140, 120, 180],
            upper_bound: [160, 200, 255],
            morphology_kernel_size: 3,
            min_contour_area: 100,
        }
    }
}

impl From<&HsvTrackingConfig> for HsvSettings {
    fn from(value: &HsvTrackingConfig) -> Self {
        Self {
            enabled: value.enabled,
            range: HsvRange {
                lower: value.lower_bound,
                upper: value.upper_bound,
            },
            morphology_kernel_size: value.morphology_kernel_size,
            min_contour_area: value.min_contour_area,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Yolo26Config {
    pub enabled: bool,
    pub onnx_model_path: PathBuf,
    pub class_names_path: PathBuf,
    pub confidence_threshold: f32,
    pub max_detections: usize,
    pub input_size: [u32; 2],
    /// Ignored until EP selection UI / EP 선택 UI 전까지 하드코딩 [directml, cpu].
    pub execution_providers: Vec<String>,
    pub selected_gpu_id: usize,
}

impl Default for Yolo26Config {
    fn default() -> Self {
        Self {
            enabled: true,
            onnx_model_path: PathBuf::from("models/yolo26n.onnx"),
            class_names_path: PathBuf::from("models/coco_classes.txt"),
            confidence_threshold: 0.25,
            max_detections: 100,
            input_size: [640, 640],
            execution_providers: vec!["directml".to_owned(), "cpu".to_owned()],
            selected_gpu_id: 0,
        }
    }
}

impl From<&Yolo26Config> for InferenceSettings {
    fn from(value: &Yolo26Config) -> Self {
        Self {
            model_path: value.onnx_model_path.clone(),
            class_names_path: value.class_names_path.clone(),
            input_size: Size2D::new(value.input_size[0], value.input_size[1]),
            confidence_threshold: value.confidence_threshold,
            max_detections: value.max_detections,
            selected_device_id: value.selected_gpu_id,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VisionAlgorithms {
    /// Display / routing hint only today / 현재는 표시용 (파이프라인은 토글 병행).
    pub selected_algorithm: String,
    pub hsv_tracking: HsvTrackingConfig,
    pub yolo26_detection: Yolo26Config,
}

impl Default for VisionAlgorithms {
    fn default() -> Self {
        Self {
            selected_algorithm: "yolo26".to_owned(),
            hsv_tracking: HsvTrackingConfig::default(),
            yolo26_detection: Yolo26Config::default(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppConfig {
    pub production_system: ProductionSystem,
    pub vision_algorithms: VisionAlgorithms,
    pub analytics: AnalyticsConfig,
    pub gui: GuiConfig,
    pub performance: PerformanceConfig,
    #[serde(default)]
    pub compatibility: CompatibilityConfig,
}

impl Default for AppConfig {
    fn default() -> Self {
        Self {
            production_system: ProductionSystem::default(),
            vision_algorithms: VisionAlgorithms::default(),
            analytics: AnalyticsConfig::default(),
            gui: GuiConfig::default(),
            performance: PerformanceConfig::default(),
            compatibility: CompatibilityConfig::default(),
        }
    }
}

/// Explicit AppConfig → runtime mapping layer (dead-field documentation lives on structs).
#[derive(Debug, Clone)]
pub struct RuntimeBindings {
    pub capture: CaptureOptions,
    pub hsv: HsvSettings,
    pub inference: InferenceSettings,
    pub yolo_enabled: bool,
    pub preview_enabled: bool,
    pub model_input: ModelInputSize,
    pub capture_roi: CaptureRoi,
}

impl RuntimeBindings {
    #[must_use]
    pub fn from_config(config: &AppConfig, preview_enabled: bool) -> Self {
        Self::from_config_with_preference(
            config,
            preview_enabled,
            CaptureBackendPreference::Auto,
        )
    }

    #[must_use]
    pub fn from_config_with_preference(
        config: &AppConfig,
        preview_enabled: bool,
        backend_preference: CaptureBackendPreference,
    ) -> Self {
        let inference: InferenceSettings = (&config.vision_algorithms.yolo26_detection).into();
        let model_input = ModelInputSize::new(inference.input_size.width, inference.input_size.height);
        Self {
            capture: CaptureOptions {
                include_cursor: false,
                draw_border: false,
                target_fps: config.performance.target_fps,
                buffer_depth: config.performance.frame_buffer_size.max(1),
                compatibility: (&config.compatibility).into(),
                backend_preference,
            },
            hsv: (&config.vision_algorithms.hsv_tracking).into(),
            inference,
            yolo_enabled: config.vision_algorithms.yolo26_detection.enabled,
            preview_enabled,
            model_input,
            // Placeholder rect; pipeline recomputes centered ROI from frame size.
            capture_roi: CaptureRoi {
                rect: capture_core::RectI::new(0, 0, 320, 320),
            },
        }
    }
}

impl AppConfig {
    pub fn load(path: impl AsRef<Path>) -> Result<Self, ConfigError> {
        let raw = fs::read_to_string(path)?;
        Self::load_from_str(&raw)
    }

    pub fn load_from_candidates(paths: &[PathBuf]) -> Result<Self, ConfigError> {
        for path in paths {
            if path.exists() {
                return Self::load(path);
            }
        }

        Ok(Self::default())
    }

    pub fn load_from_str(raw: &str) -> Result<Self, ConfigError> {
        let mut value = serde_json::to_value(Self::default())?;
        let incoming: Value = serde_json::from_str(raw)?;
        merge_values(&mut value, incoming);
        Ok(serde_json::from_value(value)?)
    }

    pub fn save(&self, path: impl AsRef<Path>) -> Result<(), ConfigError> {
        if let Some(parent) = path.as_ref().parent() {
            fs::create_dir_all(parent)?;
        }
        let body = serde_json::to_string_pretty(self)?;
        fs::write(path, body)?;
        Ok(())
    }

    #[must_use]
    pub fn compatibility_policy(&self) -> CompatibilityPolicy {
        (&self.compatibility).into()
    }

    #[must_use]
    pub fn runtime_bindings(
        &self,
        preview_enabled: bool,
        backend_preference: CaptureBackendPreference,
    ) -> RuntimeBindings {
        RuntimeBindings::from_config_with_preference(self, preview_enabled, backend_preference)
    }
}

fn merge_values(base: &mut Value, incoming: Value) {
    match (base, incoming) {
        (Value::Object(base_map), Value::Object(incoming_map)) => {
            for (key, value) in incoming_map {
                match base_map.get_mut(&key) {
                    Some(existing) => merge_values(existing, value),
                    None => {
                        base_map.insert(key, value);
                    }
                }
            }
        }
        (base_slot, incoming_value) => {
            *base_slot = incoming_value;
        }
    }
}

#[cfg(test)]
mod tests {
    use std::path::PathBuf;

    use super::AppConfig;

    #[test]
    fn merges_yolo_only_config() {
        let raw = r#"{
            "vision_algorithms": {
                "selected_algorithm": "yolo26",
                "yolo26_detection": {
                    "enabled": true,
                    "onnx_model_path": "models/yolo26n.onnx",
                    "class_names_path": "models/coco_classes.txt",
                    "confidence_threshold": 0.35,
                    "max_detections": 42,
                    "input_size": [640, 640],
                    "execution_providers": ["directml", "cpu"],
                    "selected_gpu_id": 1
                }
            }
        }"#;

        let config = AppConfig::load_from_str(raw).expect("config should load");
        assert_eq!(config.vision_algorithms.selected_algorithm, "yolo26");
        assert_eq!(
            config
                .vision_algorithms
                .yolo26_detection
                .confidence_threshold,
            0.35
        );
        assert!(config.analytics.enabled);
        assert_eq!(config.performance.target_fps, 60);
    }

    #[test]
    fn preserves_default_structure_on_empty_input() {
        let config = AppConfig::load_from_str("{}").expect("config should load");
        assert_eq!(config.production_system.version, "1.0.0");
        assert_eq!(
            config.vision_algorithms.hsv_tracking.lower_bound,
            [140, 120, 180]
        );
    }

    #[test]
    fn runtime_bindings_map_frame_buffer_size() {
        let mut config = AppConfig::default();
        config.performance.frame_buffer_size = 7;
        let bindings = config.runtime_bindings(false, Default::default());
        assert_eq!(bindings.capture.buffer_depth, 7);
        assert_eq!(bindings.capture.target_fps, 60);
    }

    #[test]
    fn save_creates_parent_directories() {
        let unique = format!(
            "capture-first-config-test-{}",
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .expect("system time should be valid")
                .as_nanos()
        );
        let root = std::env::temp_dir().join(unique);
        let path: PathBuf = root.join("nested").join("config.json");

        AppConfig::default()
            .save(&path)
            .expect("save should create parent directories");

        assert!(path.exists());
        let _ = std::fs::remove_file(&path);
        let _ = std::fs::remove_dir_all(root);
    }
}
