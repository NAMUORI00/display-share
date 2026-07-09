use std::path::PathBuf;

use capture_core::{
    CaptureBackendKind, CaptureBackendPreference, CaptureRoi, CaptureTarget, DetectionResult,
    HsvMaskStats, InferenceDiagnostics, PerformanceSnapshot, ProviderState, RectI, Size2D,
    backend_kind_label,
};
use config::AppConfig;
use egui::{
    self, Align, Color32, ColorImage, CornerRadius, FontId, Frame, Margin, Pos2, Rect, RichText,
    Sense, Stroke, TextureHandle, TextureOptions, Vec2,
};

// ── Ghibli 1:3:6 palette (Adobe-style roles: Accent / Secondary / Neutrals) ──
// Neutrals (~60%)
const PAPER: Color32 = Color32::from_rgb(0xF4, 0xF0, 0xE6); // canvas
const SURFACE: Color32 = Color32::from_rgb(0xFB, 0xF8, 0xF1); // cards / panels
const SURFACE_RAISED: Color32 = Color32::from_rgb(0xFF, 0xFC, 0xF6);
const BORDER: Color32 = Color32::from_rgb(0xD4, 0xCB, 0xB8);
const INK: Color32 = Color32::from_rgb(0x3D, 0x34, 0x29);
const INK_MUTED: Color32 = Color32::from_rgb(0x7A, 0x6F, 0x5F);
// Secondary (~30%)
const MOSS: Color32 = Color32::from_rgb(0x5B, 0x8C, 0x6A);
const SKY: Color32 = Color32::from_rgb(0x7B, 0xA3, 0xB0);
const OCHRE: Color32 = Color32::from_rgb(0xE8, 0xB8, 0x6D);
// Accent (~10%)
const TERRACOTTA: Color32 = Color32::from_rgb(0xC4, 0x5C, 0x26);
const BRICK: Color32 = Color32::from_rgb(0xA6, 0x3D, 0x3D);
const CREAM_ON_ACCENT: Color32 = Color32::from_rgb(0xFF, 0xF8, 0xF0);

// Semantic aliases
const ACCENT: Color32 = TERRACOTTA;
const ACCENT_SOFT: Color32 = Color32::from_rgba_premultiplied(0x5B, 0x8C, 0x6A, 48);
const HSV_STROKE: Color32 = OCHRE;
const YOLO_STROKE: Color32 = SKY;
const ROI_STROKE: Color32 = Color32::from_rgba_premultiplied(0xD4, 0xCB, 0xB8, 200);
const TEXT_MUTED: Color32 = INK_MUTED;
const TEXT_PRIMARY: Color32 = INK;
const LOG_DRAWER_H: f32 = 120.0;
const LOG_DRAWER_COLLAPSED: f32 = 26.0;

// ── Color Picker tool surface (neutral dark — not shell brand palette) ──
const PICKER_BG: Color32 = Color32::from_rgb(0x1E, 0x1E, 0x22);
const PICKER_PANEL: Color32 = Color32::from_rgb(0x2A, 0x2A, 0x30);
const PICKER_RAISED: Color32 = Color32::from_rgb(0x34, 0x34, 0x3C);
const PICKER_BORDER: Color32 = Color32::from_rgb(0x55, 0x55, 0x60);
const PICKER_INK: Color32 = Color32::from_rgb(0xEC, 0xEC, 0xF0);
const PICKER_MUTED: Color32 = Color32::from_rgb(0xA0, 0xA0, 0xAA);
const PICKER_ACCENT: Color32 = Color32::from_rgb(0x4C, 0xA0, 0xFF);

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
    /// HSV tuning preview images (analysis-buffer resolution).
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

pub struct SmartCaptureUi {
    pub model: UiModel,
    preview_texture: Option<TextureHandle>,
    last_preview_version: u64,
    hsv_tune_source_tex: Option<TextureHandle>,
    hsv_tune_raw_tex: Option<TextureHandle>,
    hsv_tune_morph_tex: Option<TextureHandle>,
    hsv_tune_overlay_tex: Option<TextureHandle>,
    last_hsv_tune_version: u64,
    logs_open: bool,
    /// Separate OS monitor window (egui immediate viewport).
    monitor_open: bool,
    /// Separate OS HSV studio window (tools + triptych).
    hsv_window_open: bool,
    theme_applied: bool,
    /// Previous frame HSV enable — used to clear stale View overlays on disable.
    last_hsv_enabled: bool,
}

impl SmartCaptureUi {
    #[must_use]
    pub fn new(model: UiModel) -> Self {
        let last_hsv_enabled = model.config.vision_algorithms.hsv_tracking.enabled;
        Self {
            model,
            preview_texture: None,
            last_preview_version: 0,
            hsv_tune_source_tex: None,
            hsv_tune_raw_tex: None,
            hsv_tune_morph_tex: None,
            hsv_tune_overlay_tex: None,
            last_hsv_tune_version: 0,
            logs_open: false,
            monitor_open: false,
            hsv_window_open: false,
            theme_applied: false,
            last_hsv_enabled,
        }
    }

    pub fn mark_hsv_tune_dirty(&mut self) {
        self.model.hsv_tune_dirty = true;
    }

    /// Drop mask flag, stats, and tune images so View cannot show stale HSV.
    pub fn reset_hsv_runtime_state(&mut self) {
        self.hsv_window_open = false;
        self.model.hsv_mask_overlay_enabled = false;
        self.model.hsv = HsvMaskStats::default();
        self.model.hsv_coverage_pct = 0.0;
        self.model.hsv_tune_source = None;
        self.model.hsv_tune_raw = None;
        self.model.hsv_tune_morph = None;
        self.model.hsv_tune_overlay = None;
        self.model.hsv_tune_version = self.model.hsv_tune_version.wrapping_add(1);
        self.model.hsv_tune_dirty = false;
        self.hsv_tune_source_tex = None;
        self.hsv_tune_raw_tex = None;
        self.hsv_tune_morph_tex = None;
        self.hsv_tune_overlay_tex = None;
        self.last_hsv_tune_version = self.model.hsv_tune_version;
    }

    pub fn needs_hsv_tune_refresh(&self) -> bool {
        self.model.config.vision_algorithms.hsv_tracking.enabled
            && (self.hsv_window_open
                || (self.monitor_open && self.model.hsv_mask_overlay_enabled))
    }

    fn sync_hsv_enable_edge(&mut self) {
        let hsv_on = self.model.config.vision_algorithms.hsv_tracking.enabled;
        if self.last_hsv_enabled && !hsv_on {
            self.reset_hsv_runtime_state();
        }
        if !hsv_on {
            // Belt-and-suspenders while disabled.
            self.hsv_window_open = false;
            self.model.hsv_mask_overlay_enabled = false;
        }
        self.last_hsv_enabled = hsv_on;
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
            let scale = self.model.config.gui.ui_scale.clamp(0.75, 2.0);
            if (ctx.pixels_per_point() - scale).abs() > 0.01 {
                ctx.set_pixels_per_point(scale);
            }
        }

        // DXGI path is fixed — keep preference pinned.
        self.model.backend_preference = CaptureBackendPreference::DxgiDuplication;
        self.sync_hsv_enable_edge();

        if self.monitor_open {
            self.sync_preview_texture(ctx);
        }
        let hsv_on = self.model.config.vision_algorithms.hsv_tracking.enabled;
        if hsv_on
            && (self.hsv_window_open
                || (self.monitor_open && self.model.hsv_mask_overlay_enabled))
        {
            self.sync_hsv_tune_textures(ctx);
        }

        let mut commands = Vec::new();

        egui::TopBottomPanel::top("shell_transport")
            .frame(panel_frame())
            .show(ctx, |ui| {
                self.draw_transport_bar(ui, &mut commands);
            });

        egui::TopBottomPanel::bottom("shell_logs")
            .resizable(true)
            .default_height(if self.logs_open {
                LOG_DRAWER_H
            } else {
                LOG_DRAWER_COLLAPSED
            })
            .min_height(LOG_DRAWER_COLLAPSED)
            .frame(panel_frame())
            .show(ctx, |ui| {
                self.draw_log_drawer(ui);
            });

        egui::CentralPanel::default()
            .frame(
                Frame::new()
                    .fill(PAPER)
                    .inner_margin(Margin::symmetric(12, 10)),
            )
            .show(ctx, |ui| {
                self.draw_compact_body(ui, &mut commands);
            });

        if self.monitor_open {
            self.show_monitor_viewport(ctx);
        }
        if self.hsv_window_open {
            self.show_hsv_tools_viewport(ctx, &mut commands);
        }

        commands
    }

    fn show_monitor_viewport(&mut self, ctx: &egui::Context) {
        let mut close_requested = false;

        let monitor_title = self.model.config.concealment.monitor_title().to_owned();
        ctx.show_viewport_immediate(
            egui::ViewportId::from_hash_of("vp_preview"),
            egui::ViewportBuilder::default()
                .with_title(monitor_title)
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
                                .fill(PAPER)
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

    fn show_hsv_tools_viewport(&mut self, ctx: &egui::Context, commands: &mut Vec<UiCommand>) {
        let mut close_requested = false;

        ctx.show_viewport_immediate(
            egui::ViewportId::from_hash_of("vp_hsv_tools"),
            egui::ViewportBuilder::default()
                .with_title("Color Picker")
                .with_inner_size([900.0, 580.0])
                .with_min_inner_size([720.0, 480.0]),
            |ctx, class| {
                if ctx.input(|i| i.viewport().close_requested()) {
                    close_requested = true;
                }

                // Dark neutral tool surface — color accuracy over brand chrome.
                let frame = Frame::new().fill(PICKER_BG).inner_margin(Margin::same(12));

                if class == egui::ViewportClass::Embedded {
                    egui::Window::new("Color Picker")
                        .default_size([900.0, 580.0])
                        .show(ctx, |ui| {
                            self.draw_hsv_studio(ui, commands);
                        });
                } else {
                    egui::CentralPanel::default().frame(frame).show(ctx, |ui| {
                        self.draw_hsv_studio(ui, commands);
                    });
                }
            },
        );

        if close_requested {
            self.hsv_window_open = false;
        }
    }

    /// Function-first HSV color picker: controls left, live previews right.
    fn draw_hsv_studio(&mut self, ui: &mut egui::Ui, commands: &mut Vec<UiCommand>) {
        // Header
        ui.horizontal(|ui| {
            ui.label(
                RichText::new("Color Picker")
                    .strong()
                    .size(16.0)
                    .color(PICKER_INK),
            );
            ui.add_space(10.0);
            Frame::new()
                .fill(PICKER_RAISED)
                .stroke(Stroke::new(1.0, PICKER_BORDER))
                .corner_radius(CornerRadius::same(6))
                .inner_margin(Margin::symmetric(8, 3))
                .show(ui, |ui| {
                    ui.label(
                        RichText::new(format!(
                            "Coverage {:.1}%  ·  Objects {}",
                            self.model.hsv_coverage_pct,
                            self.model.hsv_hit_count()
                        ))
                        .small()
                        .color(PICKER_MUTED),
                    );
                });
            if !self.model.capture_running {
                ui.add_space(8.0);
                ui.label(
                    RichText::new("Start share to see live masks")
                        .small()
                        .color(PICKER_ACCENT),
                );
            }
        });
        ui.add_space(10.0);

        let full = ui.available_size();
        let left_w = (full.x * 0.42).clamp(300.0, 400.0);
        let right_w = (full.x - left_w - 12.0).max(280.0);
        let body_h = full.y.max(320.0);

        ui.horizontal(|ui| {
            ui.allocate_ui_with_layout(
                Vec2::new(left_w, body_h),
                egui::Layout::top_down(Align::Min),
                |ui| {
                    Frame::new()
                        .fill(PICKER_PANEL)
                        .stroke(Stroke::new(1.0, PICKER_BORDER))
                        .corner_radius(CornerRadius::same(8))
                        .inner_margin(Margin::same(12))
                        .show(ui, |ui| {
                            ui.set_min_width(left_w - 8.0);
                            self.draw_hsv_tools(ui, commands);
                        });
                },
            );

            ui.add_space(10.0);

            ui.allocate_ui_with_layout(
                Vec2::new(right_w, body_h),
                egui::Layout::top_down(Align::Min),
                |ui| {
                    ui.set_min_width(right_w - 4.0);
                    ui.set_min_height(body_h - 4.0);
                    self.draw_hsv_tuning_panel(ui);
                },
            );
        });
    }

    fn draw_hsv_tuning_panel(&mut self, ui: &mut egui::Ui) {
        let available = ui.available_size();
        let col_w = ((available.x - 16.0) / 3.0).max(100.0);
        let preview_h = (available.y - 28.0).max(200.0);

        ui.columns(3, |columns| {
            draw_picker_tune_column(
                &mut columns[0],
                "Source",
                self.hsv_tune_source_tex.as_ref(),
                col_w,
                preview_h,
                &self.model,
                None,
            );
            columns[1].vertical(|ui| {
                ui.horizontal(|ui| {
                    ui.label(
                        RichText::new("Mask")
                            .strong()
                            .size(13.0)
                            .color(PICKER_INK),
                    );
                    ui.selectable_value(
                        &mut self.model.hsv_preview_mask_mode,
                        HsvPreviewMaskMode::RawMask,
                        RichText::new("Raw").color(PICKER_INK),
                    );
                    ui.selectable_value(
                        &mut self.model.hsv_preview_mask_mode,
                        HsvPreviewMaskMode::MorphMask,
                        RichText::new("Morph").color(PICKER_INK),
                    );
                });
                let mask_tex = match self.model.hsv_preview_mask_mode {
                    HsvPreviewMaskMode::RawMask => self.hsv_tune_raw_tex.as_ref(),
                    HsvPreviewMaskMode::MorphMask => self.hsv_tune_morph_tex.as_ref(),
                };
                draw_picker_tune_image(ui, mask_tex, col_w, preview_h - 26.0);
            });
            draw_picker_tune_column(
                &mut columns[2],
                "Detection",
                self.hsv_tune_overlay_tex.as_ref(),
                col_w,
                preview_h,
                &self.model,
                Some(&self.model.hsv),
            );
        });
    }

    /// Compact main body — display pick + modes + mini status (MP3-style).
    fn draw_compact_body(&mut self, ui: &mut egui::Ui, commands: &mut Vec<UiCommand>) {
        // Display picker card
        Frame::new()
            .fill(SURFACE_RAISED)
            .stroke(Stroke::new(1.5, BORDER))
            .corner_radius(CornerRadius::same(12))
            .inner_margin(Margin::symmetric(12, 10))
            .show(ui, |ui| {
                ui.horizontal(|ui| {
                    ui.label(
                        RichText::new("Display")
                            .small()
                            .strong()
                            .color(TEXT_MUTED),
                    );
                    ui.add_space(4.0);

                    let selected_label = self
                        .model
                        .targets
                        .get(self.model.selected_target)
                        .map(|t| {
                            format!(
                                "{}{} · {}×{}",
                                t.name,
                                if t.primary { " ★" } else { "" },
                                t.size.width,
                                t.size.height
                            )
                        })
                        .unwrap_or_else(|| "No display".to_owned());

                    egui::ComboBox::from_id_salt("display_pick")
                        .selected_text(selected_label)
                        .width(ui.available_width().min(240.0).max(160.0))
                        .show_ui(ui, |ui| {
                            if self.model.targets.is_empty() {
                                ui.label(RichText::new("Refresh targets").color(TEXT_MUTED));
                            }
                            for (index, target) in self.model.targets.iter().enumerate() {
                                let label = format!(
                                    "{}{} ({}×{})",
                                    target.name,
                                    if target.primary { " ★" } else { "" },
                                    target.size.width,
                                    target.size.height
                                );
                                if ui
                                    .selectable_label(self.model.selected_target == index, label)
                                    .clicked()
                                {
                                    self.model.selected_target = index;
                                }
                            }
                        });

                    if ui
                        .small_button(RichText::new("↻").color(TEXT_MUTED))
                        .on_hover_text("Refresh displays")
                        .clicked()
                    {
                        commands.push(UiCommand::RefreshTargets);
                    }
                });
            });

        ui.add_space(8.0);

        // Mode chips + FPS
        Frame::new()
            .fill(SURFACE_RAISED)
            .stroke(Stroke::new(1.5, BORDER))
            .corner_radius(CornerRadius::same(12))
            .inner_margin(Margin::symmetric(12, 10))
            .show(ui, |ui| {
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
                    ui.with_layout(egui::Layout::right_to_left(Align::Center), |ui| {
                        ui.add(
                            egui::DragValue::new(&mut self.model.config.performance.target_fps)
                                .range(1..=360)
                                .suffix(" fps")
                                .speed(1.0),
                        );
                        ui.label(RichText::new("Rate").small().color(TEXT_MUTED));
                    });
                });
                // HSV disable cleanup runs in sync_hsv_enable_edge() each frame.
            });

        ui.add_space(8.0);

        // Mini status line (like now-playing metadata)
        let hsv_n = self.model.hsv_hit_count();
        let yolo_n = self.model.yolo_detection_count();
        let fps = self.model.performance.fps;
        let drop = self.model.performance.dropped_frames;
        let status = if self.model.capture_running {
            format!("H:{hsv_n}  Y:{yolo_n}  ·  {fps:.0} fps  ·  {drop} drop")
        } else {
            "Ready — press Start to share".to_owned()
        };
        ui.label(RichText::new(status).small().color(TEXT_MUTED));

        if let Some(size) = self.model.capture_frame_size.filter(|_| self.model.capture_running) {
            ui.label(
                RichText::new(format!("{}×{}", size.width, size.height))
                    .small()
                    .color(TEXT_MUTED),
            );
        }

        if let Some(error) = &self.model.last_error {
            ui.add_space(6.0);
            Frame::new()
                .fill(Color32::from_rgb(0xF5, 0xE0, 0xD8))
                .corner_radius(CornerRadius::same(8))
                .inner_margin(Margin::symmetric(10, 6))
                .show(ui, |ui| {
                    ui.colored_label(Color32::from_rgb(0x8B, 0x3A, 0x2F), error);
                });
        }
    }

    fn sync_hsv_tune_textures(&mut self, ctx: &egui::Context) {
        if self.model.hsv_tune_version == self.last_hsv_tune_version {
            return;
        }
        self.last_hsv_tune_version = self.model.hsv_tune_version;
        sync_texture_slot(
            ctx,
            &mut self.hsv_tune_source_tex,
            "hsv-tune-source",
            self.model.hsv_tune_source.as_ref(),
        );
        sync_texture_slot(
            ctx,
            &mut self.hsv_tune_raw_tex,
            "hsv-tune-raw",
            self.model.hsv_tune_raw.as_ref(),
        );
        sync_texture_slot(
            ctx,
            &mut self.hsv_tune_morph_tex,
            "hsv-tune-morph",
            self.model.hsv_tune_morph.as_ref(),
        );
        sync_texture_slot(
            ctx,
            &mut self.hsv_tune_overlay_tex,
            "hsv-tune-overlay",
            self.model.hsv_tune_overlay.as_ref(),
        );
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

    fn draw_transport_bar(&mut self, ui: &mut egui::Ui, commands: &mut Vec<UiCommand>) {
        // Row 1: title | primary Start/Stop (never share a line with status chip)
        ui.horizontal(|ui| {
            ui.label(
                RichText::new(self.model.config.concealment.display_name())
                    .strong()
                    .size(15.0)
                    .color(TEXT_PRIMARY),
            );
            ui.with_layout(egui::Layout::right_to_left(Align::Center), |ui| {
                let (label, stopping) = if self.model.capture_running {
                    ("Stop", true)
                } else {
                    ("Start", false)
                };
                let start_btn = egui::Button::new(
                    RichText::new(label)
                        .strong()
                        .color(CREAM_ON_ACCENT)
                        .size(13.0),
                )
                .fill(if stopping { BRICK } else { ACCENT })
                .corner_radius(CornerRadius::same(8))
                .min_size(Vec2::new(84.0, 30.0));
                if ui.add(start_btn).clicked() {
                    commands.push(if self.model.capture_running {
                        UiCommand::StopCapture
                    } else {
                        UiCommand::StartCapture
                    });
                }
            });
        });

        ui.add_space(4.0);

        // Row 2: status chip | secondary actions
        ui.horizontal(|ui| {
            status_chip(ui, self.model.capture_running, self.model.last_error.is_some());
            ui.with_layout(egui::Layout::right_to_left(Align::Center), |ui| {
                ui.menu_button(RichText::new("···").strong(), |ui| {
                    if ui.button("Save settings").clicked() {
                        commands.push(UiCommand::SaveSettings);
                        ui.close();
                    }
                    if ui.button("Load model").clicked() {
                        commands.push(UiCommand::LoadModel);
                        ui.close();
                    }
                    if ui.button("Refresh displays").clicked() {
                        commands.push(UiCommand::RefreshTargets);
                        ui.close();
                    }
                    ui.separator();
                    ui.horizontal(|ui| {
                        ui.label("UI scale");
                        ui.add(
                            egui::DragValue::new(&mut self.model.config.gui.ui_scale)
                                .range(0.75..=2.0)
                                .speed(0.05),
                        );
                    });
                    ui.horizontal(|ui| {
                        ui.label("Buffer");
                        ui.add(
                            egui::DragValue::new(
                                &mut self.model.config.performance.frame_buffer_size,
                            )
                            .range(1..=32),
                        );
                    });
                    ui.horizontal(|ui| {
                        ui.label("GPU id");
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
                    ui.separator();
                    let ep = provider_label(&self.model.provider_state);
                    ui.label(RichText::new(format!("EP: {ep}")).small().color(TEXT_MUTED));
                    if let Some(backend) = self.model.active_backend {
                        ui.label(
                            RichText::new(format!("IO: {}", backend_kind_label(backend)))
                                .small()
                                .color(TEXT_MUTED),
                        );
                    }
                });

                let hsv_on = self.model.config.vision_algorithms.hsv_tracking.enabled;
                ui.add_enabled_ui(hsv_on, |ui| {
                    let hsv_label = if self.hsv_window_open {
                        "Color ✕"
                    } else {
                        "Color"
                    };
                    if ui
                        .add_sized([52.0, 24.0], egui::Button::new(hsv_label))
                        .on_hover_text("Open HSV color picker")
                        .clicked()
                    {
                        self.hsv_window_open = !self.hsv_window_open;
                    }
                });

                let mon_label = if self.monitor_open {
                    "View ✕"
                } else {
                    "View"
                };
                if ui
                    .add_sized([52.0, 24.0], egui::Button::new(mon_label))
                    .on_hover_text("Open live preview window")
                    .clicked()
                {
                    self.monitor_open = !self.monitor_open;
                }
            });
        });
    }

    fn draw_hsv_tools(&mut self, ui: &mut egui::Ui, commands: &mut Vec<UiCommand>) {
        let tracking = &self.model.config.vision_algorithms.hsv_tracking;
        let mut h_min = tracking.lower_bound[0];
        let mut h_max = tracking.upper_bound[0];
        let mut s_min = tracking.lower_bound[1];
        let mut s_max = tracking.upper_bound[1];
        let mut v_min = tracking.lower_bound[2];
        let mut v_max = tracking.upper_bound[2];
        let mut min_area = tracking.min_contour_area;
        let mut morph_kernel = tracking.morphology_kernel_size;

        ui.label(
            RichText::new("Selected range")
                .strong()
                .size(12.0)
                .color(PICKER_INK),
        );
        ui.add_space(4.0);
        draw_hsv_range_swatches(ui, h_min, s_min, v_min, h_max, s_max, v_max);
        ui.add_space(12.0);

        ui.label(
            RichText::new("Channel bounds  (OpenCV H 0–179 · S/V 0–255)")
                .strong()
                .size(12.0)
                .color(PICKER_INK),
        );
        ui.add_space(6.0);

        let mut changed = false;
        // Snapshot mids for spectrum painting (avoid simultaneous mut borrows).
        let hue_mid = h_min.saturating_add(h_max) / 2;
        let sat_mid = s_min.saturating_add(s_max) / 2;
        changed |= hsv_channel_row(ui, "Hue", &mut h_min, &mut h_max, 179, ChannelKind::Hue, 0, 255);
        changed |= hsv_channel_row(
            ui,
            "Sat",
            &mut s_min,
            &mut s_max,
            255,
            ChannelKind::Sat,
            hue_mid,
            255,
        );
        changed |= hsv_channel_row(
            ui,
            "Val",
            &mut v_min,
            &mut v_max,
            255,
            ChannelKind::Val,
            hue_mid,
            sat_mid,
        );

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
            let hsv = &mut self.model.config.vision_algorithms.hsv_tracking;
            hsv.lower_bound = [h_min, s_min, v_min];
            hsv.upper_bound = [h_max, s_max, v_max];
            self.mark_hsv_tune_dirty();
        }

        ui.add_space(8.0);
        egui::CollapsingHeader::new(RichText::new("Advanced").color(PICKER_MUTED))
            .default_open(false)
            .show(ui, |ui| {
                ui.horizontal(|ui| {
                    ui.vertical(|ui| {
                        ui.label(
                            RichText::new("Min area (px)")
                                .small()
                                .color(PICKER_MUTED),
                        );
                        let r = ui.add(
                            egui::DragValue::new(&mut min_area)
                                .range(1..=10_000)
                                .speed(1.0),
                        );
                        if r.changed() {
                            self.model.config.vision_algorithms.hsv_tracking.min_contour_area =
                                min_area;
                            self.mark_hsv_tune_dirty();
                        }
                    });
                    ui.add_space(12.0);
                    ui.vertical(|ui| {
                        ui.label(
                            RichText::new("Morph kernel")
                                .small()
                                .color(PICKER_MUTED),
                        );
                        let r = ui.add(
                            egui::DragValue::new(&mut morph_kernel)
                                .range(1..=15)
                                .speed(1.0),
                        );
                        if r.changed() {
                            self.model
                                .config
                                .vision_algorithms
                                .hsv_tracking
                                .morphology_kernel_size = morph_kernel;
                            self.mark_hsv_tune_dirty();
                        }
                    });
                });
                ui.add_space(8.0);
                ui.horizontal(|ui| {
                    if ui
                        .add(
                            egui::Button::new(RichText::new("Load preset").color(PICKER_INK))
                                .fill(PICKER_RAISED)
                                .stroke(Stroke::new(1.0, PICKER_BORDER)),
                        )
                        .clicked()
                    {
                        commands.push(UiCommand::LoadHsvSettings);
                    }
                    if ui
                        .add(
                            egui::Button::new(RichText::new("Save preset").color(PICKER_INK))
                                .fill(PICKER_ACCENT),
                        )
                        .clicked()
                    {
                        commands.push(UiCommand::SaveHsvSettings);
                    }
                });
            });
    }

    fn draw_preview_stage(&mut self, ui: &mut egui::Ui) {
        let hsv_on = self.model.config.vision_algorithms.hsv_tracking.enabled;
        let yolo_on = self.model.config.vision_algorithms.yolo26_detection.enabled;
        let hsv_hits = if hsv_on {
            self.model.hsv_hit_count()
        } else {
            0
        };
        let yolo_n = if yolo_on {
            self.model.yolo_detection_count()
        } else {
            0
        };
        ui.horizontal(|ui| {
            let counts = match (hsv_on, yolo_on) {
                (true, true) => format!("HSV {hsv_hits}  ·  YOLO {yolo_n}"),
                (true, false) => format!("HSV {hsv_hits}"),
                (false, true) => format!("YOLO {yolo_n}"),
                (false, false) => "Preview".to_owned(),
            };
            ui.label(RichText::new(counts).small().color(TEXT_MUTED));
            if hsv_on {
                if ui
                    .checkbox(&mut self.model.hsv_mask_overlay_enabled, "Mask")
                    .changed()
                {
                    self.mark_hsv_tune_dirty();
                }
            }
            if yolo_on
                && matches!(
                    self.model.provider_state,
                    ProviderState::Uninitialized | ProviderState::Failed(_)
                )
            {
                ui.label(
                    RichText::new("Loading model…")
                        .small()
                        .color(TERRACOTTA),
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
            self.hsv_tune_morph_tex.as_ref(),
        );
    }

    fn draw_log_drawer(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            let label = if self.logs_open {
                "▾ Session log"
            } else {
                "▸ Session log"
            };
            if ui
                .add(egui::Button::new(RichText::new(label).strong().size(12.0)).frame(false))
                .clicked()
            {
                self.logs_open = !self.logs_open;
            }
            ui.label(
                RichText::new(format!("{}", self.model.logs.len()))
                    .small()
                    .color(TEXT_MUTED),
            );
        });
        if self.logs_open {
            egui::ScrollArea::vertical()
                .stick_to_bottom(true)
                .max_height(LOG_DRAWER_H - 8.0)
                .show(ui, |ui| {
                    for line in &self.model.logs {
                        ui.label(RichText::new(line).monospace().size(11.0).color(TEXT_MUTED));
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
    hsv_mask_texture: Option<&TextureHandle>,
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

    let hsv_on = model.config.vision_algorithms.hsv_tracking.enabled;
    let yolo_on = model.config.vision_algorithms.yolo26_detection.enabled;

    // Mask overlay only while HSV is enabled (flag alone is not enough).
    if hsv_on && model.hsv_mask_overlay_enabled {
        if let Some(mask_tex) = hsv_mask_texture {
            painter.image(
                mask_tex.id(),
                image_rect,
                Rect::from_min_max(Pos2::ZERO, Pos2::new(1.0, 1.0)),
                Color32::from_rgba_premultiplied(0xE8, 0xB8, 0x6D, 110),
            );
        }
    }

    // Never flood-fill ROI (full-frame analysis ROI was painting a green film over View).
    // Only stroke a true crop that is clearly smaller than the full capture.
    if let Some(roi) = model.capture_roi {
        let full_area = (capture_size.width as u64).saturating_mul(capture_size.height as u64);
        let roi_area = (roi.rect.width as u64).saturating_mul(roi.rect.height as u64);
        let is_partial = full_area > 0 && roi_area * 100 < full_area * 95;
        if is_partial {
            let roi_screen = frame_rect_to_screen(roi.rect);
            painter.rect_stroke(
                roi_screen,
                CornerRadius::ZERO,
                Stroke::new(1.5, ROI_STROKE),
                egui::StrokeKind::Outside,
            );
        }
    }

    // HSV contour boxes — never draw stale stats after HSV is disabled.
    if hsv_on {
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
    }

    if !yolo_on {
        return;
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

fn sync_texture_slot(
    ctx: &egui::Context,
    slot: &mut Option<TextureHandle>,
    label: &str,
    image: Option<&ColorImage>,
) {
    match image {
        Some(img) => {
            if let Some(texture) = slot {
                texture.set(img.clone(), TextureOptions::LINEAR);
            } else {
                *slot = Some(ctx.load_texture(label, img.clone(), TextureOptions::LINEAR));
            }
        }
        None => {
            *slot = None;
        }
    }
}

fn apply_theme(ctx: &egui::Context, ui_scale: f32) {
    let mut visuals = egui::Visuals::light();
    visuals.panel_fill = SURFACE;
    visuals.window_fill = SURFACE_RAISED;
    visuals.extreme_bg_color = PAPER;
    visuals.faint_bg_color = PAPER;
    visuals.override_text_color = Some(INK);
    visuals.selection.bg_fill = ACCENT_SOFT;
    visuals.selection.stroke = Stroke::new(1.0, MOSS);
    visuals.widgets.inactive.bg_fill = SURFACE_RAISED;
    visuals.widgets.inactive.weak_bg_fill = SURFACE;
    visuals.widgets.inactive.fg_stroke = Stroke::new(1.0, INK);
    visuals.widgets.inactive.bg_stroke = Stroke::new(1.0, BORDER);
    visuals.widgets.hovered.bg_fill = Color32::from_rgb(0xF0, 0xE8, 0xD8);
    visuals.widgets.hovered.weak_bg_fill = Color32::from_rgb(0xF0, 0xE8, 0xD8);
    visuals.widgets.hovered.bg_stroke = Stroke::new(1.5, MOSS);
    visuals.widgets.hovered.fg_stroke = Stroke::new(1.0, INK);
    visuals.widgets.active.bg_fill = Color32::from_rgb(0xE8, 0xDF, 0xCE);
    visuals.widgets.active.bg_stroke = Stroke::new(1.5, TERRACOTTA);
    visuals.widgets.open.bg_fill = SURFACE_RAISED;
    visuals.widgets.noninteractive.fg_stroke = Stroke::new(1.0, INK_MUTED);
    visuals.hyperlink_color = TERRACOTTA;
    visuals.warn_fg_color = TERRACOTTA;
    visuals.error_fg_color = BRICK;
    ctx.set_visuals(visuals);

    let mut style = (*ctx.style()).clone();
    style.spacing.item_spacing = Vec2::new(8.0, 6.0);
    style.spacing.button_padding = Vec2::new(10.0, 5.0);
    style.spacing.window_margin = Margin::same(10);
    style.spacing.slider_width = 120.0;
    style.visuals = ctx.style().visuals.clone();
    ctx.set_style(style);

    let scale = ui_scale.clamp(0.75, 2.0);
    // Only update when the scale actually changes — calling every frame can
    // destabilize the Win32/wgpu surface and surface as 0xc000041d.
    if (ctx.pixels_per_point() - scale).abs() > 0.01 {
        ctx.set_pixels_per_point(scale);
    }
}

fn panel_frame() -> Frame {
    Frame::new()
        .fill(SURFACE)
        .inner_margin(Margin::symmetric(12, 8))
        .stroke(Stroke::new(1.0, BORDER))
}

#[derive(Clone, Copy)]
enum ChannelKind {
    Hue,
    Sat,
    Val,
}

/// OpenCV-style HSV (H 0–179, S/V 0–255) → sRGB for swatches.
fn opencv_hsv_to_color32(h: u8, s: u8, v: u8) -> Color32 {
    let h = f32::from(h) * 2.0; // 0..358
    let s = f32::from(s) / 255.0;
    let v = f32::from(v) / 255.0;
    let c = v * s;
    let h_prime = h / 60.0;
    let x = c * (1.0 - (h_prime % 2.0 - 1.0).abs());
    let (r1, g1, b1) = match h_prime as i32 {
        0 => (c, x, 0.0),
        1 => (x, c, 0.0),
        2 => (0.0, c, x),
        3 => (0.0, x, c),
        4 => (x, 0.0, c),
        _ => (c, 0.0, x),
    };
    let m = v - c;
    let r = ((r1 + m) * 255.0).round().clamp(0.0, 255.0) as u8;
    let g = ((g1 + m) * 255.0).round().clamp(0.0, 255.0) as u8;
    let b = ((b1 + m) * 255.0).round().clamp(0.0, 255.0) as u8;
    Color32::from_rgb(r, g, b)
}

fn draw_hsv_range_swatches(
    ui: &mut egui::Ui,
    h_min: u8,
    s_min: u8,
    v_min: u8,
    h_max: u8,
    s_max: u8,
    v_max: u8,
) {
    let lower = opencv_hsv_to_color32(h_min, s_min, v_min);
    let upper = opencv_hsv_to_color32(h_max, s_max, v_max);

    ui.horizontal(|ui| {
        ui.vertical(|ui| {
            ui.label(RichText::new("Lower").small().color(PICKER_MUTED));
            paint_picker_swatch(ui, lower, 56.0, 40.0);
        });
        ui.add_space(8.0);
        ui.vertical(|ui| {
            ui.label(RichText::new("Blend across range").small().color(PICKER_MUTED));
            let width = (ui.available_width() - 8.0).clamp(140.0, 220.0);
            let (response, painter) =
                ui.allocate_painter(Vec2::new(width, 40.0), Sense::hover());
            let rect = response.rect;
            let steps = 24usize;
            let step_w = rect.width() / steps as f32;
            for i in 0..steps {
                let t = i as f32 / (steps.saturating_sub(1).max(1) as f32);
                let h = lerp_u8(h_min, h_max, t);
                let s = lerp_u8(s_min, s_max, t);
                let v = lerp_u8(v_min, v_max, t);
                let c = opencv_hsv_to_color32(h, s, v);
                let x = rect.min.x + i as f32 * step_w;
                painter.rect_filled(
                    Rect::from_min_size(Pos2::new(x, rect.min.y), Vec2::new(step_w + 0.5, rect.height())),
                    CornerRadius::ZERO,
                    c,
                );
            }
            // Dual outline for visibility on any hue
            painter.rect_stroke(
                rect,
                CornerRadius::same(4),
                Stroke::new(1.0, Color32::BLACK),
                egui::StrokeKind::Outside,
            );
            painter.rect_stroke(
                rect.shrink(1.0),
                CornerRadius::same(3),
                Stroke::new(1.0, Color32::WHITE),
                egui::StrokeKind::Outside,
            );
        });
        ui.add_space(8.0);
        ui.vertical(|ui| {
            ui.label(RichText::new("Upper").small().color(PICKER_MUTED));
            paint_picker_swatch(ui, upper, 56.0, 40.0);
        });
    });
}

fn lerp_u8(a: u8, b: u8, t: f32) -> u8 {
    let t = t.clamp(0.0, 1.0);
    (f32::from(a) + (f32::from(b) - f32::from(a)) * t)
        .round()
        .clamp(0.0, 255.0) as u8
}

fn paint_picker_swatch(ui: &mut egui::Ui, color: Color32, w: f32, h: f32) {
    let (response, painter) = ui.allocate_painter(Vec2::new(w, h), Sense::hover());
    let rect = response.rect;
    painter.rect_filled(rect, CornerRadius::same(4), color);
    painter.rect_stroke(
        rect,
        CornerRadius::same(4),
        Stroke::new(1.0, Color32::BLACK),
        egui::StrokeKind::Outside,
    );
    painter.rect_stroke(
        rect.shrink(1.0),
        CornerRadius::same(3),
        Stroke::new(1.0, Color32::WHITE),
        egui::StrokeKind::Outside,
    );
}

fn paint_channel_spectrum(
    painter: &egui::Painter,
    rect: Rect,
    kind: ChannelKind,
    h_mid: u8,
    s_mid: u8,
) {
    let steps = 48usize;
    let step_w = rect.width() / steps as f32;
    for i in 0..steps {
        let t = i as f32 / (steps.saturating_sub(1).max(1) as f32);
        let c = match kind {
            ChannelKind::Hue => {
                let h = (t * 179.0).round().clamp(0.0, 179.0) as u8;
                opencv_hsv_to_color32(h, 255, 255)
            }
            ChannelKind::Sat => {
                let s = (t * 255.0).round().clamp(0.0, 255.0) as u8;
                opencv_hsv_to_color32(h_mid, s, 255)
            }
            ChannelKind::Val => {
                let v = (t * 255.0).round().clamp(0.0, 255.0) as u8;
                opencv_hsv_to_color32(h_mid, s_mid, v)
            }
        };
        let x = rect.min.x + i as f32 * step_w;
        painter.rect_filled(
            Rect::from_min_size(Pos2::new(x, rect.min.y), Vec2::new(step_w + 0.5, rect.height())),
            CornerRadius::ZERO,
            c,
        );
    }
    painter.rect_stroke(
        rect,
        CornerRadius::same(3),
        Stroke::new(1.0, PICKER_BORDER),
        egui::StrokeKind::Outside,
    );
}

/// Spectrum track + Min/Max sliders + exact numeric fields.
fn hsv_channel_row(
    ui: &mut egui::Ui,
    label: &str,
    min: &mut u8,
    max: &mut u8,
    max_val: u8,
    kind: ChannelKind,
    spectrum_h: u8,
    spectrum_s: u8,
) -> bool {
    let mut changed = false;

    ui.label(RichText::new(label).strong().size(12.0).color(PICKER_INK));

    let spectrum_w = ui.available_width().max(180.0);
    let (resp, painter) = ui.allocate_painter(Vec2::new(spectrum_w, 20.0), Sense::hover());
    paint_channel_spectrum(&painter, resp.rect, kind, spectrum_h, spectrum_s);

    // Min/max markers on spectrum
    let r = resp.rect;
    let denom = f32::from(max_val).max(1.0);
    for t in [f32::from(*min) / denom, f32::from(*max) / denom] {
        let x = r.min.x + t.clamp(0.0, 1.0) * r.width();
        painter.line_segment(
            [Pos2::new(x, r.min.y - 1.0), Pos2::new(x, r.max.y + 1.0)],
            Stroke::new(2.5, Color32::WHITE),
        );
        painter.line_segment(
            [Pos2::new(x, r.min.y - 1.0), Pos2::new(x, r.max.y + 1.0)],
            Stroke::new(1.0, Color32::BLACK),
        );
    }

    ui.horizontal(|ui| {
        ui.label(RichText::new("Min").small().color(PICKER_MUTED));
        changed |= ui
            .add(egui::Slider::new(min, 0..=max_val).show_value(false))
            .changed();
        changed |= ui
            .add(egui::DragValue::new(min).range(0..=max_val).speed(1.0))
            .changed();
    });
    ui.horizontal(|ui| {
        ui.label(RichText::new("Max").small().color(PICKER_MUTED));
        changed |= ui
            .add(egui::Slider::new(max, 0..=max_val).show_value(false))
            .changed();
        changed |= ui
            .add(egui::DragValue::new(max).range(0..=max_val).speed(1.0))
            .changed();
    });
    ui.add_space(6.0);
    changed
}

fn draw_picker_tune_column(
    ui: &mut egui::Ui,
    title: &str,
    texture: Option<&TextureHandle>,
    width: f32,
    height: f32,
    model: &UiModel,
    hsv_boxes: Option<&HsvMaskStats>,
) {
    ui.label(RichText::new(title).strong().size(13.0).color(PICKER_INK));
    let (response, painter) =
        ui.allocate_painter(Vec2::new(width, height.max(80.0)), Sense::hover());
    let rect = response.rect;
    painter.rect_filled(rect, CornerRadius::same(4), PICKER_RAISED);
    let mut image_rect = rect;
    if let Some(tex) = texture {
        let tex_size = tex.size_vec2();
        let fitted = fit_inside(tex_size, rect.size() - Vec2::splat(4.0));
        image_rect = Rect::from_center_size(rect.center(), fitted);
        painter.image(
            tex.id(),
            image_rect,
            Rect::from_min_max(Pos2::ZERO, Pos2::new(1.0, 1.0)),
            Color32::WHITE,
        );
    } else {
        painter.text(
            rect.center(),
            egui::Align2::CENTER_CENTER,
            "No frame — start share",
            FontId::proportional(12.0),
            PICKER_MUTED,
        );
    }
    if let (Some(hsv), Some(capture_size)) = (hsv_boxes, model.capture_frame_size) {
        if capture_size.width > 0 && capture_size.height > 0 && !hsv.objects.is_empty() {
            let sx = image_rect.width() / capture_size.width as f32;
            let sy = image_rect.height() / capture_size.height as f32;
            for obj in &hsv.objects {
                let r = obj.rect;
                let screen = Rect::from_min_max(
                    Pos2::new(
                        image_rect.min.x + r.x as f32 * sx,
                        image_rect.min.y + r.y as f32 * sy,
                    ),
                    Pos2::new(
                        image_rect.min.x + (r.x + r.width as i32) as f32 * sx,
                        image_rect.min.y + (r.y + r.height as i32) as f32 * sy,
                    ),
                );
                painter.rect_stroke(
                    screen,
                    CornerRadius::ZERO,
                    Stroke::new(2.0, Color32::from_rgb(255, 200, 80)),
                    egui::StrokeKind::Outside,
                );
            }
        }
    }
    painter.rect_stroke(
        rect,
        CornerRadius::same(4),
        Stroke::new(1.0, PICKER_BORDER),
        egui::StrokeKind::Outside,
    );
}

fn draw_picker_tune_image(
    ui: &mut egui::Ui,
    texture: Option<&TextureHandle>,
    width: f32,
    height: f32,
) {
    let (response, painter) =
        ui.allocate_painter(Vec2::new(width, height.max(80.0)), Sense::hover());
    let rect = response.rect;
    painter.rect_filled(rect, CornerRadius::same(4), PICKER_RAISED);
    if let Some(tex) = texture {
        let tex_size = tex.size_vec2();
        let fitted = fit_inside(tex_size, rect.size() - Vec2::splat(4.0));
        let image_rect = Rect::from_center_size(rect.center(), fitted);
        painter.image(
            tex.id(),
            image_rect,
            Rect::from_min_max(Pos2::ZERO, Pos2::new(1.0, 1.0)),
            Color32::WHITE,
        );
    } else {
        painter.text(
            rect.center(),
            egui::Align2::CENTER_CENTER,
            "No frame — start share",
            FontId::proportional(12.0),
            PICKER_MUTED,
        );
    }
    painter.rect_stroke(
        rect,
        CornerRadius::same(4),
        Stroke::new(1.0, PICKER_BORDER),
        egui::StrokeKind::Outside,
    );
}

fn status_chip(ui: &mut egui::Ui, sharing: bool, errored: bool) {
    let (text, fill, fg) = if errored {
        (
            "Error",
            Color32::from_rgb(0xF5, 0xE0, 0xD8),
            Color32::from_rgb(0x8B, 0x3A, 0x2F),
        )
    } else if sharing {
        (
            "Sharing",
            Color32::from_rgb(0xE2, 0xED, 0xE4),
            Color32::from_rgb(0x3F, 0x6B, 0x4F),
        )
    } else {
        (
            "Idle",
            Color32::from_rgb(0xF0, 0xE8, 0xD8),
            INK_MUTED,
        )
    };
    Frame::new()
        .fill(fill)
        .stroke(Stroke::new(1.0, BORDER))
        .corner_radius(CornerRadius::same(10))
        .inner_margin(Margin::symmetric(8, 3))
        .show(ui, |ui| {
            ui.label(RichText::new(text).strong().color(fg).size(12.0));
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


