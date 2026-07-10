use capture_core::{CpuBuffer, HsvTuneResult, PixelFormat};
use eframe::egui::{self, Color32};
use ui::UiModel;

pub(crate) fn cpu_buffer_to_color_image(buffer: &CpuBuffer) -> egui::ColorImage {
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

pub(crate) fn apply_hsv_tune_to_model(
    model: &mut UiModel,
    tune: &HsvTuneResult,
    source: &CpuBuffer,
) {
    if !model.config.vision_algorithms.hsv_tracking.enabled {
        return;
    }
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
            if mask_idx >= pixels.len() {
                continue;
            }
            let base = pixels[mask_idx];
            let t = 0.45;
            pixels[mask_idx] = Color32::from_rgba_unmultiplied(
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
    ((a as f32) * (1.0 - t) + (b as f32) * t)
        .round()
        .clamp(0.0, 255.0) as u8
}
