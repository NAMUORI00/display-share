use capture_core::{CaptureError, Size2D};
use windows::Win32::Graphics::Dxgi::{
    CreateDXGIFactory1, DXGI_ADAPTER_DESC1, DXGI_ERROR_NOT_FOUND, IDXGIAdapter1, IDXGIFactory1,
};
use windows_capture::monitor::Monitor;

#[derive(Debug, Clone)]
pub struct WgcMonitorInfo {
    pub native_index: usize,
    pub device_name: String,
    pub device_string: String,
    pub friendly_name: Option<String>,
    pub primary: bool,
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

pub fn enumerate_wgc_monitors() -> Result<Vec<WgcMonitorInfo>, CaptureError> {
    let primary_device_name = Monitor::primary()
        .ok()
        .and_then(|monitor| monitor.device_name().ok());
    let monitors =
        Monitor::enumerate().map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
    let mut result = Vec::with_capacity(monitors.len());

    for (index, monitor) in monitors.iter().enumerate() {
        let device_name = monitor
            .device_name()
            .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
        let device_string = monitor
            .device_string()
            .unwrap_or_else(|_| "Unknown Adapter".to_owned());
        let friendly_name = monitor.name().ok();
        let width = monitor.width().unwrap_or(0);
        let height = monitor.height().unwrap_or(0);
        let refresh_hz = monitor.refresh_rate().unwrap_or(60);
        result.push(WgcMonitorInfo {
            native_index: index,
            primary: primary_device_name
                .as_ref()
                .is_some_and(|primary| primary == &device_name),
            device_name,
            device_string,
            friendly_name,
            size: Size2D::new(width, height),
            refresh_hz,
        });
    }

    Ok(result)
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
