//! Windows capture backends (WGC + DXGI duplication).
//! Shared D3D11 device factory keeps crop/preprocess on the same adapter when possible.

mod device;
mod dxgi;
mod enumerate;
mod resolve;
mod wgc;

use capture_core::{
    CaptureBackend, CaptureBackendKind, CaptureError, CaptureOptions, CaptureSession, CaptureTarget,
};

use crate::dxgi::DxgiDuplicationBackend;
use crate::enumerate::enumerate_wgc_monitors;
use crate::resolve::resolve_backend;
use crate::wgc::WgcCaptureBackend;

pub use crate::device::SharedD3d11Device;

#[derive(Default)]
pub struct WindowsCaptureBackend {
    wgc: WgcCaptureBackend,
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
        let wgc_monitors = enumerate_wgc_monitors()?;
        match self.dxgi.enumerate_targets_with_wgc(&wgc_monitors) {
            Ok(targets) if !targets.is_empty() => Ok(targets),
            Ok(_) | Err(_) => self.wgc.enumerate_targets_with_wgc(&wgc_monitors),
        }
    }

    fn start(
        &self,
        target: &CaptureTarget,
        options: CaptureOptions,
    ) -> Result<Box<dyn CaptureSession>, CaptureError> {
        // Session.backend_kind() is the source of truth — facade does not report a fixed kind.
        match resolve_backend(target, options.backend_preference)? {
            CaptureBackendKind::WindowsGraphicsCapture => self.wgc.start(target, options),
            CaptureBackendKind::DxgiDuplication => self.dxgi.start(target, options),
            CaptureBackendKind::ObsAdapter => Err(CaptureError::BackendUnavailable(
                "OBS adapter backend is not implemented yet".to_owned(),
            )),
            other => Err(CaptureError::BackendUnavailable(format!(
                "capture backend {other:?} is not implemented"
            ))),
        }
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
    fn auto_prefers_target_default_backend() {
        let target = make_target(vec![
            CaptureBackendKind::WindowsGraphicsCapture,
            CaptureBackendKind::DxgiDuplication,
        ]);

        let backend = resolve_backend(&target, CaptureBackendPreference::Auto)
            .expect("backend should resolve");

        assert_eq!(backend, CaptureBackendKind::WindowsGraphicsCapture);
    }

    #[test]
    fn forced_backend_fails_when_unavailable() {
        let target = make_target(vec![CaptureBackendKind::DxgiDuplication]);

        let error = resolve_backend(&target, CaptureBackendPreference::WindowsGraphicsCapture)
            .expect_err("backend should fail");

        assert!(error.to_string().contains("not available"));
    }
}
