//! Windows capture — DXGI Desktop Duplication only (display-share class).
//! Shared D3D11 device factory keeps crop/preprocess on the same adapter when possible.
//! No hooks, no WGC start path (no capture border).

mod device;
mod dxgi;
mod enumerate;
mod resolve;

use capture_core::{
    CaptureBackend, CaptureError, CaptureOptions, CaptureSession, CaptureTarget,
};

use crate::dxgi::DxgiDuplicationBackend;
use crate::enumerate::enumerate_display_monitors;
use crate::resolve::resolve_backend;

pub use crate::device::SharedD3d11Device;

#[derive(Default)]
pub struct WindowsCaptureBackend {
    dxgi: DxgiDuplicationBackend,
}

impl WindowsCaptureBackend {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }
}

impl CaptureBackend for WindowsCaptureBackend {
    fn enumerate_targets(&self) -> Result<Vec<CaptureTarget>, CaptureError> {
        // DXGI outputs are the source of truth. Win32 display metadata is optional enrichment.
        let monitors = enumerate_display_monitors().unwrap_or_default();
        self.dxgi.enumerate_targets_with_monitors(&monitors)
    }

    fn start(
        &self,
        target: &CaptureTarget,
        options: CaptureOptions,
    ) -> Result<Box<dyn CaptureSession>, CaptureError> {
        // Always sanitize: no cursor, no border, no hooks, DXGI preference.
        let options = options.sanitize_for_privacy();
        // DXGI-only path — no WGC fallback. Preference is ignored for API selection.
        let _ = resolve_backend(target, options.backend_preference)?;
        self.dxgi.start(target, options)
    }

    fn supports_zero_copy(&self) -> bool {
        true
    }
}

#[cfg(test)]
mod tests {
    use capture_core::{
        CaptureBackendKind, CaptureBackendPreference, CaptureTarget, CaptureTargetKind, Size2D,
    };

    use crate::resolve::resolve_backend;

    fn make_target(available_backends: Vec<CaptureBackendKind>) -> CaptureTarget {
        CaptureTarget {
            id: "display-0".to_owned(),
            name: "Primary".to_owned(),
            kind: CaptureTargetKind::Display,
            primary: true,
            size: Size2D::new(1920, 1080),
            refresh_hz: 144,
            native_index: 0,
            device_name: Some("\\\\.\\DISPLAY1".to_owned()),
            adapter_name: Some("GPU".to_owned()),
            adapter_index: Some(0),
            output_index: Some(0),
            preferred_backend: available_backends[0],
            available_backends,
        }
    }

    #[test]
    fn auto_resolves_to_dxgi_only() {
        let target = make_target(vec![CaptureBackendKind::DxgiDuplication]);

        let backend = resolve_backend(&target, CaptureBackendPreference::Auto)
            .expect("backend should resolve");

        assert_eq!(backend, CaptureBackendKind::DxgiDuplication);
    }

    #[test]
    fn prefer_wgc_still_resolves_to_dxgi() {
        let target = make_target(vec![CaptureBackendKind::DxgiDuplication]);

        let backend =
            resolve_backend(&target, CaptureBackendPreference::WindowsGraphicsCapture)
                .expect("DXGI-only path ignores WGC preference");

        assert_eq!(backend, CaptureBackendKind::DxgiDuplication);
    }

    #[test]
    fn missing_dxgi_fails_clearly() {
        let target = make_target(vec![CaptureBackendKind::WindowsGraphicsCapture]);

        let error = resolve_backend(&target, CaptureBackendPreference::Auto).expect_err(
            "DXGI must be available",
        );

        assert!(error.to_string().contains("Display session"));
    }

    /// Hardware smoke: real DXGI session must deliver at least one frame.
    #[test]
    fn smoke_dxgi_capture_receives_frame() {
        use std::time::{Duration, Instant};

        use capture_core::{CaptureBackend, CaptureBackendKind, CaptureOptions};

        use crate::WindowsCaptureBackend;

        let backend = WindowsCaptureBackend::new();
        let targets = backend
            .enumerate_targets()
            .expect("enumerate displays");
        assert!(
            !targets.is_empty(),
            "no displays found — cannot smoke-test capture"
        );
        eprintln!(
            "smoke: enumerated {} display target(s): {:?}",
            targets.len(),
            targets
                .iter()
                .map(|t| format!(
                    "{} {}x{} primary={} backend={:?}",
                    t.name, t.size.width, t.size.height, t.primary, t.preferred_backend
                ))
                .collect::<Vec<_>>()
        );

        let target = targets
            .iter()
            .find(|t| t.primary)
            .unwrap_or(&targets[0])
            .clone();

        // Deliberately "dirty" options — sanitize must force share-class defaults.
        let options = CaptureOptions {
            include_cursor: true,
            draw_border: true,
            target_fps: 30,
            buffer_depth: 2,
            compatibility: capture_core::CompatibilityPolicy {
                no_hook: false,
                obs_adapter_allowed: true,
            },
            backend_preference: CaptureBackendPreference::WindowsGraphicsCapture,
        };

        let session = backend
            .start(&target, options)
            .expect("start display session");
        assert_eq!(session.backend_kind(), CaptureBackendKind::DxgiDuplication);

        let deadline = Instant::now() + Duration::from_secs(5);
        let mut got_frame = false;
        let mut frames = 0u64;
        let mut last_size = None;
        while Instant::now() < deadline {
            match session.try_recv() {
                Ok(Some(packet)) => {
                    frames = packet.frame_number.max(frames);
                    let size = packet.frame.size();
                    assert!(size.width > 0 && size.height > 0, "empty frame size");
                    last_size = Some(size);
                    got_frame = true;
                    if frames >= 3 {
                        break;
                    }
                }
                Ok(None) => std::thread::sleep(Duration::from_millis(5)),
                Err(err) => panic!("session error while waiting for frames: {err}"),
            }
        }

        session.stop().expect("stop session");
        assert!(
            got_frame,
            "no capture frames within 5s on {}",
            target.name
        );
        assert!(frames >= 1, "expected frame_number >= 1, got {frames}");
        let size = last_size.expect("frame size");
        eprintln!(
            "smoke: ok backend=DxgiDuplication target={} frames>={} size={}x{} (sanitize forced DXGI despite WGC preference)",
            target.name, frames, size.width, size.height
        );
    }
}
