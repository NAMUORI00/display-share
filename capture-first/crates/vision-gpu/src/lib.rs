//! GPU-oriented vision helpers. GPU resize/shader preprocess is Track 2;
//! `prepare_for_inference` currently does honest CPU preprocess (resize + NCHW).

use capture_core::{
    CaptureFrame, CaptureRoi, CpuBuffer, CpuNchwTensor, HsvMaskStats, HsvSettings, PixelFormat,
    ProcessedFrame, RectI, RoiSpec, Size2D, TensorInputHandle, VisionError, VisionPipeline,
};
use image::{RgbImage, imageops::FilterType};

/// Canonical capture pixel format on Windows.
pub const CANONICAL_PIXEL_FORMAT: PixelFormat = PixelFormat::Bgra8Unorm;

#[derive(Debug, Default)]
pub struct GpuVisionPipeline;

impl VisionPipeline for GpuVisionPipeline {
    fn crop_roi(&self, frame: &CaptureFrame, roi: RoiSpec) -> Result<ProcessedFrame, VisionError> {
        ensure_canonical_format(frame.pixel_format())?;
        match frame {
            CaptureFrame::D3d11(texture) => {
                let cropped = texture.crop(roi.rect)?;
                Ok(ProcessedFrame {
                    source: CaptureFrame::D3d11(cropped),
                    roi,
                    logical_zero_copy: true,
                })
            }
            CaptureFrame::Cpu(buffer) => {
                let cropped = crop_cpu_buffer(buffer, roi)?;
                Ok(ProcessedFrame {
                    source: CaptureFrame::Cpu(cropped),
                    roi,
                    logical_zero_copy: false,
                })
            }
        }
    }

    fn prepare_for_inference(
        &self,
        frame: &ProcessedFrame,
        target_size: Size2D,
    ) -> Result<TensorInputHandle, VisionError> {
        // Honest path: CPU resize + NCHW normalize. GPU shader path is Track 2 —
        // until then we read back and mark logical_zero_copy = false.
        let buffer = self.to_cpu_buffer(&frame.source)?;
        let tensor = cpu_preprocess_nchw(&buffer, target_size)?;
        let mut processed = frame.clone();
        processed.source = CaptureFrame::Cpu(buffer);
        processed.logical_zero_copy = false;
        Ok(TensorInputHandle {
            source: processed,
            target_size,
            normalized: true,
            cpu_nchw: Some(tensor),
        })
    }
}

impl GpuVisionPipeline {
    pub fn to_cpu_buffer(&self, frame: &CaptureFrame) -> Result<CpuBuffer, VisionError> {
        ensure_canonical_format(frame.pixel_format())?;
        match frame {
            CaptureFrame::Cpu(buffer) => Ok(buffer.clone()),
            CaptureFrame::D3d11(texture) => texture.readback(),
        }
    }

    /// HSV mask stats (hit count + union bbox). Not per-pixel DetectionResult spam.
    pub fn detect_hsv_stats(
        &self,
        buffer: &CpuBuffer,
        settings: &HsvSettings,
    ) -> Result<HsvMaskStats, VisionError> {
        if !settings.enabled {
            return Ok(HsvMaskStats::default());
        }
        ensure_canonical_format(buffer.pixel_format)?;

        let mut hit_count = 0usize;
        let mut bbox_union: Option<RectI> = None;
        let min_area = settings.min_contour_area.max(1);

        for y in 0..buffer.height {
            for x in 0..buffer.width {
                let idx = y as usize * buffer.stride as usize + x as usize * 4;
                let pixel = &buffer.data[idx..idx + 4];
                let (r, g, b) = match buffer.pixel_format {
                    PixelFormat::Bgra8Unorm => (pixel[2], pixel[1], pixel[0]),
                    PixelFormat::Rgba8Unorm => (pixel[0], pixel[1], pixel[2]),
                };
                let hsv = rgb_to_hsv(r, g, b);
                if within_range(hsv, &settings.range.lower, &settings.range.upper) {
                    hit_count += 1;
                    let cell = RectI::new(x as i32, y as i32, 1, 1);
                    bbox_union = Some(match bbox_union {
                        Some(existing) => existing.union(cell),
                        None => cell,
                    });
                }
            }
        }

        // min_contour_area: drop tiny masks (morphology deferred; area gate applied).
        if hit_count < min_area as usize {
            return Ok(HsvMaskStats::default());
        }

        Ok(HsvMaskStats {
            hit_count,
            bbox_union,
        })
    }

    pub fn crop_capture_roi(
        &self,
        frame: &CaptureFrame,
        roi: CaptureRoi,
    ) -> Result<ProcessedFrame, VisionError> {
        self.crop_roi(frame, roi.into())
    }
}

fn ensure_canonical_format(actual: PixelFormat) -> Result<(), VisionError> {
    if actual != CANONICAL_PIXEL_FORMAT {
        return Err(VisionError::PixelFormatMismatch {
            expected: CANONICAL_PIXEL_FORMAT,
            actual,
        });
    }
    Ok(())
}

pub fn cpu_preprocess_nchw(
    buffer: &CpuBuffer,
    target_size: Size2D,
) -> Result<CpuNchwTensor, VisionError> {
    let rgb = to_rgb_image(buffer)?;
    let resized = image::imageops::resize(
        &rgb,
        target_size.width,
        target_size.height,
        FilterType::Triangle,
    );
    let (width, height) = resized.dimensions();
    let mut data = vec![0.0f32; (3 * width * height) as usize];
    let plane = (width * height) as usize;
    for (x, y, pixel) in resized.enumerate_pixels() {
        let idx = (y * width + x) as usize;
        data[idx] = pixel[0] as f32 / 255.0;
        data[plane + idx] = pixel[1] as f32 / 255.0;
        data[2 * plane + idx] = pixel[2] as f32 / 255.0;
    }
    Ok(CpuNchwTensor {
        width,
        height,
        data,
    })
}

fn to_rgb_image(buffer: &CpuBuffer) -> Result<RgbImage, VisionError> {
    let mut rgb = vec![0u8; (buffer.width * buffer.height * 3) as usize];
    for y in 0..buffer.height as usize {
        for x in 0..buffer.width as usize {
            let src = y * buffer.stride as usize + x * 4;
            let dst = (y * buffer.width as usize + x) * 3;
            let px = &buffer.data[src..src + 4];
            match buffer.pixel_format {
                PixelFormat::Bgra8Unorm => {
                    rgb[dst] = px[2];
                    rgb[dst + 1] = px[1];
                    rgb[dst + 2] = px[0];
                }
                PixelFormat::Rgba8Unorm => {
                    rgb[dst] = px[0];
                    rgb[dst + 1] = px[1];
                    rgb[dst + 2] = px[2];
                }
            }
        }
    }

    RgbImage::from_raw(buffer.width, buffer.height, rgb)
        .ok_or_else(|| VisionError::Other("failed to build RGB image".to_owned()))
}

fn crop_cpu_buffer(buffer: &CpuBuffer, roi: RoiSpec) -> Result<CpuBuffer, VisionError> {
    let rect = roi.rect;
    if rect.x < 0
        || rect.y < 0
        || rect.x as u32 + rect.width > buffer.width
        || rect.y as u32 + rect.height > buffer.height
    {
        return Err(VisionError::InvalidRoi);
    }

    let mut out = CpuBuffer::empty(Size2D::new(rect.width, rect.height), buffer.pixel_format);
    let bytes_per_pixel = 4usize;
    for row in 0..rect.height as usize {
        let src_offset =
            (rect.y as usize + row) * buffer.stride as usize + rect.x as usize * bytes_per_pixel;
        let dst_offset = row * out.stride as usize;
        let row_len = rect.width as usize * bytes_per_pixel;
        out.data[dst_offset..dst_offset + row_len]
            .copy_from_slice(&buffer.data[src_offset..src_offset + row_len]);
    }

    Ok(out)
}

fn rgb_to_hsv(r: u8, g: u8, b: u8) -> [u8; 3] {
    let rf = r as f32 / 255.0;
    let gf = g as f32 / 255.0;
    let bf = b as f32 / 255.0;

    let max = rf.max(gf).max(bf);
    let min = rf.min(gf).min(bf);
    let delta = max - min;

    let h = if delta == 0.0 {
        0.0
    } else if max == rf {
        60.0 * (((gf - bf) / delta) % 6.0)
    } else if max == gf {
        60.0 * (((bf - rf) / delta) + 2.0)
    } else {
        60.0 * (((rf - gf) / delta) + 4.0)
    };
    let h = if h < 0.0 { h + 360.0 } else { h };
    let s = if max == 0.0 { 0.0 } else { delta / max };
    let v = max;

    [
        ((h / 2.0).round() as i32).clamp(0, 179) as u8,
        (s * 255.0).round() as u8,
        (v * 255.0).round() as u8,
    ]
}

fn within_range(hsv: [u8; 3], lower: &[u8; 3], upper: &[u8; 3]) -> bool {
    hsv[0] >= lower[0]
        && hsv[0] <= upper[0]
        && hsv[1] >= lower[1]
        && hsv[1] <= upper[1]
        && hsv[2] >= lower[2]
        && hsv[2] <= upper[2]
}
