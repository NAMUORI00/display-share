//! GPU-oriented vision helpers. GPU resize/shader preprocess is Track 2;
//! `prepare_for_inference` currently does honest CPU preprocess (resize + NCHW).

mod hsv_detect;

use capture_core::{
    CaptureFrame, CaptureRoi, CpuBuffer, CpuNchwTensor, HsvMaskStats, HsvSettings, PixelFormat,
    ProcessedFrame, RoiSpec, Size2D, TensorInputHandle, VisionError, VisionPipeline,
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

    /// HSV mask stats with contour-based bounding boxes (HsvColorPicker-style).
    pub fn detect_hsv_stats(
        &self,
        buffer: &CpuBuffer,
        settings: &HsvSettings,
        frame_size: Size2D,
    ) -> Result<HsvMaskStats, VisionError> {
        if !settings.enabled {
            return Ok(HsvMaskStats::default());
        }
        ensure_canonical_format(buffer.pixel_format)?;
        Ok(hsv_detect::detect_hsv_on_buffer(buffer, settings, frame_size))
    }

    pub fn crop_capture_roi(
        &self,
        frame: &CaptureFrame,
        roi: CaptureRoi,
    ) -> Result<ProcessedFrame, VisionError> {
        self.crop_roi(frame, roi.into())
    }

    /// Downscale a CPU buffer so the long edge is at most `max_long_edge`.
    /// Returns `(preview_buffer, uniform_scale)` where scale = preview / original.
    pub fn downscale_for_preview(
        &self,
        buffer: &CpuBuffer,
        max_long_edge: u32,
    ) -> Result<(CpuBuffer, f32), VisionError> {
        downscale_cpu_buffer(buffer, max_long_edge)
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

/// Public helper for pipeline full-frame preview downscale.
pub fn downscale_cpu_buffer(
    buffer: &CpuBuffer,
    max_long_edge: u32,
) -> Result<(CpuBuffer, f32), VisionError> {
    ensure_canonical_format(buffer.pixel_format)?;
    let long_edge = buffer.width.max(buffer.height);
    if long_edge == 0 {
        return Ok((buffer.clone(), 1.0));
    }
    if long_edge <= max_long_edge {
        return Ok((buffer.clone(), 1.0));
    }

    let scale = max_long_edge as f32 / long_edge as f32;
    let out_w = ((buffer.width as f32 * scale).round() as u32).max(1);
    let out_h = ((buffer.height as f32 * scale).round() as u32).max(1);
    let actual_scale = out_w as f32 / buffer.width as f32;

    let rgb = to_rgb_image(buffer)?;
    let resized = image::imageops::resize(&rgb, out_w, out_h, FilterType::Triangle);
    let mut out = CpuBuffer::empty(Size2D::new(out_w, out_h), buffer.pixel_format);
    for (x, y, pixel) in resized.enumerate_pixels() {
        let dst = y as usize * out.stride as usize + x as usize * 4;
        match buffer.pixel_format {
            PixelFormat::Bgra8Unorm => {
                out.data[dst] = pixel[2];
                out.data[dst + 1] = pixel[1];
                out.data[dst + 2] = pixel[0];
                out.data[dst + 3] = 255;
            }
            PixelFormat::Rgba8Unorm => {
                out.data[dst] = pixel[0];
                out.data[dst + 1] = pixel[1];
                out.data[dst + 2] = pixel[2];
                out.data[dst + 3] = 255;
            }
        }
    }
    Ok((out, actual_scale))
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
