//! Thin eframe shell — capture pump + UI commands. Frame orchestration lives in `pipeline`.

use std::path::{Path, PathBuf};
use std::time::Instant;

use anyhow::Result;
use capture_core::{
    CaptureBackend, CaptureSession, CpuBuffer, InferenceBackend, InferenceDiagnostics, PixelFormat,
    backend_kind_label,
};
use capture_windows::WindowsCaptureBackend;
use config::AppConfig;
use eframe::egui;
use inference_dml::DirectMlInferenceBackend;
use pipeline::{FramePipeline, FramePipelineSettings};
use telemetry::TelemetryHub;
use ui::{SmartCaptureUi, UiCommand, UiModel};

fn main() -> Result<()> {
    tracing_subscriber::fmt().with_env_filter("info").init();

    let options = eframe::NativeOptions {
        renderer: eframe::Renderer::Wgpu,
        ..Default::default()
    };

    eframe::run_native(
        "SmartScreenCapture Rust",
        options,
        Box::new(|_cc| Ok(Box::new(DesktopApp::bootstrap()?))),
    )
    .map_err(|err| anyhow::anyhow!(err.to_string()))
}

struct DesktopApp {
    ui: SmartCaptureUi,
    capture_backend: WindowsCaptureBackend,
    capture_session: Option<Box<dyn CaptureSession>>,
    inference: DirectMlInferenceBackend,
    telemetry: TelemetryHub,
    pipeline: FramePipeline,
    config_path: PathBuf,
    repo_root: PathBuf,
}

impl DesktopApp {
    fn bootstrap() -> Result<Self> {
        let repo_root = discover_repo_root(std::env::current_dir()?);
        let config_paths = vec![repo_root.join("config").join("config.json")];
        let config_path = config_paths
            .iter()
            .find(|path| path.exists())
            .cloned()
            .unwrap_or_else(|| config_paths[0].clone());
        let mut config = AppConfig::load_from_candidates(&config_paths)?;
        resolve_model_paths(&mut config, &repo_root);

        let capture_backend = WindowsCaptureBackend::new();
        let targets = capture_backend.enumerate_targets().unwrap_or_default();

        let ui = SmartCaptureUi::new(UiModel {
            targets,
            backend_label: "Windows capture selector (WGC / DXGI)".to_owned(),
            config_path: Some(config_path.clone()),
            config,
            ..UiModel::default()
        });

        Ok(Self {
            ui,
            capture_backend,
            capture_session: None,
            inference: DirectMlInferenceBackend::default(),
            telemetry: TelemetryHub::default(),
            pipeline: FramePipeline::new(),
            config_path,
            repo_root,
        })
    }

    fn handle_command(&mut self, command: UiCommand) {
        match command {
            UiCommand::RefreshTargets => match self.capture_backend.enumerate_targets() {
                Ok(targets) => {
                    self.ui.model.targets = targets;
                    if self.ui.model.selected_target >= self.ui.model.targets.len() {
                        self.ui.model.selected_target = 0;
                    }
                    self.ui.model.last_error = None;
                    self.push_log("capture targets refreshed");
                }
                Err(err) => self.ui.model.last_error = Some(err.to_string()),
            },
            UiCommand::StartCapture => {
                if self.capture_session.is_some() {
                    return;
                }
                let Some(target) = self
                    .ui
                    .model
                    .targets
                    .get(self.ui.model.selected_target)
                    .cloned()
                else {
                    self.ui.model.last_error = Some("no capture target selected".to_owned());
                    return;
                };
                let bindings = self.ui.model.config.runtime_bindings(
                    self.ui.model.preview_enabled,
                    self.ui.model.backend_preference,
                );
                match self.capture_backend.start(&target, bindings.capture) {
                    Ok(session) => {
                        let backend = session.backend_kind();
                        self.capture_session = Some(session);
                        self.ui.model.capture_running = true;
                        self.ui.model.active_backend = Some(backend);
                        self.ui.model.last_error = None;
                        self.push_log(format!(
                            "capture started on {} via {}",
                            target.name,
                            backend_kind_label(backend)
                        ));
                    }
                    Err(err) => self.ui.model.last_error = Some(err.to_string()),
                }
            }
            UiCommand::StopCapture => {
                if let Some(session) = self.capture_session.take() {
                    let _ = session.stop();
                }
                self.ui.model.capture_running = false;
                self.ui.model.active_backend = None;
                self.push_log("capture stopped");
            }
            UiCommand::LoadModel => {
                let settings = (&self.ui.model.config.vision_algorithms.yolo26_detection).into();
                let providers = self
                    .ui
                    .model
                    .config
                    .vision_algorithms
                    .yolo26_detection
                    .execution_providers
                    .join(", ");
                self.push_log(format!("loading model with EP order: [{providers}]"));
                match self.inference.initialize(settings) {
                    Ok(state) => {
                        self.sync_inference_ui_state();
                        self.ui.model.provider_state = state;
                        self.ui.model.last_error = None;
                        self.push_log(format!(
                            "inference backend initialized: {}",
                            self.inference.backend_name()
                        ));
                        self.log_inference_diagnostics();
                    }
                    Err(err) => {
                        self.ui.model.last_error = Some(err.to_string());
                        self.sync_inference_ui_state();
                    }
                }
            }
            UiCommand::SaveSettings => {
                let mut config_to_save = self.ui.model.config.clone();
                relativize_model_paths(&mut config_to_save, &self.repo_root);
                match config_to_save.save(&self.config_path) {
                    Ok(()) => {
                        self.ui.model.last_error = None;
                        self.push_log(format!("settings saved to {}", self.config_path.display()));
                    }
                    Err(err) => {
                        self.ui.model.last_error = Some(err.to_string());
                    }
                }
            }
        }
    }

    fn pump_capture(&mut self) {
        let mut disconnected = false;
        let mut pending_packets = Vec::new();
        if let Some(session) = self.capture_session.as_ref() {
            self.ui.model.active_backend = Some(session.backend_kind());
            loop {
                match session.try_recv() {
                    Ok(Some(packet)) => {
                        pending_packets.push(packet);
                    }
                    Ok(None) => break,
                    Err(err) => {
                        disconnected = true;
                        self.ui.model.last_error = Some(err.to_string());
                        break;
                    }
                }
            }

            let stats = session.stats();
            self.telemetry.ingest_capture_stats(&stats);
        }

        for packet in pending_packets {
            let started_at = Instant::now();
            if let Err(err) = self.process_packet(packet) {
                self.ui.model.last_error = Some(err.to_string());
                self.push_log(format!("frame processing failed: {err}"));
            }
            let processing_ms = started_at.elapsed().as_secs_f64() * 1000.0;
            self.telemetry
                .on_frame_with_processing(Some(processing_ms));
        }

        if disconnected {
            self.capture_session = None;
            self.ui.model.capture_running = false;
            self.ui.model.active_backend = None;
        }

        self.ui.model.performance = self.telemetry.snapshot();
        self.sync_inference_ui_state();
    }

    fn process_packet(&mut self, packet: capture_core::CapturePacket) -> Result<()> {
        let bindings = self.ui.model.config.runtime_bindings(
            self.ui.model.preview_enabled,
            self.ui.model.backend_preference,
        );
        let settings = FramePipelineSettings::from(&bindings);
        let report = self
            .pipeline
            .process(&packet.frame, &settings, Some(&mut self.inference))?;

        self.ui.model.hsv_hit_count = report.hsv_hit_count();
        self.ui.model.yolo_detection_count = report.yolo_detection_count();
        if self.ui.model.preview_enabled {
            self.ui.model.roi_preview = report
                .preview_buffer
                .as_ref()
                .map(cpu_buffer_to_color_image);
            self.ui.model.roi_preview_version = self.ui.model.roi_preview_version.wrapping_add(1);
        } else if self.ui.model.roi_preview.is_some() {
            self.ui.model.roi_preview = None;
            self.ui.model.roi_preview_version = self.ui.model.roi_preview_version.wrapping_add(1);
        }

        Ok(())
    }

    fn sync_inference_ui_state(&mut self) {
        self.ui.model.provider_state = self.inference.provider_state();
        self.ui.model.inference_diagnostics = self.inference.diagnostics();
    }

    fn log_inference_diagnostics(&mut self) {
        let diagnostics = self.ui.model.inference_diagnostics.clone();
        self.push_log(inference_summary(&diagnostics));
        if let Some(reason) = diagnostics.fallback_reason {
            self.push_log(format!("provider fallback reason: {reason}"));
        }
        for note in diagnostics.validation_notes {
            self.push_log(format!("model validation: {note}"));
        }
    }

    fn push_log(&mut self, line: impl Into<String>) {
        self.ui.model.logs.push(line.into());
        if self.ui.model.logs.len() > 200 {
            self.ui.model.logs.remove(0);
        }
    }
}

impl eframe::App for DesktopApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        self.pump_capture();
        let commands = self.ui.show(ctx);
        for command in commands {
            self.handle_command(command);
        }
    }
}

fn discover_repo_root(start: PathBuf) -> PathBuf {
    for current in start.ancestors() {
        if current.join("capture-first").exists()
            || (current.join("config").exists() && current.join("models").exists())
        {
            return current.to_path_buf();
        }
    }

    Path::new(".").to_path_buf()
}

fn resolve_model_paths(config: &mut AppConfig, repo_root: &Path) {
    let yolo = &mut config.vision_algorithms.yolo26_detection;
    if yolo.onnx_model_path.is_relative() {
        yolo.onnx_model_path = repo_root.join(&yolo.onnx_model_path);
    }
    if yolo.class_names_path.is_relative() {
        yolo.class_names_path = repo_root.join(&yolo.class_names_path);
    }
}

fn relativize_model_paths(config: &mut AppConfig, repo_root: &Path) {
    let yolo = &mut config.vision_algorithms.yolo26_detection;
    if let Ok(path) = yolo.onnx_model_path.strip_prefix(repo_root) {
        yolo.onnx_model_path = path.to_path_buf();
    }
    if let Ok(path) = yolo.class_names_path.strip_prefix(repo_root) {
        yolo.class_names_path = path.to_path_buf();
    }
}

fn cpu_buffer_to_color_image(buffer: &CpuBuffer) -> egui::ColorImage {
    let mut rgba = vec![0u8; (buffer.width * buffer.height * 4) as usize];
    for y in 0..buffer.height as usize {
        for x in 0..buffer.width as usize {
            let src = y * buffer.stride as usize + x * 4;
            let dst = (y * buffer.width as usize + x) * 4;
            match buffer.pixel_format {
                PixelFormat::Bgra8Unorm => {
                    rgba[dst] = buffer.data[src + 2];
                    rgba[dst + 1] = buffer.data[src + 1];
                    rgba[dst + 2] = buffer.data[src];
                    rgba[dst + 3] = buffer.data[src + 3];
                }
                PixelFormat::Rgba8Unorm => {
                    rgba[dst] = buffer.data[src];
                    rgba[dst + 1] = buffer.data[src + 1];
                    rgba[dst + 2] = buffer.data[src + 2];
                    rgba[dst + 3] = buffer.data[src + 3];
                }
            }
        }
    }

    egui::ColorImage::from_rgba_unmultiplied([buffer.width as usize, buffer.height as usize], &rgba)
}

fn inference_summary(diagnostics: &InferenceDiagnostics) -> String {
    let graph = diagnostics
        .model_metadata
        .graph_name
        .as_deref()
        .unwrap_or("unnamed graph");
    format!(
        "model ready: {graph}; inputs={}, outputs={}, classes={}",
        diagnostics.inputs.len(),
        diagnostics.outputs.len(),
        diagnostics.class_count
    )
}
