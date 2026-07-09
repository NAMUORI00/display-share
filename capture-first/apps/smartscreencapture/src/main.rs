//! Thin eframe shell — capture pump on UI thread; vision on a CPU-only worker.
//! D3D11 stays on the DXGI capture thread. Monitor preview is a CPU ColorImage channel.

use std::path::{Path, PathBuf};
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

use anyhow::Result;
use capture_core::{
    AnalysisFrame, CaptureBackend, CaptureBackendPreference, CaptureSession, CaptureRoi,
    CpuBuffer, DetectionResult, HsvMaskStats, HsvSettings, HsvTuneResult, InferenceBackend,
    InferenceDiagnostics, InferenceSettings, PixelFormat, ProviderState, Size2D,
};
use capture_windows::WindowsCaptureBackend;
use config::{
    AppConfig, HsvPickerSettings, hsv_settings_path_for_config, load_hsv_picker_settings,
    save_hsv_picker_settings,
};
use crossbeam_channel::{Receiver, Sender, TryRecvError, TrySendError, bounded};
use eframe::egui::{self, Color32};
use inference_dml::DirectMlInferenceBackend;
use pipeline::{FramePipeline, FramePipelineSettings};
use telemetry::TelemetryHub;
use ui::{SmartCaptureUi, UiCommand, UiModel};
use vision_gpu::detect_hsv_with_preview;

/// UI event-loop cadence while capturing (~60 Hz paint). Capture thread runs faster.
const CAPTURE_REPAINT_INTERVAL: Duration = Duration::from_millis(16);
/// Throttle UI-thread HSV re-tune while dragging sliders.
const HSV_TUNE_INTERVAL: Duration = Duration::from_millis(100);

fn main() -> Result<()> {
    // Quiet by default (share-session tone); override with RUST_LOG.
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new("error")),
        )
        .init();

    let options = eframe::NativeOptions {
        renderer: eframe::Renderer::Wgpu,
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([400.0, 320.0])
            .with_min_inner_size([340.0, 260.0]),
        ..Default::default()
    };

    let app = DesktopApp::bootstrap()?;
    let window_title = app
        .ui
        .model
        .config
        .concealment
        .window_title()
        .to_owned();
    eframe::run_native(
        &window_title,
        options,
        Box::new(move |_cc| Ok(Box::new(app))),
    )
    .map_err(|err| anyhow::anyhow!(err.to_string()))
}

/// Exclude this process's top-level windows from desktop capture composition.
fn exclude_our_windows_from_capture() {
    use windows::Win32::Foundation::{HWND, LPARAM};
    use windows::Win32::System::Threading::GetCurrentProcessId;
    use windows::Win32::UI::WindowsAndMessaging::{
        EnumWindows, GetWindowThreadProcessId, SetWindowDisplayAffinity, WDA_EXCLUDEFROMCAPTURE,
    };
    use windows::core::BOOL;

    unsafe extern "system" fn enum_proc(hwnd: HWND, lparam: LPARAM) -> BOOL {
        let our_pid = lparam.0 as u32;
        let mut pid = 0u32;
        unsafe {
            GetWindowThreadProcessId(hwnd, Some(&mut pid));
            if pid == our_pid {
                let _ = SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
            }
        }
        BOOL(1)
    }

    let pid = unsafe { GetCurrentProcessId() };
    let _ = unsafe { EnumWindows(Some(enum_proc), LPARAM(pid as isize)) };
}

enum WorkerCommand {
    Process {
        analysis: AnalysisFrame,
        settings: FramePipelineSettings,
    },
    LoadModel(InferenceSettings),
    Shutdown,
}

enum WorkerEvent {
    Frame {
        hsv: HsvMaskStats,
        hsv_tune: Option<HsvTuneResult>,
        yolo_detections: Vec<DetectionResult>,
        capture_roi: CaptureRoi,
        capture_size: Size2D,
        model_input: Size2D,
        processing_ms: f64,
        provider_state: ProviderState,
        diagnostics: InferenceDiagnostics,
    },
    ModelReady {
        state: ProviderState,
        diagnostics: InferenceDiagnostics,
        summary: String,
    },
    Failed(String),
}

struct VisionWorker {
    cmd_tx: Sender<WorkerCommand>,
    event_rx: Receiver<WorkerEvent>,
    handle: Option<JoinHandle<()>>,
}

impl VisionWorker {
    fn spawn() -> Self {
        let (cmd_tx, cmd_rx) = bounded::<WorkerCommand>(2);
        let (event_tx, event_rx) = bounded::<WorkerEvent>(4);
        // Unnamed thread — avoid capture-branded names in process tools.
        let handle = thread::spawn(move || vision_worker_loop(cmd_rx, event_tx));
        Self {
            cmd_tx,
            event_rx,
            handle: Some(handle),
        }
    }

    fn try_submit(&self, analysis: AnalysisFrame, settings: FramePipelineSettings) -> bool {
        match self
            .cmd_tx
            .try_send(WorkerCommand::Process { analysis, settings })
        {
            Ok(()) => true,
            Err(TrySendError::Full(_)) => false,
            Err(TrySendError::Disconnected(_)) => false,
        }
    }

    fn load_model(&self, settings: InferenceSettings) {
        let _ = self.cmd_tx.send(WorkerCommand::LoadModel(settings));
    }

    fn poll_events(&self) -> Vec<WorkerEvent> {
        let mut events = Vec::new();
        loop {
            match self.event_rx.try_recv() {
                Ok(event) => events.push(event),
                Err(TryRecvError::Empty) => break,
                Err(TryRecvError::Disconnected) => break,
            }
        }
        events
    }
}

impl Drop for VisionWorker {
    fn drop(&mut self) {
        let _ = self.cmd_tx.send(WorkerCommand::Shutdown);
        if let Some(handle) = self.handle.take() {
            let _ = handle.join();
        }
    }
}

fn vision_worker_loop(cmd_rx: Receiver<WorkerCommand>, event_tx: Sender<WorkerEvent>) {
    let pipeline = FramePipeline::new();
    let mut inference = DirectMlInferenceBackend::default();

    while let Ok(command) = cmd_rx.recv() {
        match command {
            WorkerCommand::Shutdown => break,
            WorkerCommand::LoadModel(settings) => match inference.initialize(settings) {
                Ok(state) => {
                    let diagnostics = inference.diagnostics();
                    let summary = inference_summary(&diagnostics);
                    let _ = event_tx.send(WorkerEvent::ModelReady {
                        state,
                        diagnostics,
                        summary,
                    });
                }
                Err(err) => {
                    let _ = event_tx.send(WorkerEvent::Failed(err.to_string()));
                }
            },
            WorkerCommand::Process {
                analysis,
                mut settings,
            } => {
                // Preview is produced on the capture thread — never request GPU readback here.
                settings.want_preview_buffer = false;
                settings.preview_enabled = false;

                let started = std::time::Instant::now();
                match pipeline.process_analysis_roi(&analysis, &settings, Some(&mut inference)) {
                    Ok(report) => {
                        let processing_ms = started.elapsed().as_secs_f64() * 1000.0;
                        let _ = event_tx.send(WorkerEvent::Frame {
                            hsv: report.hsv,
                            hsv_tune: report.hsv_tune,
                            yolo_detections: report.yolo_detections,
                            capture_roi: report.capture_roi,
                            capture_size: report.capture_size,
                            model_input: report.model_input,
                            processing_ms,
                            provider_state: inference.provider_state(),
                            diagnostics: inference.diagnostics(),
                        });
                    }
                    Err(err) => {
                        let _ = event_tx.send(WorkerEvent::Failed(err.to_string()));
                    }
                }
            }
        }
    }
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
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct SessionFingerprint {
    selected_target: usize,
    target_id: String,
    target_fps: u32,
    buffer_depth: u32,
    backend_preference: CaptureBackendPreference,
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
            backend_preference: CaptureBackendPreference::DxgiDuplication,
            config_path: Some(config_path.clone()),
            config,
            ..UiModel::default()
        });

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
        })
    }

    fn current_fingerprint(&self) -> Option<SessionFingerprint> {
        let target = self.ui.model.targets.get(self.ui.model.selected_target)?;
        Some(SessionFingerprint {
            selected_target: self.ui.model.selected_target,
            target_id: target.id.clone(),
            target_fps: self.ui.model.config.performance.target_fps,
            buffer_depth: self.ui.model.config.performance.frame_buffer_size as u32,
            backend_preference: self.ui.model.backend_preference,
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
                    self.push_log_quiet("targets refreshed", "targets refreshed");
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
                self.push_log_quiet(
                    "vision enabled",
                    format!("loading model with EP order: [{providers}]"),
                );
                self.worker.load_model(settings);
            }
            UiCommand::SaveSettings => {
                let mut config_to_save = self.ui.model.config.clone();
                relativize_model_paths(&mut config_to_save, &self.repo_root);
                match config_to_save.save(&self.config_path) {
                    Ok(()) => {
                        self.ui.model.last_error = None;
                        self.push_log_quiet("settings saved", "settings saved");
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
                        self.push_log_quiet("settings loaded", "HSV settings loaded");
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
                        self.push_log_quiet("settings saved", "HSV settings saved");
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
        self.ui.model.backend_preference = CaptureBackendPreference::DxgiDuplication;
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
                self.session_fingerprint = self.current_fingerprint();
                self.last_preview_enabled = false;
                self.last_analysis_enabled = false;
                if self.ui.model.config.concealment.exclude_windows_active() {
                    exclude_our_windows_from_capture();
                }
                self.push_log_quiet(
                    "sharing started",
                    format!(
                        "sharing started on {} @ {} FPS",
                        target.name, self.ui.model.config.performance.target_fps
                    ),
                );
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
        self.ui.model.preview_frame_version =
            self.ui.model.preview_frame_version.wrapping_add(1);
        self.last_analysis = None;
        self.clear_hsv_tune_previews();
        self.push_log_quiet("sharing stopped", "sharing stopped");
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
        self.push_log_quiet("session restarting…", "session options changed — restarting…");
        self.stop_capture();
        self.start_capture();
    }

    fn sync_session_gates(&mut self) {
        let want_preview = self.ui.monitor_is_open() && self.ui.model.preview_enabled;
        let yolo_on = self.ui.model.config.vision_algorithms.yolo26_detection.enabled;
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
        self.push_log_quiet("vision enabled", "vision enabled — loading model…");
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

            // Monitor preview: CPU ColorImage from capture-thread channel.
            loop {
                match session.try_recv_preview() {
                    Ok(Some(preview)) => {
                        self.ui.model.capture_frame_size = Some(preview.capture_size);
                        self.ui.model.preview_scale = preview.scale;
                        self.ui.model.preview_frame =
                            Some(cpu_buffer_to_color_image(&preview.buffer));
                        self.ui.model.preview_frame_version =
                            self.ui.model.preview_frame_version.wrapping_add(1);
                    }
                    Ok(None) => break,
                    Err(_) => break,
                }
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
                let bindings = self.ui.model.config.runtime_bindings(
                    self.ui.model.preview_enabled,
                    self.ui.model.backend_preference,
                );
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
        for _ in 0..captured.saturating_sub(skipped) {
            self.telemetry.on_frame();
        }
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
        self.ui.model.hsv_tune_source = None;
        self.ui.model.hsv_tune_raw = None;
        self.ui.model.hsv_tune_morph = None;
        self.ui.model.hsv_tune_overlay = None;
        self.ui.model.hsv_tune_version = self.ui.model.hsv_tune_version.wrapping_add(1);
        self.ui.model.hsv_coverage_pct = 0.0;
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

        let tune = detect_hsv_with_preview(
            &analysis.roi_buffer,
            &hsv_settings,
            analysis.capture_size,
        );
        apply_hsv_tune_to_model(&mut self.ui.model, &tune, &analysis.roi_buffer);
        self.hsv_tune_dirty = false;
        self.last_hsv_tune_at = now;
    }

    fn drain_worker_events(&mut self) {
        for event in self.worker.poll_events() {
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
                    diagnostics,
                } => {
                    self.ui.model.hsv = hsv;
                    if let (Some(tune), Some(analysis)) = (hsv_tune, self.last_analysis.as_ref()) {
                        apply_hsv_tune_to_model(&mut self.ui.model, &tune, &analysis.roi_buffer);
                        self.hsv_tune_dirty = false;
                        self.last_hsv_tune_at = Instant::now();
                    }
                    self.ui.model.yolo_detections = yolo_detections;
                    self.ui.model.capture_roi = Some(capture_roi);
                    self.ui.model.capture_frame_size = Some(capture_size);
                    self.ui.model.model_input_size = Some(model_input);
                    self.ui.model.provider_state = provider_state;
                    self.ui.model.inference_diagnostics = diagnostics;
                    self.telemetry.set_processing_ms(processing_ms);
                }
                WorkerEvent::ModelReady {
                    state,
                    diagnostics,
                    summary,
                } => {
                    self.ui.model.provider_state = state;
                    self.ui.model.inference_diagnostics = diagnostics.clone();
                    self.ui.model.last_error = None;
                    self.push_log_quiet(
                        "vision ready",
                        format!(
                            "inference backend initialized: {}",
                            provider_label_short(&self.ui.model.provider_state)
                        ),
                    );
                    if !self.ui.model.config.concealment.quiet_logs_active() {
                        self.push_log(summary);
                        if let Some(reason) = diagnostics.fallback_reason {
                            self.push_log(format!("provider fallback reason: {reason}"));
                        }
                        for note in diagnostics.validation_notes {
                            self.push_log(format!("model validation: {note}"));
                        }
                    }
                }
                WorkerEvent::Failed(message) => {
                    if self.ui.model.config.vision_algorithms.yolo26_detection.enabled
                        && matches!(
                            self.ui.model.provider_state,
                            ProviderState::Uninitialized | ProviderState::Failed(_)
                        )
                    {
                        self.yolo_autoload_done = false;
                    }
                    self.ui.model.last_error = Some(message.clone());
                    self.push_log_quiet("session error", format!("frame processing failed: {message}"));
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

    /// When quiet logs are on, show `quiet`; otherwise show `verbose`.
    fn push_log_quiet(&mut self, quiet: impl Into<String>, verbose: impl Into<String>) {
        if self.ui.model.config.concealment.quiet_logs_active() {
            self.push_log(quiet);
        } else {
            self.push_log(verbose);
        }
    }
}

impl eframe::App for DesktopApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        self.pump_capture();
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
        if self.ui.model.config.concealment.exclude_windows_active()
            && monitor_open
            && !monitor_was_open
        {
            exclude_our_windows_from_capture();
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

fn cpu_buffer_to_color_image(buffer: &CpuBuffer) -> egui::ColorImage {
    let width = buffer.width as usize;
    let height = buffer.height as usize;
    let mut rgba = vec![0u8; width * height * 4];
    match buffer.pixel_format {
        PixelFormat::Bgra8Unorm => {
            for y in 0..height {
                let src_row = y * buffer.stride as usize;
                let dst_row = y * width * 4;
                for x in 0..width {
                    let src = src_row + x * 4;
                    let dst = dst_row + x * 4;
                    rgba[dst] = buffer.data[src + 2];
                    rgba[dst + 1] = buffer.data[src + 1];
                    rgba[dst + 2] = buffer.data[src];
                    rgba[dst + 3] = buffer.data[src + 3];
                }
            }
        }
        PixelFormat::Rgba8Unorm => {
            for y in 0..height {
                let src_row = y * buffer.stride as usize;
                let dst_row = y * width * 4;
                for x in 0..width {
                    let src = src_row + x * 4;
                    let dst = dst_row + x * 4;
                    rgba[dst..dst + 4].copy_from_slice(&buffer.data[src..src + 4]);
                }
            }
        }
    }
    egui::ColorImage::from_rgba_unmultiplied([width, height], &rgba)
}

fn apply_hsv_tune_to_model(model: &mut UiModel, tune: &HsvTuneResult, source: &CpuBuffer) {
    model.hsv = tune.stats.clone();
    model.hsv_coverage_pct = tune.coverage_pct();
    model.hsv_tune_source = Some(cpu_buffer_to_color_image(source));
    model.hsv_tune_raw = Some(mask_to_color_image(
        &tune.raw_mask,
        tune.preview_width,
        tune.preview_height,
    ));
    model.hsv_tune_morph = Some(mask_to_color_image(
        &tune.morph_mask,
        tune.preview_width,
        tune.preview_height,
    ));
    if let Some(source_img) = &model.hsv_tune_source {
        model.hsv_tune_overlay = Some(overlay_from_source_and_mask(
            source_img,
            &tune.morph_mask,
            tune.preview_width,
            tune.preview_height,
        ));
    }
    model.hsv_tune_version = model.hsv_tune_version.wrapping_add(1);
}

fn mask_to_color_image(mask: &[u8], width: u32, height: u32) -> egui::ColorImage {
    let w = width as usize;
    let h = height as usize;
    let mut rgba = vec![0u8; w * h * 4];
    for (idx, alpha) in mask.iter().enumerate().take(w * h) {
        let dst = idx * 4;
        let v = if *alpha > 0 { 255 } else { 0 };
        rgba[dst] = v;
        rgba[dst + 1] = v;
        rgba[dst + 2] = v;
        rgba[dst + 3] = 255;
    }
    egui::ColorImage::from_rgba_unmultiplied([w, h], &rgba)
}

fn overlay_from_source_and_mask(
    source: &egui::ColorImage,
    mask: &[u8],
    width: u32,
    height: u32,
) -> egui::ColorImage {
    let w = width as usize;
    let h = height as usize;
    let mut pixels = source.pixels.clone();
    let tint = Color32::from_rgba_premultiplied(217, 119, 6, 102);
    for y in 0..h {
        for x in 0..w {
            let mask_idx = y * w + x;
            if mask.get(mask_idx).copied().unwrap_or(0) == 0 {
                continue;
            }
            let px_idx = mask_idx;
            if px_idx >= pixels.len() {
                continue;
            }
            let base = pixels[px_idx];
            let t = 0.45;
            pixels[px_idx] = Color32::from_rgba_unmultiplied(
                lerp_u8(base.r(), tint.r(), t),
                lerp_u8(base.g(), tint.g(), t),
                lerp_u8(base.b(), tint.b(), t),
                base.a().max(tint.a()),
            );
        }
    }
    egui::ColorImage {
        size: [w, h],
        pixels,
        source_size: source.source_size,
    }
}

fn lerp_u8(a: u8, b: u8, t: f32) -> u8 {
    ((a as f32) * (1.0 - t) + (b as f32) * t).round().clamp(0.0, 255.0) as u8
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

fn inference_summary(diagnostics: &InferenceDiagnostics) -> String {
    let mut parts = vec![format!("classes={}", diagnostics.class_count)];
    if let Some(shape) = &diagnostics.last_output_shape {
        parts.push(format!("output={shape:?}"));
    }
    if let Some(graph) = &diagnostics.model_metadata.graph_name {
        parts.push(format!("graph={graph}"));
    }
    parts.join(", ")
}
