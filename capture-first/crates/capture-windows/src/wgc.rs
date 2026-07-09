use std::sync::Arc;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::Duration;

use capture_core::{
    CaptureBackend, CaptureBackendKind, CaptureError, CaptureFrame, CaptureOptions, CapturePacket,
    CaptureSession, CaptureStats, CaptureTarget, CaptureTargetKind, PixelFormat, RectI, Size2D,
};
use crossbeam_channel::{Receiver, Sender, TryRecvError, TrySendError, bounded};
use graphics_d3d11::D3d11TextureHandle;
use parking_lot::Mutex;
use windows_capture::capture::{CaptureControl, Context, GraphicsCaptureApiHandler};
use windows_capture::frame::Frame;
use windows_capture::graphics_capture_api::InternalCaptureControl;
use windows_capture::monitor::Monitor;
use windows_capture::settings::{
    ColorFormat, CursorCaptureSettings, DirtyRegionSettings, DrawBorderSettings,
    MinimumUpdateIntervalSettings, SecondaryWindowSettings, Settings,
};

use crate::enumerate::WgcMonitorInfo;

#[derive(Debug, thiserror::Error)]
#[error("capture handler error")]
struct BackendError;

#[derive(Default)]
pub struct WgcCaptureBackend;

impl WgcCaptureBackend {
    pub fn enumerate_targets_with_wgc(
        &self,
        monitors: &[WgcMonitorInfo],
    ) -> Result<Vec<CaptureTarget>, CaptureError> {
        let targets = monitors
            .iter()
            .enumerate()
            .map(|(index, monitor)| CaptureTarget {
                id: format!("display-{index}"),
                name: monitor
                    .friendly_name
                    .clone()
                    .unwrap_or_else(|| format!("Display {}", index + 1)),
                kind: CaptureTargetKind::Display,
                primary: monitor.primary,
                size: monitor.size,
                refresh_hz: monitor.refresh_hz,
                native_index: monitor.native_index,
                device_name: Some(monitor.device_name.clone()),
                adapter_name: Some(monitor.device_string.clone()),
                adapter_index: None,
                output_index: None,
                available_backends: vec![CaptureBackendKind::WindowsGraphicsCapture],
                preferred_backend: CaptureBackendKind::WindowsGraphicsCapture,
            })
            .collect();

        Ok(targets)
    }
}

impl CaptureBackend for WgcCaptureBackend {
    fn enumerate_targets(&self) -> Result<Vec<CaptureTarget>, CaptureError> {
        self.enumerate_targets_with_wgc(&crate::enumerate::enumerate_wgc_monitors()?)
    }

    fn start(
        &self,
        target: &CaptureTarget,
        options: CaptureOptions,
    ) -> Result<Box<dyn CaptureSession>, CaptureError> {
        if target.kind != CaptureTargetKind::Display {
            return Err(CaptureError::UnsupportedTarget(target.id.clone()));
        }

        let monitor = Monitor::from_index(target.native_index + 1)
            .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;
        let (sender, receiver) = bounded(options.buffer_depth.max(1));
        let total_frames = Arc::new(AtomicU64::new(0));
        let dropped_frames = Arc::new(AtomicU64::new(0));

        // Canonical format = BGRA (DXGI native). WGC Bgra8 path avoids RGBA/BGRA mismatch.
        let flags = HandlerFlags {
            sender,
            total_frames: total_frames.clone(),
            dropped_frames: dropped_frames.clone(),
            pixel_format: PixelFormat::Bgra8Unorm,
        };

        let settings = Settings::new(
            monitor,
            if options.include_cursor {
                CursorCaptureSettings::WithCursor
            } else {
                CursorCaptureSettings::WithoutCursor
            },
            if options.draw_border {
                DrawBorderSettings::Default
            } else {
                DrawBorderSettings::WithoutBorder
            },
            SecondaryWindowSettings::Default,
            MinimumUpdateIntervalSettings::Custom(Duration::from_millis(
                (1000 / options.target_fps.max(1)) as u64,
            )),
            DirtyRegionSettings::Default,
            ColorFormat::Bgra8,
            flags,
        );

        let control = FrameHandler::start_free_threaded(settings)
            .map_err(|err| CaptureError::BackendUnavailable(err.to_string()))?;

        Ok(Box::new(WgcCaptureSession {
            receiver,
            control: Mutex::new(Some(control)),
            total_frames,
            dropped_frames,
            last_error: Mutex::new(None),
        }))
    }

    fn supports_zero_copy(&self) -> bool {
        true
    }
}

struct HandlerFlags {
    sender: Sender<CapturePacket>,
    total_frames: Arc<AtomicU64>,
    dropped_frames: Arc<AtomicU64>,
    pixel_format: PixelFormat,
}

struct FrameHandler {
    sender: Sender<CapturePacket>,
    total_frames: Arc<AtomicU64>,
    dropped_frames: Arc<AtomicU64>,
    pixel_format: PixelFormat,
}

impl GraphicsCaptureApiHandler for FrameHandler {
    type Flags = HandlerFlags;
    type Error = BackendError;

    fn new(ctx: Context<Self::Flags>) -> Result<Self, Self::Error> {
        Ok(Self {
            sender: ctx.flags.sender,
            total_frames: ctx.flags.total_frames,
            dropped_frames: ctx.flags.dropped_frames,
            pixel_format: ctx.flags.pixel_format,
        })
    }

    fn on_frame_arrived(
        &mut self,
        frame: &mut Frame,
        _capture_control: InternalCaptureControl,
    ) -> Result<(), Self::Error> {
        let desc = *frame.desc();
        let texture = frame.as_raw_texture().clone();
        let dirty_regions = frame
            .dirty_regions()
            .unwrap_or_default()
            .into_iter()
            .map(|region| {
                RectI::new(
                    region.x,
                    region.y,
                    region.width.max(0) as u32,
                    region.height.max(0) as u32,
                )
            })
            .collect();
        let frame_number = self.total_frames.fetch_add(1, Ordering::Relaxed) + 1;
        let shared = D3d11TextureHandle::new(
            texture,
            Size2D::new(desc.Width, desc.Height),
            self.pixel_format,
        )
        .into_shared();
        let packet = CapturePacket::new(frame_number, CaptureFrame::D3d11(shared), dirty_regions);

        match self.sender.try_send(packet) {
            Ok(()) => Ok(()),
            Err(TrySendError::Full(_)) | Err(TrySendError::Disconnected(_)) => {
                self.dropped_frames.fetch_add(1, Ordering::Relaxed);
                Ok(())
            }
        }
    }

    fn on_closed(&mut self) -> Result<(), Self::Error> {
        Ok(())
    }
}

struct WgcCaptureSession {
    receiver: Receiver<CapturePacket>,
    control: Mutex<Option<CaptureControl<FrameHandler, BackendError>>>,
    total_frames: Arc<AtomicU64>,
    dropped_frames: Arc<AtomicU64>,
    last_error: Mutex<Option<String>>,
}

impl CaptureSession for WgcCaptureSession {
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
        if let Some(control) = self.control.lock().take() {
            control
                .stop()
                .map_err(|err| CaptureError::Other(err.to_string()))?;
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
        CaptureBackendKind::WindowsGraphicsCapture
    }
}

impl Drop for WgcCaptureSession {
    fn drop(&mut self) {
        if let Some(control) = self.control.get_mut().take() {
            let _ = control.stop();
        }
    }
}
