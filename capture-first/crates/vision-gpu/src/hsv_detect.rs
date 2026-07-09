//! HSV color detection — ported from [HsvColorPicker](https://github.com/NAMUORI00/HsvColorPicker)
//! (mask → dilate → contour → bounding boxes, OpenCV-style hue wrap).

use capture_core::{
    CpuBuffer, HsvDetectedObject, HsvMaskStats, HsvSettings, HsvTuneResult, PixelFormat, RectI,
    Size2D,
};

#[derive(Clone, Copy, Debug, Default)]
struct HsvPixel {
    h: u8,
    s: u8,
    v: u8,
}

#[derive(Clone, Copy, Debug)]
struct HsvRange {
    h_lower: u8,
    h_upper: u8,
    s_lower: u8,
    s_upper: u8,
    v_lower: u8,
    v_upper: u8,
}

const DIRECTIONS: [(i32, i32); 8] = [
    (0, 1),
    (1, 1),
    (1, 0),
    (1, -1),
    (0, -1),
    (-1, -1),
    (-1, 0),
    (-1, 1),
];

/// Run HsvColorPicker-style detection on a CPU buffer; scale boxes to `frame_size`.
#[allow(dead_code)]
pub fn detect_hsv_on_buffer(
    buffer: &CpuBuffer,
    settings: &HsvSettings,
    frame_size: Size2D,
) -> HsvMaskStats {
    detect_hsv_with_preview(buffer, settings, frame_size).stats
}

/// Detection plus raw/morph masks for live HSV tuning UI.
pub fn detect_hsv_with_preview(
    buffer: &CpuBuffer,
    settings: &HsvSettings,
    frame_size: Size2D,
) -> HsvTuneResult {
    if !settings.enabled || buffer.width == 0 || buffer.height == 0 {
        return HsvTuneResult::default();
    }

    let range = HsvRange {
        h_lower: settings.range.lower[0],
        h_upper: settings.range.upper[0],
        s_lower: settings.range.lower[1],
        s_upper: settings.range.upper[1],
        v_lower: settings.range.lower[2],
        v_upper: settings.range.upper[2],
    };

    let hsv = rgb_to_hsv_buffer(buffer);
    let mask = create_mask(&hsv, buffer.width, buffer.height, &range);
    let kernel = settings.morphology_kernel_size.max(1);
    let dilated = dilate(&mask, buffer.width, buffer.height, kernel, 2);
    let min_area = settings.min_contour_area.max(1) as f64;
    let contours = find_contours(&dilated, buffer.width, buffer.height, min_area);

    let hit_count = mask.iter().filter(|&&v| v > 0).count();
    let sx = if buffer.width > 0 {
        frame_size.width as f32 / buffer.width as f32
    } else {
        1.0
    };
    let sy = if buffer.height > 0 {
        frame_size.height as f32 / buffer.height as f32
    } else {
        1.0
    };

    let mut objects = Vec::new();
    let mut bbox_union: Option<RectI> = None;

    for (contour, area) in contours {
        let (x, y, w, h) = get_bounding_rect(&contour);
        let frame_rect = RectI::new(
            (x as f32 * sx).round() as i32,
            (y as f32 * sy).round() as i32,
            (w as f32 * sx).round().max(1.0) as u32,
            (h as f32 * sy).round().max(1.0) as u32,
        );
        objects.push(HsvDetectedObject {
            rect: frame_rect,
            area: area as f32,
        });
        bbox_union = Some(match bbox_union {
            Some(existing) => existing.union(frame_rect),
            None => frame_rect,
        });
    }

    HsvTuneResult {
        stats: HsvMaskStats {
            hit_count,
            bbox_union,
            objects,
        },
        preview_width: buffer.width,
        preview_height: buffer.height,
        raw_mask: mask,
        morph_mask: dilated,
    }
}

fn rgb_to_hsv_buffer(buffer: &CpuBuffer) -> Vec<HsvPixel> {
    let len = (buffer.width * buffer.height) as usize;
    let mut hsv = Vec::with_capacity(len);
    for y in 0..buffer.height as usize {
        for x in 0..buffer.width as usize {
            let idx = y * buffer.stride as usize + x * 4;
            let px = &buffer.data[idx..idx + 4];
            let (r, g, b) = match buffer.pixel_format {
                PixelFormat::Bgra8Unorm => (px[2], px[1], px[0]),
                PixelFormat::Rgba8Unorm => (px[0], px[1], px[2]),
            };
            hsv.push(rgb_to_hsv_pixel(r, g, b));
        }
    }
    hsv
}

fn rgb_to_hsv_pixel(r: u8, g: u8, b: u8) -> HsvPixel {
    let r = r as f64 / 255.0;
    let g = g as f64 / 255.0;
    let b = b as f64 / 255.0;

    let maxc = r.max(g).max(b);
    let minc = r.min(g).min(b);
    let v = maxc;
    let diff = maxc - minc;

    let s = if maxc == 0.0 { 0.0 } else { diff / maxc };

    let mut h = 0.0;
    if (maxc - minc).abs() > f64::EPSILON {
        h = if maxc == r {
            let mut hue = 60.0 * (g - b) / diff;
            if g < b {
                hue += 360.0;
            }
            hue
        } else if maxc == g {
            60.0 * (b - r) / diff + 120.0
        } else {
            60.0 * (r - g) / diff + 240.0
        };
    }

    HsvPixel {
        h: (h / 2.0).round().clamp(0.0, 180.0) as u8,
        s: (s * 255.0).round().clamp(0.0, 255.0) as u8,
        v: (v * 255.0).round().clamp(0.0, 255.0) as u8,
    }
}

fn check_color_range(h: u8, s: u8, v: u8, range: &HsvRange) -> bool {
    let h_match = if range.h_lower <= range.h_upper {
        h >= range.h_lower && h <= range.h_upper
    } else {
        h >= range.h_lower || h <= range.h_upper
    };
    let s_match = s >= range.s_lower && s <= range.s_upper;
    let v_match = v >= range.v_lower && v <= range.v_upper;
    h_match && s_match && v_match
}

fn create_mask(hsv: &[HsvPixel], width: u32, height: u32, range: &HsvRange) -> Vec<u8> {
    let len = (width * height) as usize;
    let mut mask = vec![0u8; len];
    for (idx, px) in hsv.iter().enumerate().take(len) {
        if check_color_range(px.h, px.s, px.v, range) {
            mask[idx] = 255;
        }
    }
    mask
}

fn dilate(mask: &[u8], width: u32, height: u32, kernel_size: u32, iterations: u32) -> Vec<u8> {
    let w = width as i32;
    let h = height as i32;
    let pad = (kernel_size / 2) as i32;
    let len = (width * height) as usize;
    let mut result = mask.to_vec();

    for _ in 0..iterations {
        let mut temp = vec![0u8; len];
        for y in 0..h {
            for x in 0..w {
                'kernel: for ky in (y - pad).max(0)..=(y + pad).min(h - 1) {
                    for kx in (x - pad).max(0)..=(x + pad).min(w - 1) {
                        if result[(ky * w + kx) as usize] > 0 {
                            temp[(y * w + x) as usize] = 255;
                            break 'kernel;
                        }
                    }
                }
            }
        }
        result = temp;
    }
    result
}

fn calculate_contour_area(contour: &[(i32, i32)]) -> f64 {
    if contour.len() < 3 {
        return 0.0;
    }
    let mut sum = 0.0;
    let n = contour.len();
    for i in 0..n {
        let (x0, y0) = contour[i];
        let (x1, y1) = contour[(i + 1) % n];
        sum += (x0 as f64) * (y1 as f64) - (x1 as f64) * (y0 as f64);
    }
    0.5 * sum.abs()
}

fn get_bounding_rect(contour: &[(i32, i32)]) -> (i32, i32, i32, i32) {
    let mut min_x = i32::MAX;
    let mut min_y = i32::MAX;
    let mut max_x = i32::MIN;
    let mut max_y = i32::MIN;
    for &(x, y) in contour {
        min_x = min_x.min(x);
        min_y = min_y.min(y);
        max_x = max_x.max(x);
        max_y = max_y.max(y);
    }
    (
        min_x,
        min_y,
        max_x - min_x + 1,
        max_y - min_y + 1,
    )
}

fn trace_contour(
    mask: &[u8],
    width: i32,
    height: i32,
    visited: &mut [bool],
    start_y: i32,
    start_x: i32,
) -> Vec<(i32, i32)> {
    let mut contour = Vec::new();
    let mut stack = vec![(start_y, start_x)];

    while let Some((y, x)) = stack.pop() {
        let idx = (y * width + x) as usize;
        if visited[idx] {
            continue;
        }
        visited[idx] = true;

        let mut is_edge = false;
        for &(dy, dx) in &DIRECTIONS {
            let ny = y + dy;
            let nx = x + dx;
            if ny >= 0 && ny < height && nx >= 0 && nx < width {
                if mask[(ny * width + nx) as usize] == 0 {
                    is_edge = true;
                    break;
                }
            } else {
                is_edge = true;
                break;
            }
        }

        if is_edge {
            contour.push((x, y));
            for &(dy, dx) in &DIRECTIONS {
                let ny = y + dy;
                let nx = x + dx;
                if ny >= 0
                    && ny < height
                    && nx >= 0
                    && nx < width
                    && mask[(ny * width + nx) as usize] > 0
                    && !visited[(ny * width + nx) as usize]
                {
                    stack.push((ny, nx));
                }
            }
        }
    }
    contour
}

fn find_contours(
    mask: &[u8],
    width: u32,
    height: u32,
    min_area: f64,
) -> Vec<(Vec<(i32, i32)>, f64)> {
    let w = width as i32;
    let h = height as i32;
    let len = (width * height) as usize;
    let mut visited = vec![false; len];
    let mut contours = Vec::new();

    for y in 0..h {
        for x in 0..w {
            let idx = (y * w + x) as usize;
            if mask[idx] > 0 && !visited[idx] {
                let contour = trace_contour(mask, w, h, &mut visited, y, x);
                if !contour.is_empty() {
                    let area = calculate_contour_area(&contour);
                    if area >= min_area {
                        contours.push((contour, area));
                    }
                }
            }
        }
    }
    contours
}

#[cfg(test)]
mod tests {
    use super::*;
    use capture_core::{HsvRange, PixelFormat};

    #[test]
    fn pure_red_hsv() {
        let px = rgb_to_hsv_pixel(255, 0, 0);
        assert_eq!(px.h, 0);
        assert_eq!(px.s, 255);
        assert_eq!(px.v, 255);
    }

    #[test]
    fn detect_solid_color_block() {
        let mut buffer = CpuBuffer::empty(Size2D::new(320, 320), PixelFormat::Bgra8Unorm);
        for y in 100..200 {
            for x in 100..200 {
                let idx = y as usize * buffer.stride as usize + x as usize * 4;
                buffer.data[idx] = 0;
                buffer.data[idx + 1] = 0;
                buffer.data[idx + 2] = 255;
                buffer.data[idx + 3] = 255;
            }
        }

        let settings = HsvSettings {
            enabled: true,
            range: HsvRange {
                lower: [0, 100, 100],
                upper: [10, 255, 255],
            },
            morphology_kernel_size: 3,
            min_contour_area: 20,
        };

        let stats = detect_hsv_on_buffer(&buffer, &settings, Size2D::new(320, 320));
        assert!(!stats.objects.is_empty());
        assert!(stats.hit_count > 0);
    }
}
