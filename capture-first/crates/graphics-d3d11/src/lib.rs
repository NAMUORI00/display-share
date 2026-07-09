//! D3D11 texture handle — GPU crop / staging readback.
//! Kept out of `capture-core` so contracts stay platform-agnostic.

use std::fmt::Debug;
use std::sync::Arc;

use capture_core::{
    CpuBuffer, GpuTexture, GraphicsBackend, PixelFormat, RectI, Size2D, VisionError,
};
use parking_lot::Mutex;
use windows::Win32::Graphics::Direct3D11::{
    D3D11_BOX, D3D11_CPU_ACCESS_READ, D3D11_MAP_READ, D3D11_MAPPED_SUBRESOURCE,
    D3D11_TEXTURE2D_DESC, D3D11_USAGE_DEFAULT, D3D11_USAGE_STAGING, ID3D11Device,
    ID3D11DeviceContext, ID3D11Texture2D,
};
use windows::Win32::Graphics::Dxgi::Common::{
    DXGI_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SAMPLE_DESC,
};
use windows::core::Interface;

/// Reused staging textures to avoid E_OUTOFMEMORY (0x8007000E) from per-frame CreateTexture2D.
static STAGING_CACHE: Mutex<Vec<CachedStaging>> = Mutex::new(Vec::new());

struct CachedStaging {
    device: ID3D11Device,
    width: u32,
    height: u32,
    format: DXGI_FORMAT,
    texture: ID3D11Texture2D,
}

#[derive(Clone)]
pub struct D3d11TextureHandle {
    texture: ID3D11Texture2D,
    size: Size2D,
    pixel_format: PixelFormat,
}

impl D3d11TextureHandle {
    #[must_use]
    pub fn new(texture: ID3D11Texture2D, size: Size2D, pixel_format: PixelFormat) -> Self {
        Self {
            texture,
            size,
            pixel_format,
        }
    }

    #[must_use]
    pub fn raw(&self) -> &ID3D11Texture2D {
        &self.texture
    }

    #[must_use]
    pub fn into_shared(self) -> Arc<dyn GpuTexture> {
        Arc::new(self)
    }

    #[must_use]
    pub fn dxgi_format(pixel_format: PixelFormat) -> DXGI_FORMAT {
        match pixel_format {
            PixelFormat::Bgra8Unorm => DXGI_FORMAT_B8G8R8A8_UNORM,
            PixelFormat::Rgba8Unorm => DXGI_FORMAT_R8G8B8A8_UNORM,
        }
    }

    pub fn crop_owned(&self, roi: RectI) -> Result<Self, VisionError> {
        if roi.x < 0
            || roi.y < 0
            || roi.x as u32 + roi.width > self.size.width
            || roi.y as u32 + roi.height > self.size.height
        {
            return Err(VisionError::InvalidRoi);
        }

        let device = self
            .device()
            .map_err(|err| VisionError::Other(err.to_string()))?;
        let context = self
            .immediate_context()
            .map_err(|err| VisionError::Other(err.to_string()))?;
        let src_desc = self.desc();
        let desc = D3D11_TEXTURE2D_DESC {
            Width: roi.width,
            Height: roi.height,
            MipLevels: 1,
            ArraySize: 1,
            Format: src_desc.Format,
            SampleDesc: DXGI_SAMPLE_DESC {
                Count: 1,
                Quality: 0,
            },
            Usage: D3D11_USAGE_DEFAULT,
            BindFlags: src_desc.BindFlags,
            CPUAccessFlags: 0,
            MiscFlags: 0,
        };

        let mut texture = None;
        unsafe {
            device
                .CreateTexture2D(&desc, None, Some(&mut texture))
                .map_err(map_oom)?;
        }
        let texture = texture.ok_or_else(|| {
            VisionError::Other("CreateTexture2D returned null texture".to_owned())
        })?;

        let src_box = D3D11_BOX {
            left: roi.x as u32,
            top: roi.y as u32,
            front: 0,
            right: roi.x as u32 + roi.width,
            bottom: roi.y as u32 + roi.height,
            back: 1,
        };
        unsafe {
            context.CopySubresourceRegion(
                &texture,
                0,
                0,
                0,
                0,
                &self.texture,
                0,
                Some(&src_box),
            );
        }

        Ok(Self::new(
            texture,
            Size2D::new(roi.width, roi.height),
            self.pixel_format,
        ))
    }

    pub fn readback_owned(&self) -> Result<CpuBuffer, VisionError> {
        let device = self
            .device()
            .map_err(|err| VisionError::Other(err.to_string()))?;
        let context = self
            .immediate_context()
            .map_err(|err| VisionError::Other(err.to_string()))?;
        let src_desc = self.desc();
        let staging = acquire_staging(
            &device,
            src_desc.Width,
            src_desc.Height,
            src_desc.Format,
        )?;

        let mut mapped = D3D11_MAPPED_SUBRESOURCE::default();
        unsafe {
            context.CopyResource(&staging, &self.texture);
            context
                .Map(&staging, 0, D3D11_MAP_READ, 0, Some(&mut mapped))
                .map_err(map_oom)?;
        }

        let mut buffer = CpuBuffer::empty(self.size, self.pixel_format);
        let row_size = self.size.width as usize * 4;
        let source = unsafe {
            std::slice::from_raw_parts(
                mapped.pData.cast::<u8>(),
                mapped.RowPitch as usize * self.size.height as usize,
            )
        };

        for row in 0..self.size.height as usize {
            let src_offset = row * mapped.RowPitch as usize;
            let dst_offset = row * buffer.stride as usize;
            buffer.data[dst_offset..dst_offset + row_size]
                .copy_from_slice(&source[src_offset..src_offset + row_size]);
        }

        unsafe {
            context.Unmap(&staging, 0);
        }
        release_staging(device, src_desc.Width, src_desc.Height, src_desc.Format, staging);

        Ok(buffer)
    }

    fn desc(&self) -> D3D11_TEXTURE2D_DESC {
        let mut desc = D3D11_TEXTURE2D_DESC::default();
        unsafe {
            self.texture.GetDesc(&mut desc);
        }
        desc
    }

    fn device(&self) -> windows::core::Result<ID3D11Device> {
        unsafe { self.texture.GetDevice() }
    }

    fn immediate_context(&self) -> windows::core::Result<ID3D11DeviceContext> {
        let device = self.device()?;
        unsafe { device.GetImmediateContext() }
    }
}

fn acquire_staging(
    device: &ID3D11Device,
    width: u32,
    height: u32,
    format: DXGI_FORMAT,
) -> Result<ID3D11Texture2D, VisionError> {
    {
        let mut cache = STAGING_CACHE.lock();
        if let Some(index) = cache.iter().position(|entry| {
            entry.width == width
                && entry.height == height
                && entry.format == format
                && same_device(&entry.device, device)
        }) {
            return Ok(cache.swap_remove(index).texture);
        }
    }

    let desc = D3D11_TEXTURE2D_DESC {
        Width: width,
        Height: height,
        MipLevels: 1,
        ArraySize: 1,
        Format: format,
        SampleDesc: DXGI_SAMPLE_DESC {
            Count: 1,
            Quality: 0,
        },
        Usage: D3D11_USAGE_STAGING,
        BindFlags: 0,
        CPUAccessFlags: D3D11_CPU_ACCESS_READ.0 as u32,
        MiscFlags: 0,
    };
    let mut texture = None;
    unsafe {
        device
            .CreateTexture2D(&desc, None, Some(&mut texture))
            .map_err(map_oom)?;
    }
    texture.ok_or_else(|| {
        VisionError::Other("CreateTexture2D returned null staging texture".to_owned())
    })
}

fn release_staging(
    device: ID3D11Device,
    width: u32,
    height: u32,
    format: DXGI_FORMAT,
    texture: ID3D11Texture2D,
) {
    let mut cache = STAGING_CACHE.lock();
    // Keep a small pool per size to avoid unbounded growth.
    let same_size = cache
        .iter()
        .filter(|e| e.width == width && e.height == height && e.format == format)
        .count();
    if same_size < 2 {
        cache.push(CachedStaging {
            device,
            width,
            height,
            format,
            texture,
        });
    }
}

fn same_device(a: &ID3D11Device, b: &ID3D11Device) -> bool {
    // COM identity: same underlying object.
    a.as_raw() == b.as_raw()
}

fn map_oom(err: windows::core::Error) -> VisionError {
    let message = err.to_string();
    if message.contains("0x8007000E") || message.contains("E_OUTOFMEMORY") {
        VisionError::Other(format!(
            "GPU/system out of memory during D3D11 texture op (0x8007000E). \
             Close Monitor if open, lower target FPS, or reduce frame buffer. Detail: {message}"
        ))
    } else {
        VisionError::Other(message)
    }
}

impl Debug for D3d11TextureHandle {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("D3d11TextureHandle")
            .field("size", &self.size)
            .field("pixel_format", &self.pixel_format)
            .finish()
    }
}

impl GpuTexture for D3d11TextureHandle {
    fn backend(&self) -> GraphicsBackend {
        GraphicsBackend::D3d11
    }

    fn size(&self) -> Size2D {
        self.size
    }

    fn pixel_format(&self) -> PixelFormat {
        self.pixel_format
    }

    fn crop(&self, roi: RectI) -> Result<Arc<dyn GpuTexture>, VisionError> {
        Ok(Arc::new(self.crop_owned(roi)?))
    }

    fn readback(&self) -> Result<CpuBuffer, VisionError> {
        self.readback_owned()
    }
}
