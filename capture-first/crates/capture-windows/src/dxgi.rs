use std::collections::HashMap;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

use capture_core::{
    AnalysisFrame, CaptureBackend, CaptureBackendKind, CaptureError, CaptureFrame, CaptureOptions,
    CapturePacket, CaptureRoi, CaptureSession, CaptureStats, CaptureTarget, CaptureTargetKind,
    PixelFormat, PreviewFrame, RectI, Size2D,
};
use crossbeam_channel::{Receiver, Sender, TryRecvError, TrySendError, bounded};
use graphics_d3d11::D3d11TextureHandle;
use parking_lot::Mutex;
use vision_gpu::downscale_cpu_buffer;
use windows::Win32::Foundation::RECT;
use windows::Win32::Graphics::Direct3D11::{
    D3D11_TEXTURE2D_DESC, D3D11_USAGE_DEFAULT, ID3D11Device, ID3D11DeviceContext, ID3D11Texture2D,
};
use windows::Win32::Graphics::Dxgi::{
    DXGI_ERROR_ACCESS_LOST, DXGI_ERROR_DEVICE_REMOVED, DXGI_ERROR_DEVICE_RESET,
    DXGI_ERROR_SESSION_DISCONNECTED, DXGI_ERROR_UNSUPPORTED, DXGI_ERROR_WAIT_TIMEOUT,
    DXGI_OUTDUPL_FRAME_INFO, IDXGIOutput1, IDXGIOutputDuplication, IDXGIResource,
};
use windows::core::Interface;

use crate::device::{create_device_for_adapter, shared_device_for_adapter};
use crate::enumerate::{
    DisplayMonitorInfo, DxgiOutputInfo, enumerate_display_monitors, enumerate_dxgi_outputs,
    utf16_to_string,
};

const REINIT_BACKOFF_START: Duration = Duration::from_millis(50);
const REINIT_BACKOFF_CAP: Duration = Duration::from_secs(2);
const REINIT_FAILURE_LIMIT: u32 = 40;
/// Monitor preview cadence — independent of capture target FPS.
const PREVIEW_MIN_INTERVAL: Duration = Duration::from_millis(50);
/// Near-native View quality (1080p long edge). Analysis path stays at 640 separately.
const PREVIEW_MAX_LONG_EDGE: u32 = 1920;
/// YOLO/HSV analysis cadence (~15 FPS). Uses full-frame downscale, not center crop.
const ANALYSIS_MIN_INTERVAL: Duration = Duration::from_millis(66);
const ANALYSIS_MAX_LONG_EDGE: u32 = 640;
const PREVIEW_CHANNEL_DEPTH: usize = 1;
const ANALYSIS_CHANNEL_DEPTH: usize = 1;

#[derive(Default)]
pub struct DxgiDuplicationBackend;

impl DxgiDuplicationBackend {
    pub fn enumerate_targets_with_monitors(
        &self,
        monitors: &[DisplayMonitorInfo],
    ) -> Result<Vec<CaptureTarget>, CaptureError> {
        let dxgi_outputs = enumerate_dxgi_outputs()?;
        if dxgi_outputs.is_empty() {
            return Err(CaptureError::BackendUnavailable(
                "no DXGI outputs found for Desktop Duplication".to_owned(),
            ));
        }

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
                    // DXGI is the only active backend in this crate.
                    available_backends: vec![CaptureBackendKind::DxgiDuplication],
                    preferred_backend: CaptureBackendKind::DxgiDuplication,
                }
            })
            .collect();

        Ok(targets)
    }
}

impl CaptureBackend for DxgiDuplicationBackend {
    fn enumerate_targets(&self) -> Result<Vec<CaptureTarget>, CaptureError> {
        self.enumerate_targets_with_monitors(&enumerate_display_monitors().unwrap_or_default())
    }

    fn start(
        &self,
        target: &CaptureTarget,
        options: CaptureOptions,
    ) -> Result<Box<dyn CaptureSession>, CaptureError> {
        if target.kind != CaptureTargetKind::Display {
            return Err(CaptureError::UnsupportedTarget(target.id.clone()));
        }

        let options = options.normalize_for_display_capture();
        let binding = resolve_output_binding(target)?;
        let (sender, receiver) = bounded(options.buffer_depth.max(1));
        let (preview_tx, preview_rx) = bounded(PREVIEW_CHANNEL_DEPTH);
        let (analysis_tx, analysis_rx) = bounded(ANALYSIS_CHANNEL_DEPTH);
        let total_frames = Arc::new(AtomicU64::new(0));
        let dropped_frames = Arc::new(AtomicU64::new(0));
        let stop_requested = Arc::new(AtomicBool::new(false));
        let preview_enabled = Arc::new(AtomicBool::new(false));
        let analysis_enabled = Arc::new(AtomicBool::new(false));
        let last_error = Arc::new(Mutex::new(None));

        // Warm shared device cache for this adapter (crop/preprocess reuse).
        let _ = shared_device_for_adapter(binding.adapter_index);

        let thread_total_frames = total_frames.clone();
        let thread_dropped_frames = dropped_frames.clone();
        let thread_stop_requested = stop_requested.clone();
        let thread_preview_enabled = preview_enabled.clone();
        let thread_analysis_enabled = analysis_enabled.clone();
        let thread_last_error = last_error.clone();
        let handle = thread::spawn(move || {
            let result = run_dxgi_capture_loop(
                binding,
                options,
                sender,
                preview_tx,
                analysis_tx,
                thread_total_frames,
                thread_dropped_frames,
                thread_stop_requested,
                thread_preview_enabled,
                thread_analysis_enabled,
            );
            if let Err(err) = result {
                *thread_last_error.lock() = Some(err.to_string());
            }
        });

        Ok(Box::new(DxgiCaptureSession {
            receiver,
            preview_rx,
            analysis_rx,
            total_frames,
            dropped_frames,
            stop_requested,
            preview_enabled,
            analysis_enabled,
            worker: Mutex::new(Some(handle)),
            last_error,
        }))
    }

    fn supports_zero_copy(&self) -> bool {
        true
    }
}

#[derive(Debug, Clone)]
struct OutputBinding {
    adapter_index: u32,
    output_index: u32,
    device_name: Option<String>,
    adapter_name: Option<String>,
}

/// Re-enumerate DXGI outputs and bind the selected monitor to the correct adapter/output.
fn resolve_output_binding(target: &CaptureTarget) -> Result<OutputBinding, CaptureError> {
    let outputs = enumerate_dxgi_outputs()?;
    if outputs.is_empty() {
        return Err(CaptureError::BackendUnavailable(format!(
            "no DXGI outputs while starting {} (device={:?})",
            target.name, target.device_name
        )));
    }

    if let Some(device_name) = target.device_name.as_deref()
        && let Some(matched) = outputs.iter().find(|o| o.device_name == device_name)
    {
        return Ok(binding_from_output(matched));
    }

    if let (Some(adapter_index), Some(output_index)) = (target.adapter_index, target.output_index)
        && let Some(matched) = outputs
            .iter()
            .find(|o| o.adapter_index == adapter_index && o.output_index == output_index)
    {
        return Ok(binding_from_output(matched));
    }

    Err(CaptureError::BackendUnavailable(format!(
        "DXGI output not found for {} (adapter={:?}, output={:?}, device={:?})",
        target.name, target.adapter_index, target.output_index, target.device_name
    )))
}

fn binding_from_output(output: &DxgiOutputInfo) -> OutputBinding {
    OutputBinding {
        adapter_index: output.adapter_index,
        output_index: output.output_index,
        device_name: Some(output.device_name.clone()),
        adapter_name: Some(output.adapter_name.clone()),
    }
}

/// Re-resolve binding by device_name after mode changes (indices may shift).
fn refresh_binding(binding: &OutputBinding) -> Result<OutputBinding, CaptureError> {
    let outputs = enumerate_dxgi_outputs()?;
    if let Some(device_name) = binding.device_name.as_deref()
        && let Some(matched) = outputs.iter().find(|o| o.device_name == device_name)
    {
        return Ok(binding_from_output(matched));
    }
    if let Some(matched) = outputs.iter().find(|o| {
        o.adapter_index == binding.adapter_index && o.output_index == binding.output_index
    }) {
        return Ok(binding_from_output(matched));
    }
    Err(CaptureError::BackendUnavailable(format!(
        "DXGI output lost during recovery (adapter={}, output={}, device={:?}, gpu={:?})",
        binding.adapter_index, binding.output_index, binding.device_name, binding.adapter_name
    )))
}

struct DxgiCaptureSession {
    receiver: Receiver<CapturePacket>,
    preview_rx: Receiver<PreviewFrame>,
    analysis_rx: Receiver<AnalysisFrame>,
    total_frames: Arc<AtomicU64>,
    dropped_frames: Arc<AtomicU64>,
    stop_requested: Arc<AtomicBool>,
    preview_enabled: Arc<AtomicBool>,
    analysis_enabled: Arc<AtomicBool>,
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

    fn set_preview_enabled(&self, enabled: bool) {
        self.preview_enabled.store(enabled, Ordering::Relaxed);
    }

    fn try_recv_preview(&self) -> Result<Option<PreviewFrame>, CaptureError> {
        match self.preview_rx.try_recv() {
            Ok(frame) => Ok(Some(frame)),
            Err(TryRecvError::Empty) => Ok(None),
            Err(TryRecvError::Disconnected) => Ok(None),
        }
    }

    fn set_analysis_enabled(&self, enabled: bool) {
        self.analysis_enabled.store(enabled, Ordering::Relaxed);
    }

    fn try_recv_analysis(&self) -> Result<Option<AnalysisFrame>, CaptureError> {
        match self.analysis_rx.try_recv() {
            Ok(frame) => Ok(Some(frame)),
            Err(TryRecvError::Empty) => Ok(None),
            Err(TryRecvError::Disconnected) => Ok(None),
        }
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

#[allow(clippy::too_many_arguments)]
fn run_dxgi_capture_loop(
    mut binding: OutputBinding,
    options: CaptureOptions,
    sender: Sender<CapturePacket>,
    preview_tx: Sender<PreviewFrame>,
    analysis_tx: Sender<AnalysisFrame>,
    total_frames: Arc<AtomicU64>,
    dropped_frames: Arc<AtomicU64>,
    stop_requested: Arc<AtomicBool>,
    preview_enabled: Arc<AtomicBool>,
    analysis_enabled: Arc<AtomicBool>,
) -> Result<(), CaptureError> {
    let mut session = open_duplication_session(&binding)?;
    let min_frame_interval = Duration::from_secs_f64(1.0 / options.target_fps.max(1) as f64);
    let mut last_sent_at = Instant::now()
        .checked_sub(min_frame_interval)
        .unwrap_or_else(Instant::now);
    let mut last_preview_at = Instant::now()
        .checked_sub(PREVIEW_MIN_INTERVAL)
        .unwrap_or_else(Instant::now);
    let mut last_analysis_at = Instant::now()
        .checked_sub(ANALYSIS_MIN_INTERVAL)
        .unwrap_or_else(Instant::now);
    let mut backoff = REINIT_BACKOFF_START;
    let mut consecutive_reinit_failures = 0u32;

    while !stop_requested.load(Ordering::Relaxed) {
        match acquire_next_packet(&mut session) {
            Ok(Some(packet)) => {
                consecutive_reinit_failures = 0;
                backoff = REINIT_BACKOFF_START;
                if last_sent_at.elapsed() < min_frame_interval {
                    continue;
                }

                last_sent_at = Instant::now();
                let frame_number = total_frames.fetch_add(1, Ordering::Relaxed) + 1;
                let packet = CapturePacket {
                    frame_number,
                    ..packet
                };

                // Same-thread D3D11 readback only — never from UI/vision worker.
                let want_preview = preview_enabled.load(Ordering::Relaxed)
                    && last_preview_at.elapsed() >= PREVIEW_MIN_INTERVAL;
                let want_analysis = analysis_enabled.load(Ordering::Relaxed)
                    && last_analysis_at.elapsed() >= ANALYSIS_MIN_INTERVAL;
                if (want_preview || want_analysis)
                    && emit_cpu_side_channels(
                        &packet,
                        want_preview,
                        want_analysis,
                        &preview_tx,
                        &analysis_tx,
                    )
                    .is_ok()
                {
                    if want_preview {
                        last_preview_at = Instant::now();
                    }
                    if want_analysis {
                        last_analysis_at = Instant::now();
                    }
                }

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
                if !should_reinitialize(&err) {
                    return Err(err);
                }

                // Do not kill the worker on ACCESS_LOST — backoff and reopen.
                loop {
                    if stop_requested.load(Ordering::Relaxed) {
                        return Ok(());
                    }
                    thread::sleep(backoff);
                    match refresh_binding(&binding).and_then(|next| {
                        binding = next;
                        open_duplication_session(&binding)
                    }) {
                        Ok(reopened) => {
                            session = reopened;
                            consecutive_reinit_failures = 0;
                            backoff = REINIT_BACKOFF_START;
                            break;
                        }
                        Err(reopen_err) => {
                            consecutive_reinit_failures += 1;
                            if consecutive_reinit_failures >= REINIT_FAILURE_LIMIT {
                                return Err(CaptureError::Other(format!(
                                    "DXGI session recovery failed after {REINIT_FAILURE_LIMIT} attempts: {reopen_err} (original: {err})"
                                )));
                            }
                            backoff = (backoff * 2).min(REINIT_BACKOFF_CAP);
                        }
                    }
                }
            }
        }
    }

    Ok(())
}

/// Readback + downscale / ROI crop on the DXGI capture thread only.
fn emit_cpu_side_channels(
    packet: &CapturePacket,
    want_preview: bool,
    want_analysis: bool,
    preview_tx: &Sender<PreviewFrame>,
    analysis_tx: &Sender<AnalysisFrame>,
) -> Result<(), CaptureError> {
    let capture_size = packet.frame.size();
    if capture_size.width == 0 || capture_size.height == 0 {
        return Ok(());
    }

    // Prefer a single full-frame readback when both preview and analysis need CPU.
    let full_cpu = if want_preview || want_analysis {
        match &packet.frame {
            CaptureFrame::D3d11(texture) => texture
                .readback()
                .map_err(|err| CaptureError::Other(err.to_string()))?,
            CaptureFrame::Cpu(buffer) => buffer.clone(),
        }
    } else {
        return Ok(());
    };

    if want_preview {
        let (scaled, scale) = downscale_cpu_buffer(&full_cpu, PREVIEW_MAX_LONG_EDGE)
            .map_err(|err| CaptureError::Other(err.to_string()))?;
        let preview = PreviewFrame {
            frame_number: packet.frame_number,
            buffer: scaled,
            scale,
            capture_size,
        };
        // Depth-1 channel: if UI is behind, drop this preview (next tick retries).
        let _ = preview_tx.try_send(preview);
    }

    if want_analysis {
        // Full-frame analysis so objects anywhere on the monitor are visible in overlays.
        let capture_roi = CaptureRoi {
            rect: RectI::new(0, 0, capture_size.width, capture_size.height),
        };
        let (roi_buffer, _) = downscale_cpu_buffer(&full_cpu, ANALYSIS_MAX_LONG_EDGE)
            .map_err(|err| CaptureError::Other(err.to_string()))?;
        let analysis = AnalysisFrame {
            frame_number: packet.frame_number,
            capture_size,
            capture_roi,
            roi_buffer,
        };
        let _ = analysis_tx.try_send(analysis);
    }

    Ok(())
}

fn should_reinitialize(err: &CaptureError) -> bool {
    match err {
        CaptureError::Other(message) | CaptureError::BackendUnavailable(message) => {
            message.contains("0x887A0026") // DXGI_ERROR_ACCESS_LOST
                || message.contains("0x887A0005") // DXGI_ERROR_DEVICE_REMOVED
                || message.contains("0x887A0007") // DXGI_ERROR_DEVICE_RESET
                || message.contains("0x887A0028") // DXGI_ERROR_SESSION_DISCONNECTED
                || message.contains("ACCESS_LOST")
                || message.contains("DEVICE_REMOVED")
                || message.contains("DEVICE_RESET")
                || message.contains("SESSION_DISCONNECTED")
                || message.contains("output changed")
                || message.contains("output lost")
        }
        _ => false,
    }
}

struct DxgiDuplicationSession {
    duplication: IDXGIOutputDuplication,
    device: ID3D11Device,
    context: ID3D11DeviceContext,
    pixel_format: PixelFormat,
    /// Pool of reusable GPU copies. A slot is reused only when no CapturePacket still holds it
    /// (`Arc::strong_count == 1`), preventing overwrite races and unbounded CreateTexture2D.
    frame_pool: Vec<Arc<D3d11TextureHandle>>,
    frame_pool_size: (u32, u32),
}

fn open_duplication_session(
    binding: &OutputBinding,
) -> Result<DxgiDuplicationSession, CaptureError> {
    use windows::Win32::Graphics::Dxgi::{CreateDXGIFactory1, IDXGIFactory1};

    let factory: IDXGIFactory1 = unsafe { CreateDXGIFactory1() }
        .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
    let adapter = unsafe { factory.EnumAdapters1(binding.adapter_index) }.map_err(|err| {
        CaptureError::BackendUnavailable(format!(
            "EnumAdapters1({}) failed for device={:?}: {err}",
            binding.adapter_index, binding.device_name
        ))
    })?;
    let output = unsafe { adapter.EnumOutputs(binding.output_index) }.map_err(|err| {
        CaptureError::BackendUnavailable(format!(
            "EnumOutputs({}) on adapter {} failed for device={:?}: {err}",
            binding.output_index, binding.adapter_index, binding.device_name
        ))
    })?;
    let output_desc = unsafe { output.GetDesc() }
        .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
    let actual_device_name = utf16_to_string(&output_desc.DeviceName);
    if let Some(expected_device_name) = binding.device_name.as_deref()
        && actual_device_name != expected_device_name
    {
        return Err(CaptureError::BackendUnavailable(format!(
            "DXGI output changed from {expected_device_name} to {actual_device_name}"
        )));
    }

    let output1: IDXGIOutput1 = output
        .cast()
        .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;

    // Prefer shared device cache so vision crop stays on the same adapter device.
    let (device, context) = if let Ok(shared) = shared_device_for_adapter(binding.adapter_index) {
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
        frame_pool: Vec::new(),
        frame_pool_size: (0, 0),
    })
}

fn acquire_next_packet(
    session: &mut DxgiDuplicationSession,
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
        Err(err) if is_reinit_hresult(err.code()) => {
            return Err(CaptureError::Other(format!(
                "DXGI AcquireNextFrame ACCESS_LOST/device event: {err}"
            )));
        }
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
        let Some(shared) = try_copy_into_pooled_texture(session, &texture)? else {
            // All pool slots still held by in-flight packets — skip without allocating.
            return Ok(None);
        };
        let dirty_regions = read_dirty_regions(&session.duplication).unwrap_or_default();
        Ok(Some(CapturePacket::new(
            0,
            CaptureFrame::D3d11(shared),
            dirty_regions,
        )))
    })();

    let _ = unsafe { session.duplication.ReleaseFrame() };
    packet_result
}

fn is_reinit_hresult(code: windows::core::HRESULT) -> bool {
    code == DXGI_ERROR_ACCESS_LOST
        || code == DXGI_ERROR_DEVICE_REMOVED
        || code == DXGI_ERROR_DEVICE_RESET
        || code == DXGI_ERROR_SESSION_DISCONNECTED
}

/// Copy desktop resource into a pooled texture. `Ok(None)` = pool busy, skip frame.
fn try_copy_into_pooled_texture(
    session: &mut DxgiDuplicationSession,
    source: &ID3D11Texture2D,
) -> Result<Option<Arc<dyn capture_core::GpuTexture>>, CaptureError> {
    const POOL_CAP: usize = 3;

    let mut desc = D3D11_TEXTURE2D_DESC::default();
    unsafe {
        source.GetDesc(&mut desc);
    }
    let size = (desc.Width, desc.Height);
    if session.frame_pool_size != size {
        session.frame_pool.clear();
        session.frame_pool_size = size;
    }

    // Prefer a pool entry that is no longer referenced by any in-flight packet.
    let free_index = session
        .frame_pool
        .iter()
        .position(|tex| Arc::strong_count(tex) == 1);

    let handle = if let Some(index) = free_index {
        session.frame_pool[index].clone()
    } else if session.frame_pool.len() < POOL_CAP {
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        let mut texture = None;
        unsafe {
            session
                .device
                .CreateTexture2D(&desc, None, Some(&mut texture))
                .map_err(map_create_texture_error)?;
        }
        let texture = texture.ok_or_else(|| {
            CaptureError::Other("CreateTexture2D returned null owned texture".to_owned())
        })?;
        let handle = Arc::new(D3d11TextureHandle::new(
            texture,
            Size2D::new(size.0, size.1),
            session.pixel_format,
        ));
        session.frame_pool.push(handle.clone());
        handle
    } else {
        return Ok(None);
    };

    unsafe {
        session.context.CopyResource(handle.raw(), source);
    }
    Ok(Some(handle))
}

fn map_create_texture_error(err: windows::core::Error) -> CaptureError {
    let message = err.to_string();
    if message.contains("0x8007000E") || message.contains("E_OUTOFMEMORY") {
        CaptureError::Other(format!(
            "GPU/system out of memory creating capture texture (0x8007000E). \
             Lower target FPS / frame buffer, or close Monitor. Detail: {message}"
        ))
    } else {
        CaptureError::Other(message)
    }
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn should_reinitialize_detects_access_lost_codes() {
        assert!(should_reinitialize(&CaptureError::Other(
            "0x887A0026 ACCESS_LOST".to_owned()
        )));
        assert!(should_reinitialize(&CaptureError::Other(
            "DXGI_ERROR_DEVICE_REMOVED 0x887A0005".to_owned()
        )));
        assert!(!should_reinitialize(&CaptureError::SessionStopped));
    }
}
