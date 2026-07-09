use capture_core::{CaptureError, Size2D};
use windows::Win32::Graphics::Dxgi::{
    CreateDXGIFactory1, DXGI_ADAPTER_DESC1, DXGI_ERROR_NOT_FOUND, IDXGIAdapter1, IDXGIFactory1,
};
use windows::Win32::Graphics::Gdi::{
    DEVMODEW, DISPLAY_DEVICE_ACTIVE, DISPLAY_DEVICE_PRIMARY_DEVICE, DISPLAY_DEVICEW,
    ENUM_CURRENT_SETTINGS, EnumDisplayDevicesW, EnumDisplaySettingsW,
};
use windows::core::PCWSTR;

/// Monitor metadata for target labels (Win32 display APIs).
#[derive(Debug, Clone)]
pub struct DisplayMonitorInfo {
    pub native_index: usize,
    pub device_name: String,
    #[allow(dead_code)]
    pub device_string: String,
    pub friendly_name: Option<String>,
    pub primary: bool,
    #[allow(dead_code)]
    pub size: Size2D,
    pub refresh_hz: u32,
}

#[derive(Debug, Clone)]
pub struct DxgiOutputInfo {
    pub adapter_index: u32,
    pub output_index: u32,
    pub adapter_name: String,
    pub device_name: String,
    pub size: Size2D,
}

/// Enumerate attached displays via Win32.
pub fn enumerate_display_monitors() -> Result<Vec<DisplayMonitorInfo>, CaptureError> {
    let mut result = Vec::new();
    let mut adapter_index = 0u32;

    loop {
        let mut device = DISPLAY_DEVICEW {
            cb: std::mem::size_of::<DISPLAY_DEVICEW>() as u32,
            ..Default::default()
        };
        let ok = unsafe { EnumDisplayDevicesW(PCWSTR::null(), adapter_index, &mut device, 0) };
        if !ok.as_bool() {
            break;
        }

        let state = device.StateFlags;
        if (state & DISPLAY_DEVICE_ACTIVE).0 == 0 {
            adapter_index += 1;
            continue;
        }

        let device_name = utf16_to_string(&device.DeviceName);
        let device_string = utf16_to_string(&device.DeviceString);
        let primary = (state & DISPLAY_DEVICE_PRIMARY_DEVICE).0 != 0;

        let mut monitor = DISPLAY_DEVICEW {
            cb: std::mem::size_of::<DISPLAY_DEVICEW>() as u32,
            ..Default::default()
        };
        let friendly_name = {
            let device_name_wide: Vec<u16> = device_name
                .encode_utf16()
                .chain(std::iter::once(0))
                .collect();
            let has_monitor = unsafe {
                EnumDisplayDevicesW(PCWSTR(device_name_wide.as_ptr()), 0, &mut monitor, 0)
            };
            if has_monitor.as_bool() {
                let name = utf16_to_string(&monitor.DeviceString);
                if name.is_empty() { None } else { Some(name) }
            } else {
                None
            }
        };

        let (width, height, refresh_hz) = display_mode_for_device(&device_name);

        result.push(DisplayMonitorInfo {
            native_index: result.len(),
            device_name,
            device_string,
            friendly_name,
            primary,
            size: Size2D::new(width, height),
            refresh_hz,
        });
        adapter_index += 1;
    }

    Ok(result)
}

fn display_mode_for_device(device_name: &str) -> (u32, u32, u32) {
    let device_name_wide: Vec<u16> = device_name
        .encode_utf16()
        .chain(std::iter::once(0))
        .collect();
    let mut mode = DEVMODEW {
        dmSize: std::mem::size_of::<DEVMODEW>() as u16,
        ..Default::default()
    };
    let ok = unsafe {
        EnumDisplaySettingsW(
            PCWSTR(device_name_wide.as_ptr()),
            ENUM_CURRENT_SETTINGS,
            &mut mode,
        )
    };
    if !ok.as_bool() {
        return (0, 0, 60);
    }
    let width = mode.dmPelsWidth;
    let height = mode.dmPelsHeight;
    let refresh = mode.dmDisplayFrequency;
    let refresh_hz = if refresh == 0 || refresh == 1 {
        60
    } else {
        refresh
    };
    (width, height, refresh_hz)
}

pub fn enumerate_dxgi_outputs() -> Result<Vec<DxgiOutputInfo>, CaptureError> {
    let factory: IDXGIFactory1 = unsafe { CreateDXGIFactory1() }
        .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
    let mut outputs = Vec::new();
    let mut adapter_index = 0u32;

    loop {
        let adapter = match unsafe { factory.EnumAdapters1(adapter_index) } {
            Ok(adapter) => adapter,
            Err(err) if err.code() == DXGI_ERROR_NOT_FOUND => break,
            Err(err) => return Err(CaptureError::BackendUnavailable(err.to_string())),
        };
        let adapter_desc = unsafe { adapter.GetDesc1() }
            .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;

        enumerate_outputs_for_adapter(adapter_index, &adapter, &adapter_desc, &mut outputs)?;
        adapter_index += 1;
    }

    Ok(outputs)
}

fn enumerate_outputs_for_adapter(
    adapter_index: u32,
    adapter: &IDXGIAdapter1,
    adapter_desc: &DXGI_ADAPTER_DESC1,
    outputs: &mut Vec<DxgiOutputInfo>,
) -> Result<(), CaptureError> {
    let mut output_index = 0u32;
    let adapter_name = utf16_to_string(&adapter_desc.Description);

    loop {
        let output = match unsafe { adapter.EnumOutputs(output_index) } {
            Ok(output) => output,
            Err(err) if err.code() == DXGI_ERROR_NOT_FOUND => break,
            Err(err) => return Err(CaptureError::BackendUnavailable(err.to_string())),
        };
        let output_desc = unsafe { output.GetDesc() }
            .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
        let width = (output_desc.DesktopCoordinates.right - output_desc.DesktopCoordinates.left)
            .max(0) as u32;
        let height = (output_desc.DesktopCoordinates.bottom - output_desc.DesktopCoordinates.top)
            .max(0) as u32;
        outputs.push(DxgiOutputInfo {
            adapter_index,
            output_index,
            adapter_name: adapter_name.clone(),
            device_name: utf16_to_string(&output_desc.DeviceName),
            size: Size2D::new(width, height),
        });
        output_index += 1;
    }

    Ok(())
}

pub fn utf16_to_string(buffer: &[u16]) -> String {
    let end = buffer
        .iter()
        .position(|value| *value == 0)
        .unwrap_or(buffer.len());
    String::from_utf16_lossy(&buffer[..end])
}
