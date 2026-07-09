use capture_core::{
    CaptureBackendKind, CaptureBackendPreference, CaptureError, CaptureTarget,
};

pub fn resolve_backend(
    target: &CaptureTarget,
    preference: CaptureBackendPreference,
) -> Result<CaptureBackendKind, CaptureError> {
    let requested = match preference {
        CaptureBackendPreference::Auto => target.preferred_backend,
        CaptureBackendPreference::WindowsGraphicsCapture => {
            CaptureBackendKind::WindowsGraphicsCapture
        }
        CaptureBackendPreference::DxgiDuplication => CaptureBackendKind::DxgiDuplication,
        CaptureBackendPreference::ObsAdapter => CaptureBackendKind::ObsAdapter,
        other => {
            return Err(CaptureError::BackendUnavailable(format!(
                "unsupported backend preference {other:?}"
            )));
        }
    };

    if target.available_backends.contains(&requested) {
        Ok(requested)
    } else {
        Err(CaptureError::BackendUnavailable(format!(
            "requested backend {:?} is not available for {}",
            requested, target.name
        )))
    }
}
