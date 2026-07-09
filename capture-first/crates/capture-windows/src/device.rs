//! Shared D3D11 device factory — adapter-scoped so WGC/DXGI/vision share one device when possible.

use std::sync::Arc;

use capture_core::CaptureError;
use parking_lot::Mutex;
use windows::Win32::Foundation::HMODULE;
use windows::Win32::Graphics::Direct3D::{
    D3D_DRIVER_TYPE_UNKNOWN, D3D_FEATURE_LEVEL, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1,
};
use windows::Win32::Graphics::Direct3D11::{
    D3D11_CREATE_DEVICE_BGRA_SUPPORT, D3D11_SDK_VERSION, D3D11CreateDevice, ID3D11Device,
    ID3D11DeviceContext,
};
use windows::Win32::Graphics::Dxgi::{CreateDXGIFactory1, IDXGIAdapter, IDXGIAdapter1, IDXGIFactory1};
use windows::core::Interface;

#[derive(Clone)]
pub struct SharedD3d11Device {
    pub device: ID3D11Device,
    pub context: ID3D11DeviceContext,
    pub adapter_index: u32,
}

impl SharedD3d11Device {
    pub fn for_adapter(adapter_index: u32) -> Result<Self, CaptureError> {
        let factory: IDXGIFactory1 = unsafe { CreateDXGIFactory1() }
            .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
        let adapter = unsafe { factory.EnumAdapters1(adapter_index) }
            .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
        let (device, context) = create_device_for_adapter(&adapter)?;
        Ok(Self {
            device,
            context,
            adapter_index,
        })
    }

    pub fn for_default_adapter() -> Result<Self, CaptureError> {
        Self::for_adapter(0)
    }
}

/// Process-wide cache keyed by adapter index (optional reuse across sessions).
static DEVICE_CACHE: Mutex<Vec<(u32, Arc<SharedD3d11Device>)>> = Mutex::new(Vec::new());

pub fn shared_device_for_adapter(adapter_index: u32) -> Result<Arc<SharedD3d11Device>, CaptureError> {
    let mut cache = DEVICE_CACHE.lock();
    if let Some((_, existing)) = cache.iter().find(|(idx, _)| *idx == adapter_index) {
        return Ok(existing.clone());
    }
    let device = Arc::new(SharedD3d11Device::for_adapter(adapter_index)?);
    cache.push((adapter_index, device.clone()));
    Ok(device)
}

pub fn create_device_for_adapter(
    adapter: &IDXGIAdapter1,
) -> Result<(ID3D11Device, ID3D11DeviceContext), CaptureError> {
    let feature_levels = [D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0];
    let adapter_base: IDXGIAdapter = adapter
        .cast()
        .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
    let mut device = None;
    let mut feature_level = D3D_FEATURE_LEVEL::default();
    let mut context = None;
    unsafe {
        D3D11CreateDevice(
            &adapter_base,
            D3D_DRIVER_TYPE_UNKNOWN,
            HMODULE::default(),
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            Some(&feature_levels),
            D3D11_SDK_VERSION,
            Some(&mut device),
            Some(&mut feature_level),
            Some(&mut context),
        )
    }
    .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;

    if feature_level.0 < D3D_FEATURE_LEVEL_11_0.0 {
        return Err(CaptureError::BackendUnavailable(
            "DXGI duplication requires D3D11 feature level 11.0+".to_owned(),
        ));
    }

    let device = device.ok_or_else(|| {
        CaptureError::BackendUnavailable("D3D11CreateDevice returned null device".to_owned())
    })?;
    let context = context.ok_or_else(|| {
        CaptureError::BackendUnavailable("D3D11CreateDevice returned null context".to_owned())
    })?;

    Ok((device, context))
}
