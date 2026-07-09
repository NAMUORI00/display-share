use std::path::PathBuf;

use capture_core::{
    CaptureBackendKind, CaptureBackendPreference, CaptureTarget, InferenceDiagnostics,
    PerformanceSnapshot, ProviderState, backend_kind_label, backend_preference_label,
};
use config::AppConfig;
use egui::{self, ColorImage, RichText, TextureHandle, TextureOptions, Vec2};
use egui_dock::{DockArea, DockState, TabViewer};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WorkspaceTab {
    Overview,
    Control,
    Metrics,
    Logs,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UiCommand {
    RefreshTargets,
    StartCapture,
    StopCapture,
    LoadModel,
    SaveSettings,
}

/// Display + draft edit state. App owns authoritative config; SaveSettings persists draft.
#[derive(Debug, Clone)]
pub struct UiModel {
    pub targets: Vec<CaptureTarget>,
    pub selected_target: usize,
    pub capture_running: bool,
    pub performance: PerformanceSnapshot,
    pub provider_state: ProviderState,
    pub backend_label: String,
    pub backend_preference: CaptureBackendPreference,
    pub active_backend: Option<CaptureBackendKind>,
    pub last_error: Option<String>,
    pub logs: Vec<String>,
    /// Draft settings edited in Control tab (app persists on SaveSettings).
    pub config: AppConfig,
    pub roi_preview: Option<ColorImage>,
    pub roi_preview_version: u64,
    pub preview_enabled: bool,
    pub hsv_hit_count: usize,
    pub yolo_detection_count: usize,
    pub inference_diagnostics: InferenceDiagnostics,
    pub config_path: Option<PathBuf>,
}

impl Default for UiModel {
    fn default() -> Self {
        Self {
            targets: Vec::new(),
            selected_target: 0,
            capture_running: false,
            performance: PerformanceSnapshot::default(),
            provider_state: ProviderState::Uninitialized,
            backend_label: "Windows capture selector".to_owned(),
            backend_preference: CaptureBackendPreference::Auto,
            active_backend: None,
            last_error: None,
            logs: Vec::new(),
            config: AppConfig::default(),
            roi_preview: None,
            roi_preview_version: 0,
            preview_enabled: false,
            hsv_hit_count: 0,
            yolo_detection_count: 0,
            inference_diagnostics: InferenceDiagnostics::default(),
            config_path: None,
        }
    }
}

pub struct SmartCaptureUi {
    dock_state: DockState<WorkspaceTab>,
    pub model: UiModel,
    preview_texture: Option<TextureHandle>,
    last_preview_version: u64,
}

impl SmartCaptureUi {
    #[must_use]
    pub fn new(model: UiModel) -> Self {
        Self {
            dock_state: DockState::new(vec![
                WorkspaceTab::Overview,
                WorkspaceTab::Control,
                WorkspaceTab::Metrics,
                WorkspaceTab::Logs,
            ]),
            model,
            preview_texture: None,
            last_preview_version: 0,
        }
    }

    pub fn show(&mut self, ctx: &egui::Context) -> Vec<UiCommand> {
        let mut commands = Vec::new();
        self.sync_preview_texture(ctx);

        egui::TopBottomPanel::top("top_bar").show(ctx, |ui| {
            ui.horizontal(|ui| {
                ui.heading("SmartScreenCapture Rust");
                ui.separator();
                ui.label(format!("Backend: {}", self.model.backend_label));
                ui.separator();
                ui.label(format!(
                    "Active: {}",
                    self.model
                        .active_backend
                        .map(backend_kind_label)
                        .unwrap_or("idle")
                ));
                ui.separator();
                ui.label(format!(
                    "Provider: {}",
                    provider_label(&self.model.provider_state)
                ));
                if ui.button("Refresh Targets").clicked() {
                    commands.push(UiCommand::RefreshTargets);
                }
                if ui.button("Load Model").clicked() {
                    commands.push(UiCommand::LoadModel);
                }
                if ui.button("Save Settings").clicked() {
                    commands.push(UiCommand::SaveSettings);
                }
                let button = if self.model.capture_running {
                    "Stop Capture"
                } else {
                    "Start Capture"
                };
                if ui.button(button).clicked() {
                    commands.push(if self.model.capture_running {
                        UiCommand::StopCapture
                    } else {
                        UiCommand::StartCapture
                    });
                }
            });
        });

        let mut viewer = WorkspaceViewer {
            model: &mut self.model,
            preview_texture: self.preview_texture.as_ref(),
        };
        DockArea::new(&mut self.dock_state).show(ctx, &mut viewer);

        commands
    }

    fn sync_preview_texture(&mut self, ctx: &egui::Context) {
        if self.model.roi_preview_version == self.last_preview_version {
            return;
        }

        self.last_preview_version = self.model.roi_preview_version;
        match &self.model.roi_preview {
            Some(image) => {
                if let Some(texture) = &mut self.preview_texture {
                    texture.set(image.clone(), TextureOptions::LINEAR);
                } else {
                    self.preview_texture = Some(ctx.load_texture(
                        "roi-preview",
                        image.clone(),
                        TextureOptions::LINEAR,
                    ));
                }
            }
            None => {
                self.preview_texture = None;
            }
        }
    }
}

struct WorkspaceViewer<'a> {
    model: &'a mut UiModel,
    preview_texture: Option<&'a TextureHandle>,
}

impl TabViewer for WorkspaceViewer<'_> {
    type Tab = WorkspaceTab;

    fn title(&mut self, tab: &mut Self::Tab) -> egui::WidgetText {
        match tab {
            WorkspaceTab::Overview => "Overview".into(),
            WorkspaceTab::Control => "Control".into(),
            WorkspaceTab::Metrics => "Metrics".into(),
            WorkspaceTab::Logs => "Logs".into(),
        }
    }

    fn ui(&mut self, ui: &mut egui::Ui, tab: &mut Self::Tab) {
        match tab {
            WorkspaceTab::Overview => {
                ui.heading("Capture-first Rewrite");
                ui.label("The capture backend emits D3D11 textures by default. The preview shown here is a readback of the current ROI for operator visibility.");
                ui.separator();
                ui.label(format!(
                    "Configured target FPS: {}",
                    self.model.config.performance.target_fps
                ));
                ui.label(format!(
                    "Selected algorithm: {}",
                    self.model.config.vision_algorithms.selected_algorithm
                ));
                ui.label(format!(
                    "Backend preference: {}",
                    backend_preference_label(self.model.backend_preference)
                ));
                if let Some(path) = &self.model.config_path {
                    ui.label(format!("Config path: {}", path.display()));
                }
                ui.label(format!(
                    "Preview readback: {}",
                    if self.model.preview_enabled {
                        "enabled"
                    } else {
                        "disabled"
                    }
                ));
                ui.label(format!("HSV hits: {}", self.model.hsv_hit_count));
                ui.label(format!(
                    "YOLO detections: {}",
                    self.model.yolo_detection_count
                ));
                ui.label(format!(
                    "Loaded classes: {}",
                    self.model.inference_diagnostics.class_count
                ));
                if let Some(shape) = &self.model.inference_diagnostics.last_output_shape {
                    ui.label(format!("Last output shape: {}", format_shape(shape)));
                }
                if let Some(reason) = &self.model.inference_diagnostics.fallback_reason {
                    ui.label(format!("Fallback reason: {reason}"));
                }
                ui.separator();
                if let Some(texture) = self.preview_texture {
                    let size = fit_preview(texture.size_vec2(), ui.available_size());
                    ui.image((texture.id(), size));
                } else {
                    ui.label("ROI preview is not available yet.");
                }
                if let Some(error) = &self.model.last_error {
                    ui.colored_label(ui.visuals().warn_fg_color, error);
                }
            }
            WorkspaceTab::Control => {
                ui.heading("Targets");
                if self.model.targets.is_empty() {
                    ui.label("No capture targets enumerated yet.");
                } else {
                    for (index, target) in self.model.targets.iter().enumerate() {
                        let selected = self.model.selected_target == index;
                        if ui
                            .selectable_label(
                                selected,
                                format!(
                                    "{}{} ({}x{})",
                                    target.name,
                                    if target.primary { " [primary]" } else { "" },
                                    target.size.width,
                                    target.size.height
                                ),
                            )
                            .clicked()
                        {
                            self.model.selected_target = index;
                        }
                    }
                }

                if let Some(target) = self.model.targets.get(self.model.selected_target) {
                    ui.separator();
                    ui.heading("Selected Target");
                    ui.label(format!(
                        "Device: {}",
                        target.device_name.as_deref().unwrap_or("unknown")
                    ));
                    ui.label(format!(
                        "Adapter: {}",
                        target.adapter_name.as_deref().unwrap_or("unknown")
                    ));
                    ui.label(format!(
                        "Available backends: {}",
                        target
                            .available_backends
                            .iter()
                            .map(|backend| backend_kind_label(*backend))
                            .collect::<Vec<_>>()
                            .join(", ")
                    ));
                }

                ui.separator();
                ui.heading("Model");
                ui.label(format!(
                    "Model: {}",
                    self.model
                        .config
                        .vision_algorithms
                        .yolo26_detection
                        .onnx_model_path
                        .display()
                ));
                ui.label(format!(
                    "Classes: {}",
                    self.model
                        .config
                        .vision_algorithms
                        .yolo26_detection
                        .class_names_path
                        .display()
                ));
                ui.label(format!(
                    "GPU / DML device: {}",
                    self.model
                        .config
                        .vision_algorithms
                        .yolo26_detection
                        .selected_gpu_id
                ));
                ui.label(format!(
                    "EP order: {}",
                    self.model
                        .config
                        .vision_algorithms
                        .yolo26_detection
                        .execution_providers
                        .join(", ")
                ));
                if let Some(device) = &self
                    .model
                    .config
                    .vision_algorithms
                    .yolo26_detection
                    .openvino_device_type
                {
                    ui.label(format!("OpenVINO device: {device}"));
                } else {
                    ui.label("OpenVINO device: GPU then NPU (auto)");
                }
                ui.label(format!(
                    "Loaded classes: {}",
                    self.model.inference_diagnostics.class_count
                ));
                if let Some(graph_name) =
                    &self.model.inference_diagnostics.model_metadata.graph_name
                {
                    ui.label(format!("Graph: {graph_name}"));
                }
                if let Some(producer) = &self.model.inference_diagnostics.model_metadata.producer {
                    ui.label(format!("Producer: {producer}"));
                }
                if let Some(version) = self.model.inference_diagnostics.model_metadata.version {
                    ui.label(format!("Version: {version}"));
                }
                if let Some(reason) = &self.model.inference_diagnostics.fallback_reason {
                    ui.colored_label(
                        ui.visuals().warn_fg_color,
                        format!("Provider fallback chain: {reason}"),
                    );
                }
                if let Some(error) = &self.model.inference_diagnostics.last_error {
                    ui.colored_label(
                        ui.visuals().error_fg_color,
                        format!("Last inference error: {error}"),
                    );
                }
                ui.separator();
                ui.heading("Model I/O");
                render_descriptors(ui, "Inputs", &self.model.inference_diagnostics.inputs);
                render_descriptors(ui, "Outputs", &self.model.inference_diagnostics.outputs);
                if !self.model.inference_diagnostics.validation_notes.is_empty() {
                    ui.separator();
                    ui.heading("Validation Notes");
                    for note in &self.model.inference_diagnostics.validation_notes {
                        ui.colored_label(ui.visuals().warn_fg_color, note);
                    }
                }
                ui.separator();
                ui.heading("Runtime");
                egui::ComboBox::from_label("Capture Backend")
                    .selected_text(backend_preference_label(self.model.backend_preference))
                    .show_ui(ui, |ui| {
                        ui.selectable_value(
                            &mut self.model.backend_preference,
                            CaptureBackendPreference::Auto,
                            backend_preference_label(CaptureBackendPreference::Auto),
                        );
                        ui.selectable_value(
                            &mut self.model.backend_preference,
                            CaptureBackendPreference::WindowsGraphicsCapture,
                            backend_preference_label(
                                CaptureBackendPreference::WindowsGraphicsCapture,
                            ),
                        );
                        ui.selectable_value(
                            &mut self.model.backend_preference,
                            CaptureBackendPreference::DxgiDuplication,
                            backend_preference_label(CaptureBackendPreference::DxgiDuplication),
                        );
                    });
                egui::ComboBox::from_label("Preview Mode")
                    .selected_text(if self.model.preview_enabled {
                        "ROI readback"
                    } else {
                        "Off"
                    })
                    .show_ui(ui, |ui| {
                        ui.selectable_value(&mut self.model.preview_enabled, false, "Off");
                        ui.selectable_value(&mut self.model.preview_enabled, true, "ROI readback");
                    });
                ui.checkbox(
                    &mut self.model.config.vision_algorithms.hsv_tracking.enabled,
                    "Enable HSV tracking",
                );
                ui.checkbox(
                    &mut self.model.config.vision_algorithms.yolo26_detection.enabled,
                    "Enable YOLO detection",
                );
                ui.horizontal(|ui| {
                    ui.label("Target FPS");
                    ui.add(
                        egui::DragValue::new(&mut self.model.config.performance.target_fps)
                            .range(1..=360),
                    );
                });
                ui.horizontal(|ui| {
                    ui.label("Frame buffer");
                    ui.add(
                        egui::DragValue::new(
                            &mut self.model.config.performance.frame_buffer_size,
                        )
                        .range(1..=32),
                    );
                });
                ui.horizontal(|ui| {
                    ui.label("DML device");
                    ui.add(
                        egui::DragValue::new(
                            &mut self
                                .model
                                .config
                                .vision_algorithms
                                .yolo26_detection
                                .selected_gpu_id,
                        )
                        .range(0..=16),
                    );
                });
            }
            WorkspaceTab::Metrics => {
                ui.heading("Performance");
                metric(ui, "FPS", format!("{:.1}", self.model.performance.fps));
                metric(
                    ui,
                    "Frame time",
                    format!("{:.2} ms", self.model.performance.frame_time_ms),
                );
                metric(
                    ui,
                    "Processing time",
                    format!("{:.2} ms", self.model.performance.processing_time_ms),
                );
                metric(
                    ui,
                    "Dropped frames",
                    self.model.performance.dropped_frames.to_string(),
                );
                metric(
                    ui,
                    "Total frames",
                    self.model.performance.total_frames.to_string(),
                );
            }
            WorkspaceTab::Logs => {
                ui.heading("Logs");
                egui::ScrollArea::vertical().show(ui, |ui| {
                    for line in &self.model.logs {
                        ui.label(RichText::new(line).monospace());
                    }
                });
            }
        }
    }
}

fn provider_label(state: &ProviderState) -> String {
    match state {
        ProviderState::Uninitialized => "uninitialized".to_owned(),
        ProviderState::DirectMl {
            device_id,
            cpu_fallback,
        } => format!(
            "DirectML device {device_id}{}",
            if *cpu_fallback { " (fallback)" } else { "" }
        ),
        ProviderState::OpenVino { device_type } => format!("OpenVINO {device_type}"),
        ProviderState::CpuFallback => "CPU fallback".to_owned(),
        ProviderState::Failed(message) => format!("failed: {message}"),
    }
}

fn metric(ui: &mut egui::Ui, label: &str, value: String) {
    ui.horizontal(|ui| {
        ui.label(RichText::new(label).strong());
        ui.separator();
        ui.label(value);
    });
}

fn fit_preview(source: Vec2, available: Vec2) -> Vec2 {
    if source.x <= 0.0 || source.y <= 0.0 {
        return Vec2::ZERO;
    }

    let max_width = available.x.max(64.0);
    let max_height = available.y.max(64.0);
    let scale = (max_width / source.x).min(max_height / source.y).min(1.0);
    Vec2::new(source.x * scale, source.y * scale)
}

fn render_descriptors(
    ui: &mut egui::Ui,
    heading: &str,
    descriptors: &[capture_core::InferenceIoDescriptor],
) {
    ui.label(RichText::new(heading).strong());
    if descriptors.is_empty() {
        ui.label("none");
        return;
    }

    for descriptor in descriptors {
        ui.label(format!(
            "{}: {}{}{}",
            descriptor.name,
            descriptor.value_type,
            descriptor
                .tensor_shape
                .as_ref()
                .map(|shape| format!(" shape={}", format_shape(shape)))
                .unwrap_or_default(),
            descriptor
                .tensor_element_type
                .as_ref()
                .map(|element| format!(" element={element}"))
                .unwrap_or_default()
        ));
    }
}

fn format_shape(shape: &[i64]) -> String {
    let body = shape
        .iter()
        .map(|dim| dim.to_string())
        .collect::<Vec<_>>()
        .join(", ");
    format!("[{body}]")
}
