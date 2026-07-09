//! Windows capture — DXGI Desktop Duplication only (OBS Display Capture axis).
//! Shared D3D11 device factory keeps crop/preprocess on the same adapter when possible.

mod device;
mod dxgi;
mod enumerate;
mod resolve;
#[allow(dead_code)]
mod wgc;

use capture_core::{
    CaptureBackend, CaptureError, CaptureOptions, CaptureSession, CaptureTarget,
};

use crate::dxgi::DxgiDuplicationBackend;
use crate::enumerate::enumerate_wgc_monitors;
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
        // DXGI outputs are the source of truth. WGC monitor metadata is optional enrichment.
        let monitors = enumerate_wgc_monitors().unwrap_or_default();
        self.dxgi.enumerate_targets_with_wgc(&monitors)
    }

    fn start(
        &self,
        target: &CaptureTarget,
        options: CaptureOptions,
    ) -> Result<Box<dyn CaptureSession>, CaptureError> {
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

        assert!(error.to_string().contains("DXGI"));
    }
}
