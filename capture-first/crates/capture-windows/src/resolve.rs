use capture_core::{
    CaptureBackendKind, CaptureBackendPreference, CaptureError, CaptureTarget,
};

/// DXGI-only capture path. Preference does not select WGC; it only validates DXGI availability.
pub fn resolve_backend(
    target: &CaptureTarget,
    _preference: CaptureBackendPreference,
) -> Result<CaptureBackendKind, CaptureError> {
    if target
        .available_backends
        .contains(&CaptureBackendKind::DxgiDuplication)
    {
        Ok(CaptureBackendKind::DxgiDuplication)
    } else {
        Err(CaptureError::BackendUnavailable(format!(
            "Display session is not available for {} (adapter={:?}, output={:?}, device={:?})",
            target.name,
            target.adapter_index,
            target.output_index,
            target.device_name
        )))
    }
}
