//! Eframe application shell: capture orchestration and UI state synchronization.
//! D3D11 stays on the DXGI capture thread. Monitor preview is a CPU ColorImage channel.

use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

use anyhow::Result;
use capture_core::{
    AnalysisFrame, CaptureBackend, CaptureRoi, CaptureSession, HsvMaskStats, HsvSettings,
    ProviderState,
};
use capture_windows::{WindowsCaptureBackend, exclude_own_windows_from_capture};
use config::{
    AppConfig, HsvPickerSettings, hsv_settings_path_for_config, load_hsv_picker_settings,
    save_hsv_picker_settings,
};
use eframe::egui;
use pipeline::FramePipelineSettings;
use telemetry::TelemetryHub;
use ui::{SmartCaptureUi, UiCommand, UiModel};
use vision_gpu::detect_hsv_with_preview;

use crate::image_presenter::{apply_hsv_tune_to_model, cpu_buffer_to_color_image};
use crate::vision_worker::{VisionWorker, WorkerEvent};

/// UI event-loop cadence while capturing (~60 Hz paint). Capture thread runs faster.
const CAPTURE_REPAINT_INTERVAL: Duration = Duration::from_millis(16);
/// Throttle UI-thread HSV re-tune while dragging sliders.
const HSV_TUNE_INTERVAL: Duration = Duration::from_millis(100);

pub fn run() -> Result<()> {
    // Keep diagnostics visible by default; override with RUST_LOG when needed.
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new("info")),
        )
        .init();

    let options = eframe::NativeOptions {
        renderer: eframe::Renderer::Wgpu,
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([440.0, 340.0])
            .with_min_inner_size([380.0, 280.0]),
        ..Default::default()
    };

    let app = DesktopApp::bootstrap()?;
    let window_title = app.ui.model.config.privacy.window_title().to_owned();
    eframe::run_native(
        &window_title,
        options,
        Box::new(move |_cc| Ok(Box::new(app))),
    )
    .map_err(|err| anyhow::anyhow!(err.to_string()))
}

struct DesktopApp {
    ui: SmartCaptureUi,
    capture_backend: WindowsCaptureBackend,
    capture_session: Option<Box<dyn CaptureSession>>,
    worker: VisionWorker,
    telemetry: TelemetryHub,
    config_path: PathBuf,
    repo_root: PathBuf,
    session_fingerprint: Option<SessionFingerprint>,
    last_preview_enabled: bool,
    last_analysis_enabled: bool,
    yolo_autoload_done: bool,
    last_analysis: Option<AnalysisFrame>,
    hsv_tune_dirty: bool,
    last_hsv_tune_at: Instant,
    last_monitor_open: bool,
    last_hsv_enabled: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct SessionFingerprint {
    selected_target: usize,
    target_id: String,
    target_fps: u32,
    buffer_depth: u32,
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
        let hsv_path = hsv_settings_path_for_config(&config_path);
        if let Ok(mut picker) = load_hsv_picker_settings(&hsv_path) {
            picker.clamp_min_max();
            picker.apply_to(&mut config.vision_algorithms.hsv_tracking);
        }

        let capture_backend = WindowsCaptureBackend::new();
        let targets = capture_backend.enumerate_targets().unwrap_or_default();

        let ui = SmartCaptureUi::new(UiModel {
            targets,
            backend_label: "Display".to_owned(),
            config_path: Some(config_path.clone()),
            config,
            ..UiModel::default()
        });
        let last_hsv_enabled = ui.model.config.vision_algorithms.hsv_tracking.enabled;

        Ok(Self {
            ui,
            capture_backend,
            capture_session: None,
            worker: VisionWorker::spawn(),
            telemetry: TelemetryHub::default(),
            config_path,
            repo_root,
            session_fingerprint: None,
            last_preview_enabled: false,
            last_analysis_enabled: false,
            yolo_autoload_done: false,
            last_analysis: None,
            hsv_tune_dirty: true,
            last_hsv_tune_at: Instant::now() - HSV_TUNE_INTERVAL,
            last_monitor_open: false,
            last_hsv_enabled,
        })
    }

    fn current_fingerprint(&self) -> Option<SessionFingerprint> {
        let target = self.ui.model.targets.get(self.ui.model.selected_target)?;
        Some(SessionFingerprint {
            selected_target: self.ui.model.selected_target,
            target_id: target.id.clone(),
            target_fps: self.ui.model.config.performance.target_fps,
            buffer_depth: self.ui.model.config.performance.frame_buffer_size as u32,
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
                    self.push_log("targets refreshed");
                }
                Err(err) => self.ui.model.last_error = Some(err.to_string()),
            },
            UiCommand::StartCapture => self.start_capture(),
            UiCommand::StopCapture => self.stop_capture(),
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
                self.worker.load_model(settings);
            }
            UiCommand::SaveSettings => {
                let mut config_to_save = self.ui.model.config.clone();
                relativize_model_paths(&mut config_to_save, &self.repo_root);
                match config_to_save.save(&self.config_path) {
                    Ok(()) => {
                        self.ui.model.last_error = None;
                        self.push_log("settings saved");
                    }
                    Err(err) => {
                        self.ui.model.last_error = Some(err.to_string());
                    }
                }
            }
            UiCommand::LoadHsvSettings => {
                let path = self.hsv_settings_path();
                match load_hsv_picker_settings(&path) {
                    Ok(mut picker) => {
                        picker.clamp_min_max();
                        picker.apply_to(&mut self.ui.model.config.vision_algorithms.hsv_tracking);
                        self.ui.mark_hsv_tune_dirty();
                        self.ui.model.last_error = None;
                        self.push_log("HSV settings loaded");
                    }
                    Err(err) => self.ui.model.last_error = Some(err.to_string()),
                }
            }
            UiCommand::SaveHsvSettings => {
                let path = self.hsv_settings_path();
                let mut picker = HsvPickerSettings::from_tracking(
                    &self.ui.model.config.vision_algorithms.hsv_tracking,
                );
                picker.clamp_min_max();
                match save_hsv_picker_settings(&path, &picker) {
                    Ok(()) => {
                        self.ui.model.last_error = None;
                        self.push_log("HSV settings saved");
                    }
                    Err(err) => self.ui.model.last_error = Some(err.to_string()),
                }
            }
        }
    }

    fn hsv_settings_path(&self) -> PathBuf {
        hsv_settings_path_for_config(&self.config_path)
    }

    fn start_capture(&mut self) {
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
        let bindings = self
            .ui
            .model
            .config
            .runtime_bindings(self.ui.model.preview_enabled);
        let window_exclusion = self
            .ui
            .model
            .config
            .privacy
            .exclude_own_windows_active()
            .then(exclude_own_windows_from_capture);
        match self.capture_backend.start(&target, bindings.capture) {
            Ok(session) => {
                let backend = session.backend_kind();
                self.capture_session = Some(session);
                self.ui.model.capture_running = true;
                self.ui.model.active_backend = Some(backend);
                self.ui.model.last_error = None;
                self.session_fingerprint = self.current_fingerprint();
                self.last_preview_enabled = false;
                self.last_analysis_enabled = false;
                if let Some(result) = window_exclusion {
                    self.push_log(format!(
                        "privacy: excluded {} own window(s) from local capture preview; {} failure(s)",
                        result.applied, result.failed
                    ));
                }
                self.push_log(format!(
                    "sharing started on {} @ {} FPS",
                    target.name, self.ui.model.config.performance.target_fps
                ));
            }
            Err(err) => self.ui.model.last_error = Some(err.to_string()),
        }
    }

    fn stop_capture(&mut self) {
        if let Some(session) = self.capture_session.take() {
            session.set_preview_enabled(false);
            session.set_analysis_enabled(false);
            // Drain residual preview/analysis so stale frames do not linger.
            while session.try_recv_preview().ok().flatten().is_some() {}
            while session.try_recv_analysis().ok().flatten().is_some() {}
            while session.try_recv().ok().flatten().is_some() {}
            let _ = session.stop();
        }
        self.ui.model.capture_running = false;
        self.ui.model.active_backend = None;
        self.session_fingerprint = None;
        self.last_preview_enabled = false;
        self.last_analysis_enabled = false;
        self.ui.model.preview_frame = None;
        self.ui.model.preview_frame_version = self.ui.model.preview_frame_version.wrapping_add(1);
        self.last_analysis = None;
        self.clear_hsv_tune_previews();
        self.push_log("sharing stopped");
    }

    fn maybe_restart_capture_for_option_changes(&mut self) {
        if self.capture_session.is_none() {
            return;
        }
        let Some(current) = self.current_fingerprint() else {
            return;
        };
        let Some(previous) = self.session_fingerprint.as_ref() else {
            return;
        };
        if current == *previous {
            return;
        }
        self.push_log("session options changed — restarting…");
        self.stop_capture();
        self.start_capture();
    }

    fn sync_session_gates(&mut self) {
        let want_preview = self.ui.monitor_is_open() && self.ui.model.preview_enabled;
        let yolo_on = self
            .ui
            .model
            .config
            .vision_algorithms
            .yolo26_detection
            .enabled;
        let hsv_on = self.ui.model.config.vision_algorithms.hsv_tracking.enabled;
        let want_analysis = hsv_on || yolo_on;

        if yolo_on {
            self.maybe_autoload_yolo_model();
        } else {
            self.yolo_autoload_done = false;
        }

        if let Some(session) = self.capture_session.as_ref() {
            if want_preview != self.last_preview_enabled {
                session.set_preview_enabled(want_preview);
                self.last_preview_enabled = want_preview;
                if !want_preview && self.ui.model.preview_frame.is_some() {
                    self.ui.model.preview_frame = None;
                    self.ui.model.preview_frame_version =
                        self.ui.model.preview_frame_version.wrapping_add(1);
                }
            }
            if want_analysis != self.last_analysis_enabled {
                session.set_analysis_enabled(want_analysis);
                self.last_analysis_enabled = want_analysis;
            }
        }
    }

    fn maybe_autoload_yolo_model(&mut self) {
        if self.yolo_autoload_done {
            return;
        }
        if !matches!(
            self.ui.model.provider_state,
            ProviderState::Uninitialized | ProviderState::Failed(_)
        ) {
            self.yolo_autoload_done = true;
            return;
        }
        let model_path = self
            .ui
            .model
            .config
            .vision_algorithms
            .yolo26_detection
            .onnx_model_path
            .clone();
        if !model_path.exists() {
            self.ui.model.last_error = Some(format!(
                "YOLO model not found: {} — place yolo26n.onnx in models/ (see models/README.md)",
                model_path.display()
            ));
            self.yolo_autoload_done = true;
            return;
        }
        self.yolo_autoload_done = true;
        let settings = (&self.ui.model.config.vision_algorithms.yolo26_detection).into();
        self.push_log("vision enabled — loading model…");
        self.worker.load_model(settings);
    }

    fn pump_capture(&mut self) {
        self.maybe_restart_capture_for_option_changes();
        self.drain_worker_events();
        self.sync_session_gates();

        let mut disconnected = false;
        let mut latest_packet_size = None;
        let mut skipped = 0u64;
        let mut captured = 0u64;

        if let Some(session) = self.capture_session.as_ref() {
            self.ui.model.active_backend = Some(session.backend_kind());

            // Drain GPU frame channel for FPS accounting only — do not submit D3D11 to worker.
            loop {
                match session.try_recv() {
                    Ok(Some(packet)) => {
                        captured += 1;
                        if latest_packet_size.replace(packet.frame.size()).is_some() {
                            skipped += 1;
                        }
                    }
                    Ok(None) => break,
                    Err(err) => {
                        disconnected = true;
                        self.ui.model.last_error = Some(err.to_string());
                        break;
                    }
                }
            }

            // Drain first, then convert only the latest preview. Converting inside the loop can
            // keep the UI thread chasing newly produced frames indefinitely in debug builds.
            let mut latest_preview = None;
            loop {
                match session.try_recv_preview() {
                    Ok(Some(preview)) => {
                        let _ = latest_preview.replace(preview);
                    }
                    Ok(None) => break,
                    Err(_) => break,
                }
            }
            if let Some(preview) = latest_preview {
                self.ui.model.capture_frame_size = Some(preview.capture_size);
                self.ui.model.preview_scale = preview.scale;
                self.ui.model.preview_frame = Some(cpu_buffer_to_color_image(&preview.buffer));
                self.ui.model.preview_frame_version =
                    self.ui.model.preview_frame_version.wrapping_add(1);
            }

            // Analysis ROI: CPU-only submit to vision worker.
            let mut latest_analysis = None;
            loop {
                match session.try_recv_analysis() {
                    Ok(Some(frame)) => {
                        let _ = latest_analysis.replace(frame);
                    }
                    Ok(None) => break,
                    Err(_) => break,
                }
            }
            if let Some(analysis) = latest_analysis {
                self.last_analysis = Some(analysis.clone());
                self.ui.model.capture_frame_size = Some(analysis.capture_size);
                let bindings = self
                    .ui
                    .model
                    .config
                    .runtime_bindings(self.ui.model.preview_enabled);
                let mut settings = FramePipelineSettings::from(&bindings);
                settings.want_preview_buffer = false;
                settings.preview_enabled = false;
                if !self.worker.try_submit(analysis, settings) {
                    self.telemetry.on_drop(1);
                }
            }

            let stats = session.stats();
            self.telemetry.ingest_capture_stats(&stats);
        }

        if skipped > 0 {
            self.telemetry.on_drop(skipped);
        }
        self.telemetry.on_frames(captured.saturating_sub(skipped));
        if let Some(size) = latest_packet_size {
            self.ui.model.capture_frame_size = Some(size);
        }

        if disconnected {
            self.capture_session = None;
            self.ui.model.capture_running = false;
            self.ui.model.active_backend = None;
            self.session_fingerprint = None;
            self.last_preview_enabled = false;
            self.last_analysis_enabled = false;
        }

        self.ui.model.performance = self.telemetry.snapshot();
    }

    fn clear_hsv_tune_previews(&mut self) {
        self.ui.reset_hsv_runtime_state();
    }

    /// When HSV is turned off, drop stale View overlays immediately.
    fn sync_hsv_disable_cleanup(&mut self) {
        let hsv_on = self.ui.model.config.vision_algorithms.hsv_tracking.enabled;
        if self.last_hsv_enabled && !hsv_on {
            self.ui.reset_hsv_runtime_state();
            self.hsv_tune_dirty = false;
        }
        self.last_hsv_enabled = hsv_on;
    }

    fn refresh_hsv_tune_if_needed(&mut self) {
        if !self.ui.needs_hsv_tune_refresh() {
            return;
        }
        let Some(analysis) = self.last_analysis.as_ref() else {
            return;
        };
        let now = Instant::now();
        if !self.hsv_tune_dirty && now.duration_since(self.last_hsv_tune_at) < HSV_TUNE_INTERVAL {
            return;
        }

        let hsv_settings: HsvSettings =
            (&self.ui.model.config.vision_algorithms.hsv_tracking).into();
        if !hsv_settings.enabled {
            return;
        }

        let tune =
            detect_hsv_with_preview(&analysis.roi_buffer, &hsv_settings, analysis.capture_size);
        apply_hsv_tune_to_model(&mut self.ui.model, &tune, &analysis.roi_buffer);
        self.hsv_tune_dirty = false;
        self.last_hsv_tune_at = now;
    }

    fn drain_worker_events(&mut self) {
        while let Some(event) = self.worker.try_recv_event() {
            match event {
                WorkerEvent::Frame {
                    hsv,
                    hsv_tune,
                    yolo_detections,
                    capture_roi,
                    capture_size,
                    model_input,
                    processing_ms,
                    provider_state,
                } => {
                    let hsv_on = self.ui.model.config.vision_algorithms.hsv_tracking.enabled;
                    let yolo_on = self
                        .ui
                        .model
                        .config
                        .vision_algorithms
                        .yolo26_detection
                        .enabled;
                    // Never re-apply HSV results after the feature is disabled.
                    if hsv_on {
                        self.ui.model.hsv = hsv;
                        if let (Some(tune), Some(analysis)) =
                            (hsv_tune, self.last_analysis.as_ref())
                        {
                            apply_hsv_tune_to_model(
                                &mut self.ui.model,
                                &tune,
                                &analysis.roi_buffer,
                            );
                            self.hsv_tune_dirty = false;
                            self.last_hsv_tune_at = Instant::now();
                        }
                    } else {
                        self.ui.model.hsv = HsvMaskStats::default();
                    }
                    self.ui.model.yolo_detections =
                        if yolo_on { yolo_detections } else { Vec::new() };
                    self.ui.model.capture_roi = visible_capture_roi(hsv_on, yolo_on, capture_roi);
                    self.ui.model.capture_frame_size = Some(capture_size);
                    self.ui.model.model_input_size = Some(model_input);
                    self.ui.model.provider_state = provider_state;
                    self.telemetry.set_processing_ms(processing_ms);
                }
                WorkerEvent::ModelReady {
                    state,
                    diagnostics,
                    summary,
                } => {
                    let diagnostics = *diagnostics;
                    self.ui.model.provider_state = state;
                    self.ui.model.inference_diagnostics = diagnostics.clone();
                    self.ui.model.last_error = None;
                    self.push_log(format!(
                        "inference backend initialized: {}",
                        provider_label_short(&self.ui.model.provider_state)
                    ));
                    self.push_log(summary);
                    if let Some(reason) = diagnostics.fallback_reason {
                        self.push_log(format!("provider fallback reason: {reason}"));
                    }
                    for note in diagnostics.validation_notes {
                        self.push_log(format!("model validation: {note}"));
                    }
                }
                WorkerEvent::Failed(message) => {
                    if self
                        .ui
                        .model
                        .config
                        .vision_algorithms
                        .yolo26_detection
                        .enabled
                        && matches!(
                            self.ui.model.provider_state,
                            ProviderState::Uninitialized | ProviderState::Failed(_)
                        )
                    {
                        self.yolo_autoload_done = false;
                    }
                    self.ui.model.last_error = Some(message.clone());
                    self.push_log(format!("frame processing failed: {message}"));
                }
            }
        }
    }

    fn push_log(&mut self, line: impl Into<String>) {
        self.ui.model.logs.push(line.into());
        if self.ui.model.logs.len() > 200 {
            self.ui.model.logs.remove(0);
        }
    }
}

fn visible_capture_roi(
    hsv_enabled: bool,
    yolo_enabled: bool,
    roi: CaptureRoi,
) -> Option<CaptureRoi> {
    (hsv_enabled || yolo_enabled).then_some(roi)
}

impl eframe::App for DesktopApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        self.pump_capture();
        self.sync_hsv_disable_cleanup();
        if self.ui.model.hsv_tune_dirty {
            self.hsv_tune_dirty = true;
        }
        self.refresh_hsv_tune_if_needed();
        let commands = self.ui.show(ctx);
        for command in commands {
            self.handle_command(command);
        }
        // Keep gates in sync after UI toggles Monitor open/close this frame.
        let monitor_was_open = self.last_monitor_open;
        self.sync_session_gates();
        let monitor_open = self.ui.monitor_is_open();
        // Apply window exclusion only when needed (not every frame).
        if self.ui.model.config.privacy.exclude_own_windows_active()
            && monitor_open
            && !monitor_was_open
        {
            let result = exclude_own_windows_from_capture();
            self.push_log(format!(
                "privacy: refreshed own-window capture exclusion for preview; {} applied, {} failed",
                result.applied, result.failed
            ));
        }
        self.last_monitor_open = monitor_open;
        if self.ui.model.capture_running || self.ui.needs_hsv_tune_refresh() {
            ctx.request_repaint_after(CAPTURE_REPAINT_INTERVAL);
        }
        if self.ui.model.hsv_tune_dirty {
            ctx.request_repaint();
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

fn provider_label_short(state: &ProviderState) -> String {
    match state {
        ProviderState::Uninitialized => "uninitialized".to_owned(),
        ProviderState::DirectMl {
            device_id,
            cpu_fallback,
        } => {
            if *cpu_fallback {
                format!("DirectML#{device_id}+CPU")
            } else {
                format!("DirectML#{device_id}")
            }
        }
        ProviderState::OpenVino { device_type } => format!("OpenVINO/{device_type}"),
        ProviderState::CpuFallback => "CPU".to_owned(),
        ProviderState::Failed(_) => "failed".to_owned(),
    }
}

#[cfg(test)]
mod tests {
    use capture_core::{CaptureRoi, RectI};

    use super::visible_capture_roi;

    #[test]
    fn hides_stale_roi_when_all_analysis_is_disabled() {
        let roi = CaptureRoi {
            rect: RectI::new(10, 20, 320, 320),
        };

        assert_eq!(visible_capture_roi(false, false, roi), None);
        assert_eq!(visible_capture_roi(true, false, roi), Some(roi));
        assert_eq!(visible_capture_roi(false, true, roi), Some(roi));
    }
}
