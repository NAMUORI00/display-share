use std::path::PathBuf;

use capture_core::{
    CaptureBackendKind, CaptureBackendPreference, CaptureRoi, CaptureTarget, DetectionResult,
    HsvMaskStats, InferenceDiagnostics, PerformanceSnapshot, ProviderState, RectI, Size2D,
    backend_kind_label,
};
use config::AppConfig;
use egui::{
    self, Align, Color32, ColorImage, CornerRadius, FontId, Frame, Margin, Pos2, Rect, RichText,
    Sense, Stroke, TextureHandle, TextureOptions, Vec2, epaint::RectShape,
};

const ACCENT: Color32 = Color32::from_rgb(14, 116, 144); // teal
const ACCENT_SOFT: Color32 = Color32::from_rgba_premultiplied(2, 18, 23, 40);
const HSV_STROKE: Color32 = Color32::from_rgb(217, 119, 6); // amber
const YOLO_STROKE: Color32 = Color32::from_rgb(14, 116, 144);
const ROI_STROKE: Color32 = Color32::from_rgba_premultiplied(160, 160, 160, 160);
const PANEL_BG: Color32 = Color32::from_rgb(248, 250, 252);
const SURFACE: Color32 = Color32::from_rgb(255, 255, 255);
const BORDER: Color32 = Color32::from_rgb(226, 232, 240);
const TEXT_MUTED: Color32 = Color32::from_rgb(100, 116, 139);
const LEFT_RAIL_W: f32 = 220.0;
const RIGHT_RAIL_W: f32 = 240.0;
const LOG_DRAWER_H: f32 = 180.0;

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
    /// Draft settings edited in Control rail (app persists on SaveSettings).
    pub config: AppConfig,
    /// Full-frame capture preview (downscaled).
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
            backend_preference: CaptureBackendPreference::DxgiDuplication,
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

pub struct SmartCaptureUi {
    pub model: UiModel,
    preview_texture: Option<TextureHandle>,
    last_preview_version: u64,
    logs_open: bool,
    /// Separate OS monitor window (egui immediate viewport).
    monitor_open: bool,
    theme_applied: bool,
}

impl SmartCaptureUi {
    #[must_use]
    pub fn new(model: UiModel) -> Self {
        Self {
            model,
            preview_texture: None,
            last_preview_version: 0,
            logs_open: false,
            monitor_open: false,
            theme_applied: false,
        }
    }

    #[must_use]
    pub fn monitor_is_open(&self) -> bool {
        self.monitor_open
    }

    pub fn show(&mut self, ctx: &egui::Context) -> Vec<UiCommand> {
        // Native secondary windows (not embedded in the main viewport).
        ctx.set_embed_viewports(false);
        if !self.theme_applied {
            apply_theme(ctx, self.model.config.gui.ui_scale);
            self.theme_applied = true;
        } else {
            // Only re-apply pixels_per_point when the user changes ui_scale.
            let scale = self.model.config.gui.ui_scale.clamp(0.75, 2.0);
            if (ctx.pixels_per_point() - scale).abs() > 0.01 {
                ctx.set_pixels_per_point(scale);
            }
        }

        if self.monitor_open {
            self.sync_preview_texture(ctx);
        }

        let mut commands = Vec::new();

        egui::TopBottomPanel::top("operator_top")
            .frame(panel_frame())
            .show(ctx, |ui| {
                self.draw_top_bar(ui, &mut commands);
            });

        egui::TopBottomPanel::bottom("operator_logs")
            .resizable(true)
            .default_height(if self.logs_open { LOG_DRAWER_H } else { 28.0 })
            .min_height(28.0)
            .frame(panel_frame())
            .show(ctx, |ui| {
                self.draw_log_drawer(ui);
            });

        egui::SidePanel::left("operator_left")
            .exact_width(LEFT_RAIL_W)
            .resizable(false)
            .frame(panel_frame())
            .show(ctx, |ui| {
                self.draw_left_rail(ui, &mut commands);
            });

        egui::SidePanel::right("operator_right")
            .exact_width(RIGHT_RAIL_W)
            .resizable(false)
            .frame(panel_frame())
            .show(ctx, |ui| {
                self.draw_right_rail(ui);
            });

        egui::CentralPanel::default()
            .frame(
                Frame::new()
                    .fill(Color32::from_rgb(241, 245, 249))
                    .inner_margin(Margin::same(8)),
            )
            .show(ctx, |ui| {
                self.draw_ops_summary(ui);
            });

        if self.monitor_open {
            self.show_monitor_viewport(ctx);
        }

        commands
    }

    fn show_monitor_viewport(&mut self, ctx: &egui::Context) {
        let mut close_requested = false;

        ctx.show_viewport_immediate(
            egui::ViewportId::from_hash_of("capture_monitor"),
            egui::ViewportBuilder::default()
                .with_title("SmartScreenCapture — Monitor")
                .with_inner_size([960.0, 540.0])
                .with_min_inner_size([480.0, 270.0]),
            |ctx, class| {
                if ctx.input(|i| i.viewport().close_requested()) {
                    close_requested = true;
                }

                if class == egui::ViewportClass::Embedded {
                    egui::Window::new("Monitor")
                        .default_size([640.0, 360.0])
                        .show(ctx, |ui| {
                            self.draw_preview_stage(ui);
                        });
                } else {
                    egui::CentralPanel::default()
                        .frame(
                            Frame::new()
                                .fill(Color32::from_rgb(241, 245, 249))
                                .inner_margin(Margin::same(12)),
                        )
                        .show(ctx, |ui| {
                            self.draw_preview_stage(ui);
                        });
                }
            },
        );

        if close_requested {
            self.monitor_open = false;
        }
    }

    /// Central panel: minimal hint + errors only (metrics live in right rail).
    fn draw_ops_summary(&mut self, ui: &mut egui::Ui) {
        if !self.model.capture_running {
            ui.label(
                RichText::new("Start capture · open Monitor for live view")
                    .small()
                    .color(TEXT_MUTED),
            );
        } else if let Some(size) = self.model.capture_frame_size {
            ui.label(
                RichText::new(format!("Capture {}×{}", size.width, size.height))
                    .small()
                    .color(TEXT_MUTED),
            );
        }

        if let Some(error) = &self.model.last_error {
            ui.add_space(6.0);
            Frame::new()
                .fill(Color32::from_rgb(254, 226, 226))
                .corner_radius(CornerRadius::same(6))
                .inner_margin(Margin::symmetric(8, 4))
                .show(ui, |ui| {
                    ui.colored_label(Color32::from_rgb(153, 27, 27), error);
                });
        }
    }

    fn sync_preview_texture(&mut self, ctx: &egui::Context) {
        if self.model.preview_frame_version == self.last_preview_version {
            return;
        }
        self.last_preview_version = self.model.preview_frame_version;
        match &self.model.preview_frame {
            Some(image) => {
                if let Some(texture) = &mut self.preview_texture {
                    texture.set(image.clone(), TextureOptions::LINEAR);
                } else {
                    self.preview_texture = Some(ctx.load_texture(
                        "capture-preview",
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

    fn draw_top_bar(&mut self, ui: &mut egui::Ui, commands: &mut Vec<UiCommand>) {
        ui.horizontal(|ui| {
            ui.label(
                RichText::new("SmartScreenCapture")
                    .strong()
                    .size(14.0)
                    .color(Color32::from_rgb(15, 23, 42)),
            );
            ui.add_space(6.0);
            status_chip(ui, self.model.capture_running, self.model.last_error.is_some());
            badge(ui, "DXGI", "dup");
            badge(ui, "EP", &provider_label(&self.model.provider_state));
            if self.model.capture_running {
                if let Some(backend) = self.model.active_backend {
                    badge(ui, "Cap", backend_kind_label(backend));
                }
            }
            if self.monitor_open {
                badge(ui, "Mon", "on");
            }

            ui.with_layout(egui::Layout::right_to_left(Align::Center), |ui| {
                let start_stop = if self.model.capture_running {
                    ("Stop", true)
                } else {
                    ("Start", false)
                };
                let start_btn = egui::Button::new(
                    RichText::new(start_stop.0)
                        .strong()
                        .color(Color32::WHITE),
                )
                .fill(if start_stop.1 {
                    Color32::from_rgb(185, 28, 28)
                } else {
                    ACCENT
                })
                .corner_radius(CornerRadius::same(6));
                if ui.add(start_btn).clicked() {
                    commands.push(if self.model.capture_running {
                        UiCommand::StopCapture
                    } else {
                        UiCommand::StartCapture
                    });
                }
                if ui.small_button("Save").clicked() {
                    commands.push(UiCommand::SaveSettings);
                }
                if ui.small_button("Model").clicked() {
                    commands.push(UiCommand::LoadModel);
                }
                if ui.small_button("Refresh").clicked() {
                    commands.push(UiCommand::RefreshTargets);
                }
                let monitor_label = if self.monitor_open { "Monitor ✕" } else { "Monitor" };
                if ui.small_button(monitor_label).clicked() {
                    self.monitor_open = !self.monitor_open;
                }
            });
        });
    }

    fn draw_left_rail(&mut self, ui: &mut egui::Ui, commands: &mut Vec<UiCommand>) {
        egui::ScrollArea::vertical().show(ui, |ui| {
            section_title(ui, "Capture target");
            if self.model.targets.is_empty() {
                ui.label(RichText::new("No targets yet — Refresh").color(TEXT_MUTED));
            } else {
                for (index, target) in self.model.targets.iter().enumerate() {
                    let selected = self.model.selected_target == index;
                    let label = format!(
                        "{}{} ({}×{})",
                        target.name,
                        if target.primary { " ★" } else { "" },
                        target.size.width,
                        target.size.height
                    );
                    if ui.selectable_label(selected, label).clicked() {
                        self.model.selected_target = index;
                    }
                }
            }

            if let Some(target) = self.model.targets.get(self.model.selected_target) {
                ui.add_space(4.0);
                ui.label(
                    RichText::new(format!(
                        "{} · {}",
                        target.device_name.as_deref().unwrap_or("device?"),
                        target.adapter_name.as_deref().unwrap_or("adapter?")
                    ))
                    .small()
                    .color(TEXT_MUTED),
                );
            }

            ui.add_space(8.0);
            section_title(ui, "Runtime");

            // Keep preference pinned — no WGC/DXGI switcher (single-path plan).
            self.model.backend_preference = CaptureBackendPreference::DxgiDuplication;

            ui.horizontal(|ui| {
                ui.checkbox(&mut self.model.preview_enabled, "Feed");
                ui.checkbox(
                    &mut self.model.config.vision_algorithms.hsv_tracking.enabled,
                    "HSV",
                );
                ui.checkbox(
                    &mut self.model.config.vision_algorithms.yolo26_detection.enabled,
                    "YOLO",
                );
            });

            ui.add_space(4.0);
            let hsv_enabled = self.model.config.vision_algorithms.hsv_tracking.enabled;
            ui.add_enabled_ui(hsv_enabled, |ui| {
                egui::CollapsingHeader::new("HSV Tools")
                    .default_open(false)
                    .show(ui, |ui| {
                        self.draw_hsv_tools(ui, commands);
                    });
            });

            ui.add_space(4.0);
            ui.columns(3, |columns| {
                columns[0].horizontal(|ui| {
                    ui.label("FPS");
                    ui.add(
                        egui::DragValue::new(&mut self.model.config.performance.target_fps)
                            .range(1..=360),
                    );
                });
                columns[1].horizontal(|ui| {
                    ui.label("Buf");
                    ui.add(
                        egui::DragValue::new(&mut self.model.config.performance.frame_buffer_size)
                            .range(1..=32),
                    );
                });
                columns[2].horizontal(|ui| {
                    ui.label("DML");
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
            });

            ui.add_space(8.0);
            egui::CollapsingHeader::new("Advanced")
                .default_open(false)
                .show(ui, |ui| {
                    let yolo = &self.model.config.vision_algorithms.yolo26_detection;
                    ui.label(
                        RichText::new(format!("Model: {}", yolo.onnx_model_path.display()))
                            .small(),
                    );
                    ui.label(
                        RichText::new(format!("Classes: {}", yolo.class_names_path.display()))
                            .small(),
                    );
                    ui.label(
                        RichText::new(format!("EP: {}", yolo.execution_providers.join(", ")))
                            .small(),
                    );
                    match &yolo.openvino_device_type {
                        Some(d) => ui.label(RichText::new(format!("OpenVINO: {d}")).small()),
                        None => ui.label(RichText::new("OpenVINO: GPU→NPU auto").small()),
                    };
                    ui.horizontal(|ui| {
                        ui.label("UI scale");
                        ui.add(
                            egui::DragValue::new(&mut self.model.config.gui.ui_scale)
                                .range(0.75..=2.0)
                                .speed(0.05),
                        );
                    });
                    if let Some(path) = &self.model.config_path {
                        ui.label(
                            RichText::new(format!("Config: {}", path.display()))
                                .small()
                                .color(TEXT_MUTED),
                        );
                    }
                });
        });
    }

    fn draw_hsv_tools(&mut self, ui: &mut egui::Ui, commands: &mut Vec<UiCommand>) {
        let hsv = &mut self.model.config.vision_algorithms.hsv_tracking;
        let mut h_min = hsv.lower_bound[0];
        let mut h_max = hsv.upper_bound[0];
        let mut s_min = hsv.lower_bound[1];
        let mut s_max = hsv.upper_bound[1];
        let mut v_min = hsv.lower_bound[2];
        let mut v_max = hsv.upper_bound[2];

        let mut changed = false;
        changed |= hsv_slider_row(ui, "H", &mut h_min, &mut h_max, 179);
        changed |= hsv_slider_row(ui, "S", &mut s_min, &mut s_max, 255);
        changed |= hsv_slider_row(ui, "V", &mut v_min, &mut v_max, 255);

        if changed {
            if h_min > h_max {
                h_min = h_max;
            }
            if s_min > s_max {
                s_min = s_max;
            }
            if v_min > v_max {
                v_min = v_max;
            }
            hsv.lower_bound = [h_min, s_min, v_min];
            hsv.upper_bound = [h_max, s_max, v_max];
        }

        ui.label(
            RichText::new(format!(
                "H[{h_min}-{h_max}] S[{s_min}-{s_max}] V[{v_min}-{v_max}]"
            ))
            .small()
            .color(TEXT_MUTED),
        );

        ui.horizontal(|ui| {
            if ui.small_button("Load").clicked() {
                commands.push(UiCommand::LoadHsvSettings);
            }
            if ui.small_button("Save").clicked() {
                commands.push(UiCommand::SaveHsvSettings);
            }
        });

        ui.horizontal(|ui| {
            ui.label("Min area");
            ui.add(
                egui::DragValue::new(&mut hsv.min_contour_area)
                    .range(1..=10_000),
            );
        });
        ui.horizontal(|ui| {
            ui.label("Morph kernel");
            ui.add(
                egui::DragValue::new(&mut hsv.morphology_kernel_size)
                    .range(1..=15),
            );
        });
    }

    fn draw_preview_stage(&mut self, ui: &mut egui::Ui) {
        let hsv_hits = self.model.hsv_hit_count();
        let yolo_n = self.model.yolo_detection_count();
        let yolo_on = self.model.config.vision_algorithms.yolo26_detection.enabled;
        ui.horizontal(|ui| {
            ui.label(
                RichText::new(format!("HSV {hsv_hits}  ·  YOLO {yolo_n}"))
                    .small()
                    .color(TEXT_MUTED),
            );
            if yolo_on
                && matches!(
                    self.model.provider_state,
                    ProviderState::Uninitialized | ProviderState::Failed(_)
                )
            {
                ui.label(
                    RichText::new("Loading model…")
                        .small()
                        .color(Color32::from_rgb(217, 119, 6)),
                );
            }
        });
        ui.add_space(4.0);

        let available = ui.available_size();
        let (response, painter) = ui.allocate_painter(available, Sense::hover());
        let stage = response.rect;

        painter.rect_filled(stage, CornerRadius::same(8), SURFACE);
        painter.rect_stroke(stage, CornerRadius::same(8), Stroke::new(1.0, BORDER), egui::StrokeKind::Outside);

        let Some(texture) = self.preview_texture.as_ref() else {
            painter.text(
                stage.center(),
                egui::Align2::CENTER_CENTER,
                if !self.model.preview_enabled {
                    "Monitor feed disabled"
                } else if self.model.capture_running {
                    "Waiting for first frame…"
                } else {
                    "Start capture"
                },
                FontId::proportional(16.0),
                TEXT_MUTED,
            );
            return;
        };

        let tex_size = texture.size_vec2();
        let fitted = fit_inside(tex_size, stage.size() - Vec2::splat(16.0));
        let image_rect = Rect::from_center_size(stage.center(), fitted);

        painter.image(
            texture.id(),
            image_rect,
            Rect::from_min_max(Pos2::ZERO, Pos2::new(1.0, 1.0)),
            Color32::WHITE,
        );

        paint_overlays(
            &painter,
            image_rect,
            tex_size,
            &self.model,
        );
    }

    fn draw_right_rail(&mut self, ui: &mut egui::Ui) {
        egui::ScrollArea::vertical().show(ui, |ui| {
            section_title(ui, "Live metrics");
            let hsv_n = self.model.hsv_hit_count();
            let yolo_n = self.model.yolo_detection_count();
            compact_metrics_grid(ui, [
                ("FPS", format!("{:.1}", self.model.performance.fps)),
                (
                    "Frame",
                    format!("{:.1}ms", self.model.performance.frame_time_ms),
                ),
                (
                    "Proc",
                    format!("{:.0}ms", self.model.performance.processing_time_ms),
                ),
                (
                    "Drop",
                    self.model.performance.dropped_frames.to_string(),
                ),
                (
                    "Total",
                    self.model.performance.total_frames.to_string(),
                ),
                ("Det", format!("HSV {hsv_n} / YOLO {yolo_n}")),
            ]);

            ui.add_space(6.0);
            section_title(ui, "Detections");
            if self.model.yolo_detections.is_empty() {
                ui.label(RichText::new("No detections").color(TEXT_MUTED));
            } else {
                let mut ranked = self.model.yolo_detections.clone();
                ranked.sort_by(|a, b| b.confidence_milli.cmp(&a.confidence_milli));
                for det in ranked.iter().take(12) {
                    let conf = det.confidence_milli as f32 / 10.0;
                    ui.horizontal(|ui| {
                        ui.label(RichText::new(&det.label).strong());
                        ui.label(RichText::new(format!("{conf:.0}%")).color(ACCENT));
                    });
                    egui::CollapsingHeader::new("bbox")
                        .default_open(false)
                        .show(ui, |ui| {
                            ui.label(
                                RichText::new(format!(
                                    "{}×{} @ ({}, {})",
                                    det.bounding_box.width,
                                    det.bounding_box.height,
                                    det.bounding_box.x,
                                    det.bounding_box.y
                                ))
                                .small()
                                .color(TEXT_MUTED),
                            );
                        });
                }
            }

            ui.add_space(6.0);
            section_title(ui, "Inference");
            ui.label(format!(
                "Classes: {}",
                self.model.inference_diagnostics.class_count
            ));
            if let Some(shape) = &self.model.inference_diagnostics.last_output_shape {
                ui.label(format!("Output: {}", format_shape(shape)));
            }
            if let Some(reason) = &self.model.inference_diagnostics.fallback_reason {
                ui.colored_label(ui.visuals().warn_fg_color, reason);
            }

            egui::CollapsingHeader::new("Diagnostics")
                .default_open(false)
                .show(ui, |ui| {
                    if let Some(graph) = &self.model.inference_diagnostics.model_metadata.graph_name
                    {
                        ui.label(format!("Graph: {graph}"));
                    }
                    if let Some(producer) =
                        &self.model.inference_diagnostics.model_metadata.producer
                    {
                        ui.label(format!("Producer: {producer}"));
                    }
                    render_descriptors(ui, "Inputs", &self.model.inference_diagnostics.inputs);
                    render_descriptors(ui, "Outputs", &self.model.inference_diagnostics.outputs);
                    for note in &self.model.inference_diagnostics.validation_notes {
                        ui.colored_label(ui.visuals().warn_fg_color, note);
                    }
                    if let Some(err) = &self.model.inference_diagnostics.last_error {
                        ui.colored_label(ui.visuals().error_fg_color, err);
                    }
                });
        });
    }

    fn draw_log_drawer(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            let label = if self.logs_open {
                "▾ Logs"
            } else {
                "▸ Logs"
            };
            if ui
                .add(egui::Button::new(RichText::new(label).strong()).frame(false))
                .clicked()
            {
                self.logs_open = !self.logs_open;
            }
            ui.label(
                RichText::new(format!("{} lines", self.model.logs.len()))
                    .small()
                    .color(TEXT_MUTED),
            );
        });
        if self.logs_open {
            egui::ScrollArea::vertical()
                .stick_to_bottom(true)
                .show(ui, |ui| {
                    for line in &self.model.logs {
                        ui.label(RichText::new(line).monospace().size(12.0));
                    }
                });
        }
    }
}

fn paint_overlays(
    painter: &egui::Painter,
    image_rect: Rect,
    tex_size: Vec2,
    model: &UiModel,
) {
    let Some(capture_size) = model.capture_frame_size else {
        return;
    };
    if capture_size.width == 0 || capture_size.height == 0 || tex_size.x <= 0.0 {
        return;
    }

    // Preview buffer pixels → screen: image_rect already matches tex_size aspect.
    let sx = image_rect.width() / tex_size.x;
    let sy = image_rect.height() / tex_size.y;
    let scale = model.preview_scale.max(1e-6);

    let to_screen = |frame_x: f32, frame_y: f32| -> Pos2 {
        let px = frame_x * scale;
        let py = frame_y * scale;
        Pos2::new(image_rect.min.x + px * sx, image_rect.min.y + py * sy)
    };

    let frame_rect_to_screen = |r: RectI| -> Rect {
        let min = to_screen(r.x as f32, r.y as f32);
        let max = to_screen(
            (r.x + r.width as i32) as f32,
            (r.y + r.height as i32) as f32,
        );
        Rect::from_min_max(min, max)
    };

    if let Some(roi) = model.capture_roi {
        let roi_screen = frame_rect_to_screen(roi.rect);
        painter.add(RectShape::new(
            roi_screen,
            CornerRadius::ZERO,
            ACCENT_SOFT,
            Stroke::new(1.5, ROI_STROKE),
            egui::StrokeKind::Outside,
        ));
    }

    // HSV contour boxes (frame coordinates).
    if !model.hsv.objects.is_empty() {
        for (index, obj) in model.hsv.objects.iter().enumerate() {
            let screen = frame_rect_to_screen(obj.rect);
            painter.rect_stroke(
                screen,
                CornerRadius::ZERO,
                Stroke::new(2.0, HSV_STROKE),
                egui::StrokeKind::Outside,
            );
            painter.text(
                Pos2::new(screen.min.x, (screen.min.y - 2.0).max(image_rect.min.y)),
                egui::Align2::LEFT_BOTTOM,
                format!("HSV {index}"),
                FontId::proportional(11.0),
                HSV_STROKE,
            );
        }
    } else if let Some(bbox) = model.hsv.bbox_union {
        let screen = frame_rect_to_screen(bbox);
        painter.rect_stroke(
            screen,
            CornerRadius::ZERO,
            Stroke::new(2.0, HSV_STROKE),
            egui::StrokeKind::Outside,
        );
        painter.text(
            Pos2::new(screen.min.x, screen.min.y - 14.0),
            egui::Align2::LEFT_BOTTOM,
            format!("HSV {}", model.hsv.hit_count),
            FontId::proportional(12.0),
            HSV_STROKE,
        );
    }

    let model_w = model
        .model_input_size
        .map(|s| s.width.max(1) as f32)
        .unwrap_or(640.0);
    let model_h = model
        .model_input_size
        .map(|s| s.height.max(1) as f32)
        .unwrap_or(640.0);
    let roi_w = model
        .capture_roi
        .map(|r| r.rect.width.max(1) as f32)
        .unwrap_or(320.0);
    let roi_h = model
        .capture_roi
        .map(|r| r.rect.height.max(1) as f32)
        .unwrap_or(320.0);
    let roi_origin = model
        .capture_roi
        .map(|r| (r.rect.x as f32, r.rect.y as f32))
        .unwrap_or((0.0, 0.0));

    for det in &model.yolo_detections {
        // model space → ROI local → full frame
        let lx = det.bounding_box.x as f32 * roi_w / model_w;
        let ly = det.bounding_box.y as f32 * roi_h / model_h;
        let lw = det.bounding_box.width as f32 * roi_w / model_w;
        let lh = det.bounding_box.height as f32 * roi_h / model_h;
        let frame_box = RectI::new(
            (roi_origin.0 + lx).round() as i32,
            (roi_origin.1 + ly).round() as i32,
            lw.round().max(1.0) as u32,
            lh.round().max(1.0) as u32,
        );
        let screen = frame_rect_to_screen(frame_box);
        painter.rect_stroke(
            screen,
            CornerRadius::ZERO,
            Stroke::new(2.0, YOLO_STROKE),
            egui::StrokeKind::Outside,
        );
        let conf = det.confidence_milli as f32 / 10.0;
        painter.text(
            Pos2::new(screen.min.x, (screen.min.y - 2.0).max(image_rect.min.y)),
            egui::Align2::LEFT_BOTTOM,
            format!("{} {:.0}%", det.label, conf),
            FontId::proportional(12.0),
            YOLO_STROKE,
        );
    }
}

fn apply_theme(ctx: &egui::Context, ui_scale: f32) {
    let mut visuals = egui::Visuals::light();
    visuals.panel_fill = PANEL_BG;
    visuals.window_fill = SURFACE;
    visuals.override_text_color = Some(Color32::from_rgb(30, 41, 59));
    visuals.selection.bg_fill = ACCENT_SOFT;
    visuals.widgets.inactive.bg_fill = SURFACE;
    visuals.widgets.hovered.bg_fill = Color32::from_rgb(241, 245, 249);
    visuals.widgets.active.bg_fill = Color32::from_rgb(226, 232, 240);
    visuals.widgets.open.bg_fill = SURFACE;
    visuals.hyperlink_color = ACCENT;
    ctx.set_visuals(visuals);

    let scale = ui_scale.clamp(0.75, 2.0);
    // Only update when the scale actually changes — calling every frame can
    // destabilize the Win32/wgpu surface and surface as 0xc000041d.
    if (ctx.pixels_per_point() - scale).abs() > 0.01 {
        ctx.set_pixels_per_point(scale);
    }
}

fn panel_frame() -> Frame {
    Frame::new()
        .fill(PANEL_BG)
        .inner_margin(Margin::symmetric(8, 6))
        .stroke(Stroke::new(1.0, BORDER))
}

fn hsv_slider_row(
    ui: &mut egui::Ui,
    label: &str,
    min: &mut u8,
    max: &mut u8,
    max_val: u8,
) -> bool {
    let mut changed = false;
    ui.horizontal(|ui| {
        ui.label(RichText::new(label).small());
        changed |= ui
            .add(egui::Slider::new(min, 0..=max_val).show_value(true))
            .changed();
        changed |= ui
            .add(egui::Slider::new(max, 0..=max_val).show_value(true))
            .changed();
    });
    changed
}

fn section_title(ui: &mut egui::Ui, title: &str) {
    ui.label(
        RichText::new(title)
            .strong()
            .size(12.0)
            .color(Color32::from_rgb(15, 23, 42)),
    );
    ui.add_space(2.0);
}

fn compact_metric(ui: &mut egui::Ui, label: &str, value: &str) {
    Frame::new()
        .fill(SURFACE)
        .stroke(Stroke::new(1.0, BORDER))
        .corner_radius(CornerRadius::same(6))
        .inner_margin(Margin::symmetric(8, 4))
        .show(ui, |ui| {
            ui.vertical(|ui| {
                ui.label(RichText::new(label).small().color(TEXT_MUTED));
                ui.label(RichText::new(value).strong().size(14.0));
            });
        });
}

fn compact_metrics_grid(ui: &mut egui::Ui, metrics: [(&str, String); 6]) {
    ui.columns(2, |columns| {
        for (index, (label, value)) in metrics.into_iter().enumerate() {
            columns[index % 2].vertical(|ui| {
                compact_metric(ui, label, &value);
            });
        }
    });
}

fn status_chip(ui: &mut egui::Ui, capturing: bool, errored: bool) {
    let (text, fill, fg) = if errored {
        (
            "Error",
            Color32::from_rgb(254, 226, 226),
            Color32::from_rgb(153, 27, 27),
        )
    } else if capturing {
        (
            "Capturing",
            Color32::from_rgb(204, 251, 241),
            Color32::from_rgb(15, 118, 110),
        )
    } else {
        (
            "Idle",
            Color32::from_rgb(241, 245, 249),
            Color32::from_rgb(71, 85, 105),
        )
    };
    Frame::new()
        .fill(fill)
        .corner_radius(CornerRadius::same(10))
        .inner_margin(Margin::symmetric(8, 3))
        .show(ui, |ui| {
            ui.label(RichText::new(text).strong().color(fg).size(12.0));
        });
}

fn badge(ui: &mut egui::Ui, key: &str, value: &str) {
    Frame::new()
        .fill(SURFACE)
        .stroke(Stroke::new(1.0, BORDER))
        .corner_radius(CornerRadius::same(6))
        .inner_margin(Margin::symmetric(6, 2))
        .show(ui, |ui| {
            ui.label(
                RichText::new(format!("{key}: {value}"))
                    .small()
                    .color(TEXT_MUTED),
            );
        });
}

fn provider_label(state: &ProviderState) -> String {
    match state {
        ProviderState::Uninitialized => "uninitialized".to_owned(),
        ProviderState::DirectMl {
            device_id,
            cpu_fallback,
        } => format!(
            "DirectML#{device_id}{}",
            if *cpu_fallback { " (fb)" } else { "" }
        ),
        ProviderState::OpenVino { device_type } => format!("OpenVINO {device_type}"),
        ProviderState::CpuFallback => "CPU".to_owned(),
        ProviderState::Failed(message) => format!("failed: {message}"),
    }
}

fn fit_inside(source: Vec2, available: Vec2) -> Vec2 {
    if source.x <= 0.0 || source.y <= 0.0 {
        return Vec2::ZERO;
    }
    let scale = (available.x / source.x)
        .min(available.y / source.y)
        .min(1.0)
        .max(0.0);
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
        ui.label(
            RichText::new(format!(
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
            ))
            .small(),
        );
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
