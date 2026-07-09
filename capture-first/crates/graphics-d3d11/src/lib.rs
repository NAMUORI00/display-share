//! D3D11 texture handle — GPU crop / staging readback.
//! Kept out of `capture-core` so contracts stay platform-agnostic.

use std::fmt::Debug;
use std::sync::Arc;

use capture_core::{
    CpuBuffer, GpuTexture, GraphicsBackend, PixelFormat, RectI, Size2D, VisionError,
};
use windows::Win32::Graphics::Direct3D11::{
    D3D11_BOX, D3D11_CPU_ACCESS_READ, D3D11_MAP_READ, D3D11_MAPPED_SUBRESOURCE,
    D3D11_TEXTURE2D_DESC, D3D11_USAGE_DEFAULT, D3D11_USAGE_STAGING, ID3D11Device,
    ID3D11DeviceContext, ID3D11Texture2D,
};
use windows::Win32::Graphics::Dxgi::Common::{
    DXGI_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SAMPLE_DESC,
};

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
            MiscFlags: src_desc.MiscFlags,
        };

        let mut texture = None;
        unsafe {
            device
                .CreateTexture2D(&desc, None, Some(&mut texture))
                .map_err(|err| VisionError::Other(err.to_string()))?;
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
        let desc = D3D11_TEXTURE2D_DESC {
            Width: src_desc.Width,
            Height: src_desc.Height,
            MipLevels: 1,
            ArraySize: 1,
            Format: src_desc.Format,
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
        let mut mapped = D3D11_MAPPED_SUBRESOURCE::default();
        unsafe {
            device
                .CreateTexture2D(&desc, None, Some(&mut texture))
                .map_err(|err| VisionError::Other(err.to_string()))?;
        }
        let staging = texture.ok_or_else(|| {
            VisionError::Other("CreateTexture2D returned null staging texture".to_owned())
        })?;

        unsafe {
            context.CopyResource(&staging, &self.texture);
            context
                .Map(&staging, 0, D3D11_MAP_READ, 0, Some(&mut mapped))
                .map_err(|err| VisionError::Other(err.to_string()))?;
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
