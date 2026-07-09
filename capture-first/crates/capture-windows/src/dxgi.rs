use std::collections::HashMap;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

use capture_core::{
    CaptureBackend, CaptureBackendKind, CaptureError, CaptureFrame, CaptureOptions, CapturePacket,
    CaptureSession, CaptureStats, CaptureTarget, CaptureTargetKind, PixelFormat, RectI, Size2D,
};
use crossbeam_channel::{Receiver, Sender, TryRecvError, TrySendError, bounded};
use graphics_d3d11::D3d11TextureHandle;
use parking_lot::Mutex;
use windows::Win32::Foundation::RECT;
use windows::Win32::Graphics::Direct3D11::{
    D3D11_TEXTURE2D_DESC, D3D11_USAGE_DEFAULT, ID3D11Device, ID3D11DeviceContext, ID3D11Texture2D,
};
use windows::Win32::Graphics::Dxgi::{
    DXGI_ERROR_SESSION_DISCONNECTED, DXGI_ERROR_UNSUPPORTED, DXGI_ERROR_WAIT_TIMEOUT,
    DXGI_OUTDUPL_FRAME_INFO, IDXGIOutput1, IDXGIOutputDuplication, IDXGIResource,
};
use windows::core::Interface;

use crate::device::{create_device_for_adapter, shared_device_for_adapter};
use crate::enumerate::{WgcMonitorInfo, enumerate_dxgi_outputs, enumerate_wgc_monitors, utf16_to_string};

#[derive(Default)]
pub struct DxgiDuplicationBackend;

impl DxgiDuplicationBackend {
    pub fn enumerate_targets_with_wgc(
        &self,
        monitors: &[WgcMonitorInfo],
    ) -> Result<Vec<CaptureTarget>, CaptureError> {
        let dxgi_outputs = enumerate_dxgi_outputs()?;
        let monitor_by_device = monitors
            .iter()
            .map(|monitor| (monitor.device_name.clone(), monitor))
            .collect::<HashMap<_, _>>();

        let targets = dxgi_outputs
            .into_iter()
            .enumerate()
            .map(|(index, output)| {
                let matched_monitor = monitor_by_device.get(&output.device_name).copied();
                let name = matched_monitor
                    .and_then(|monitor| monitor.friendly_name.clone())
                    .unwrap_or_else(|| format!("Display {}", index + 1));
                let primary = matched_monitor.is_some_and(|monitor| monitor.primary);
                let refresh_hz = matched_monitor.map_or(60, |monitor| monitor.refresh_hz);
                let native_index = matched_monitor.map_or(index, |monitor| monitor.native_index);
                let mut available_backends = vec![CaptureBackendKind::DxgiDuplication];
                let preferred_backend = if matched_monitor.is_some() {
                    available_backends.insert(0, CaptureBackendKind::WindowsGraphicsCapture);
                    CaptureBackendKind::WindowsGraphicsCapture
                } else {
                    CaptureBackendKind::DxgiDuplication
                };

                CaptureTarget {
                    id: format!("display-{index}"),
                    name,
                    kind: CaptureTargetKind::Display,
                    primary,
                    size: output.size,
                    refresh_hz,
                    native_index,
                    device_name: Some(output.device_name.clone()),
                    adapter_name: Some(output.adapter_name.clone()),
                    adapter_index: Some(output.adapter_index),
                    output_index: Some(output.output_index),
                    available_backends,
                    preferred_backend,
                }
            })
            .collect();

        Ok(targets)
    }
}

impl CaptureBackend for DxgiDuplicationBackend {
    fn enumerate_targets(&self) -> Result<Vec<CaptureTarget>, CaptureError> {
        self.enumerate_targets_with_wgc(&enumerate_wgc_monitors()?)
    }

    fn start(
        &self,
        target: &CaptureTarget,
        options: CaptureOptions,
    ) -> Result<Box<dyn CaptureSession>, CaptureError> {
        if target.kind != CaptureTargetKind::Display {
            return Err(CaptureError::UnsupportedTarget(target.id.clone()));
        }

        let adapter_index = target.adapter_index.ok_or_else(|| {
            CaptureError::BackendUnavailable("DXGI adapter metadata is missing".to_owned())
        })?;
        let output_index = target.output_index.ok_or_else(|| {
            CaptureError::BackendUnavailable("DXGI output metadata is missing".to_owned())
        })?;
        let (sender, receiver) = bounded(options.buffer_depth.max(1));
        let total_frames = Arc::new(AtomicU64::new(0));
        let dropped_frames = Arc::new(AtomicU64::new(0));
        let stop_requested = Arc::new(AtomicBool::new(false));
        let last_error = Arc::new(Mutex::new(None));

        // Warm shared device cache for this adapter (crop/preprocess reuse).
        let _ = shared_device_for_adapter(adapter_index);

        let thread_total_frames = total_frames.clone();
        let thread_dropped_frames = dropped_frames.clone();
        let thread_stop_requested = stop_requested.clone();
        let thread_last_error = last_error.clone();
        let target_device_name = target.device_name.clone();
        let handle = thread::Builder::new()
            .name(format!("dxgi-capture-{adapter_index}-{output_index}"))
            .spawn(move || {
                let result = run_dxgi_capture_loop(
                    adapter_index,
                    output_index,
                    target_device_name.as_deref(),
                    options,
                    sender,
                    thread_total_frames,
                    thread_dropped_frames,
                    thread_stop_requested,
                );
                if let Err(err) = result {
                    *thread_last_error.lock() = Some(err.to_string());
                }
            })
            .map_err(|err| CaptureError::Other(err.to_string()))?;

        Ok(Box::new(DxgiCaptureSession {
            receiver,
            total_frames,
            dropped_frames,
            stop_requested,
            worker: Mutex::new(Some(handle)),
            last_error,
        }))
    }

    fn supports_zero_copy(&self) -> bool {
        true
    }
}

struct DxgiCaptureSession {
    receiver: Receiver<CapturePacket>,
    total_frames: Arc<AtomicU64>,
    dropped_frames: Arc<AtomicU64>,
    stop_requested: Arc<AtomicBool>,
    worker: Mutex<Option<JoinHandle<()>>>,
    last_error: Arc<Mutex<Option<String>>>,
}

impl CaptureSession for DxgiCaptureSession {
    fn try_recv(&self) -> Result<Option<CapturePacket>, CaptureError> {
        match self.receiver.try_recv() {
            Ok(packet) => Ok(Some(packet)),
            Err(TryRecvError::Empty) => Ok(None),
            Err(TryRecvError::Disconnected) => Err(self
                .last_error
                .lock()
                .clone()
                .map_or(CaptureError::SessionStopped, CaptureError::Other)),
        }
    }

    fn stop(&self) -> Result<(), CaptureError> {
        self.stop_requested.store(true, Ordering::Relaxed);
        if let Some(worker) = self.worker.lock().take() {
            worker
                .join()
                .map_err(|_| CaptureError::Other("DXGI capture worker panicked".to_owned()))?;
        }
        Ok(())
    }

    fn stats(&self) -> CaptureStats {
        CaptureStats {
            total_frames: self.total_frames.load(Ordering::Relaxed),
            dropped_frames: self.dropped_frames.load(Ordering::Relaxed),
            last_latency: None,
        }
    }

    fn backend_kind(&self) -> CaptureBackendKind {
        CaptureBackendKind::DxgiDuplication
    }
}

impl Drop for DxgiCaptureSession {
    fn drop(&mut self) {
        self.stop_requested.store(true, Ordering::Relaxed);
        if let Some(worker) = self.worker.get_mut().take() {
            let _ = worker.join();
        }
    }
}

fn run_dxgi_capture_loop(
    adapter_index: u32,
    output_index: u32,
    expected_device_name: Option<&str>,
    options: CaptureOptions,
    sender: Sender<CapturePacket>,
    total_frames: Arc<AtomicU64>,
    dropped_frames: Arc<AtomicU64>,
    stop_requested: Arc<AtomicBool>,
) -> Result<(), CaptureError> {
    let mut session = open_duplication_session(adapter_index, output_index, expected_device_name)?;
    let min_frame_interval = Duration::from_secs_f64(1.0 / options.target_fps.max(1) as f64);
    let mut last_sent_at = Instant::now()
        .checked_sub(min_frame_interval)
        .unwrap_or_else(Instant::now);

    while !stop_requested.load(Ordering::Relaxed) {
        match acquire_next_packet(&session) {
            Ok(Some(packet)) => {
                if last_sent_at.elapsed() < min_frame_interval {
                    continue;
                }

                last_sent_at = Instant::now();
                let frame_number = total_frames.fetch_add(1, Ordering::Relaxed) + 1;
                let packet = CapturePacket {
                    frame_number,
                    ..packet
                };
                match sender.try_send(packet) {
                    Ok(()) => {}
                    Err(TrySendError::Full(_)) | Err(TrySendError::Disconnected(_)) => {
                        dropped_frames.fetch_add(1, Ordering::Relaxed);
                    }
                }
            }
            Ok(None) => {
                thread::sleep(Duration::from_millis(2));
            }
            Err(err) => {
                if should_reinitialize(&err) {
                    session = open_duplication_session(
                        adapter_index,
                        output_index,
                        expected_device_name,
                    )?;
                    continue;
                }
                return Err(err);
            }
        }
    }

    Ok(())
}

fn should_reinitialize(err: &CaptureError) -> bool {
    match err {
        CaptureError::Other(message) => {
            message.contains("0x887A0026")
                || message.contains("0x887A0005")
                || message.contains("0x887A0028")
        }
        _ => false,
    }
}

struct DxgiDuplicationSession {
    duplication: IDXGIOutputDuplication,
    device: ID3D11Device,
    context: ID3D11DeviceContext,
    pixel_format: PixelFormat,
}

fn open_duplication_session(
    adapter_index: u32,
    output_index: u32,
    expected_device_name: Option<&str>,
) -> Result<DxgiDuplicationSession, CaptureError> {
    use windows::Win32::Graphics::Dxgi::{CreateDXGIFactory1, IDXGIFactory1};

    let factory: IDXGIFactory1 = unsafe { CreateDXGIFactory1() }
        .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
    let adapter = unsafe { factory.EnumAdapters1(adapter_index) }
        .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
    let output = unsafe { adapter.EnumOutputs(output_index) }
        .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
    let output_desc = unsafe { output.GetDesc() }
        .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
    let actual_device_name = utf16_to_string(&output_desc.DeviceName);
    if let Some(expected_device_name) = expected_device_name {
        if actual_device_name != expected_device_name {
            return Err(CaptureError::BackendUnavailable(format!(
                "DXGI output changed from {expected_device_name} to {actual_device_name}"
            )));
        }
    }

    let output1: IDXGIOutput1 = output
        .cast()
        .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;

    // Prefer shared device cache so vision crop stays on the same adapter device.
    let (device, context) = if let Ok(shared) = shared_device_for_adapter(adapter_index) {
        (shared.device.clone(), shared.context.clone())
    } else {
        create_device_for_adapter(&adapter)?
    };

    let duplication = unsafe { output1.DuplicateOutput(&device) }.map_err(map_duplication_error)?;

    Ok(DxgiDuplicationSession {
        duplication,
        device,
        context,
        pixel_format: PixelFormat::Bgra8Unorm,
    })
}

fn acquire_next_packet(
    session: &DxgiDuplicationSession,
) -> Result<Option<CapturePacket>, CaptureError> {
    let mut frame_info = DXGI_OUTDUPL_FRAME_INFO::default();
    let mut resource: Option<IDXGIResource> = None;
    match unsafe {
        session
            .duplication
            .AcquireNextFrame(16, &mut frame_info, &mut resource)
    } {
        Ok(()) => {}
        Err(err) if err.code() == DXGI_ERROR_WAIT_TIMEOUT => return Ok(None),
        Err(err) => return Err(CaptureError::Other(err.to_string())),
    }

    let packet_result = (|| -> Result<Option<CapturePacket>, CaptureError> {
        if frame_info.AccumulatedFrames == 0 {
            return Ok(None);
        }

        let resource = resource.ok_or_else(|| {
            CaptureError::Other("DXGI duplication returned no desktop resource".to_owned())
        })?;
        let texture: ID3D11Texture2D = resource
            .cast()
            .map_err(|err| CaptureError::Other(err.to_string()))?;
        let owned_texture = clone_texture(&session.device, &session.context, &texture)?;
        let dirty_regions = read_dirty_regions(&session.duplication).unwrap_or_default();
        let mut desc = D3D11_TEXTURE2D_DESC::default();
        unsafe {
            owned_texture.GetDesc(&mut desc);
        }
        let shared = D3d11TextureHandle::new(
            owned_texture,
            Size2D::new(desc.Width, desc.Height),
            session.pixel_format,
        )
        .into_shared();
        Ok(Some(CapturePacket::new(
            0,
            CaptureFrame::D3d11(shared),
            dirty_regions,
        )))
    })();

    let _ = unsafe { session.duplication.ReleaseFrame() };
    packet_result
}

fn clone_texture(
    device: &ID3D11Device,
    context: &ID3D11DeviceContext,
    source: &ID3D11Texture2D,
) -> Result<ID3D11Texture2D, CaptureError> {
    let mut desc = D3D11_TEXTURE2D_DESC::default();
    unsafe {
        source.GetDesc(&mut desc);
    }
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    let mut texture = None;
    unsafe {
        device
            .CreateTexture2D(&desc, None, Some(&mut texture))
            .map_err(|err| CaptureError::Other(err.to_string()))?;
    }
    let texture = texture
        .ok_or_else(|| CaptureError::Other("CreateTexture2D returned null owned texture".to_owned()))?;
    unsafe {
        context.CopyResource(&texture, source);
    }
    Ok(texture)
}

fn read_dirty_regions(duplication: &IDXGIOutputDuplication) -> Result<Vec<RectI>, CaptureError> {
    let mut required_size = 0u32;
    let _ = unsafe { duplication.GetFrameDirtyRects(0, std::ptr::null_mut(), &mut required_size) };
    if required_size == 0 {
        return Ok(Vec::new());
    }

    let rect_count = required_size as usize / std::mem::size_of::<RECT>();
    let mut rects = vec![RECT::default(); rect_count];
    unsafe {
        duplication
            .GetFrameDirtyRects(required_size, rects.as_mut_ptr(), &mut required_size)
            .map_err(|err| CaptureError::Other(err.to_string()))?;
    }

    Ok(rects
        .into_iter()
        .map(|rect| {
            RectI::new(
                rect.left,
                rect.top,
                (rect.right - rect.left).max(0) as u32,
                (rect.bottom - rect.top).max(0) as u32,
            )
        })
        .collect())
}

fn map_duplication_error(err: windows::core::Error) -> CaptureError {
    if matches!(
        err.code(),
        DXGI_ERROR_UNSUPPORTED | DXGI_ERROR_SESSION_DISCONNECTED
    ) {
        CaptureError::BackendUnavailable(err.to_string())
    } else {
        CaptureError::Other(err.to_string())
    }
}