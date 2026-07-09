//! Multi-vendor inference: DirectML / OpenVINO / CPU via ort EP chain.
//! Preprocess (resize/NCHW) lives in `vision-gpu` / `pipeline`.

use std::fs;

use capture_core::{
    CpuNchwTensor, DetectionResult, InferenceBackend, InferenceDiagnostics, InferenceError,
    InferenceIoDescriptor, InferenceModelMetadata, InferenceSettings, ProviderState, RectI, Size2D,
    TensorInputHandle, default_execution_providers,
};
use ort::ep;
use ort::ep::ExecutionProviderDispatch;
use ort::session::Session;
use ort::session::builder::GraphOptimizationLevel;
use ort::value::{Outlet, TensorRef};
use tracing::warn;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ProviderKind {
    DirectMl,
    OpenVino,
    Cpu,
}

pub struct DirectMlInferenceBackend {
    session: Option<Session>,
    provider_state: ProviderState,
    last_error: Option<String>,
    class_names: Vec<String>,
    settings: Option<InferenceSettings>,
    diagnostics: InferenceDiagnostics,
}

impl Default for DirectMlInferenceBackend {
    fn default() -> Self {
        Self {
            session: None,
            provider_state: ProviderState::Uninitialized,
            last_error: None,
            class_names: Vec::new(),
            settings: None,
            diagnostics: InferenceDiagnostics::default(),
        }
    }
}

impl DirectMlInferenceBackend {
    fn reset_diagnostics(&mut self, settings: &InferenceSettings) {
        self.diagnostics = InferenceDiagnostics {
            model_path: Some(settings.model_path.clone()),
            class_names_path: Some(settings.class_names_path.clone()),
            expected_input_size: Some(settings.input_size),
            ..InferenceDiagnostics::default()
        };
    }

    fn load_class_names(&mut self, settings: &InferenceSettings) -> Result<(), InferenceError> {
        let body = fs::read_to_string(&settings.class_names_path)
            .map_err(|err| InferenceError::Other(err.to_string()))?;
        self.class_names = body
            .lines()
            .map(str::trim)
            .filter(|line| !line.is_empty())
            .map(ToOwned::to_owned)
            .collect();
        self.diagnostics.class_count = self.class_names.len();
        Ok(())
    }

    fn set_last_error_message(&mut self, message: String) {
        self.last_error = Some(message.clone());
        self.diagnostics.last_error = Some(message);
    }

    fn clear_last_error(&mut self) {
        self.last_error = None;
        self.diagnostics.last_error = None;
    }

    fn sync_session_diagnostics(&mut self, session: &Session) {
        self.diagnostics.inputs = describe_outlets(session.inputs());
        self.diagnostics.outputs = describe_outlets(session.outputs());
        self.diagnostics.model_metadata = session
            .metadata()
            .ok()
            .map(|metadata| InferenceModelMetadata {
                producer: metadata.producer(),
                graph_name: metadata.name(),
                domain: metadata.domain(),
                description: metadata.description(),
                graph_description: metadata.graph_description(),
                version: metadata.version(),
            })
            .unwrap_or_default();
        self.diagnostics.validation_notes = build_validation_notes(
            &self.diagnostics.inputs,
            &self.diagnostics.outputs,
            self.diagnostics.expected_input_size,
        );
    }

    fn run_nchw(&mut self, tensor: &CpuNchwTensor) -> Result<Vec<DetectionResult>, InferenceError> {
        let settings = self.settings.clone().ok_or_else(|| {
            let error = InferenceError::NotInitialized;
            self.set_last_error_message(error.to_string());
            error
        })?;

        let shape = tensor.shape();
        let input_tensor = TensorRef::from_array_view((shape, tensor.data.as_slice()))
            .map_err(|err| InferenceError::Other(err.to_string()))
            .inspect_err(|err| {
                self.set_last_error_message(err.to_string());
            })?;

        if self.session.is_none() {
            let error = InferenceError::NotInitialized;
            self.set_last_error_message(error.to_string());
            return Err(error);
        }

        let mut observed_output_shape = None;
        let detections_result = {
            let session = self
                .session
                .as_mut()
                .ok_or(InferenceError::NotInitialized)?;

            let outputs = session
                .run(ort::inputs![input_tensor])
                .map_err(|err| InferenceError::Other(err.to_string()))?;

            if outputs.len() == 0 {
                Ok(Vec::new())
            } else {
                let (shape, data) = outputs[0]
                    .try_extract_tensor::<f32>()
                    .map_err(|err| InferenceError::Other(err.to_string()))?;
                let runtime_shape = runtime_shape(shape)?;
                observed_output_shape = Some(runtime_shape.iter().map(|dim| *dim as i64).collect());
                parse_detections(
                    runtime_shape,
                    data,
                    &self.class_names,
                    tensor.width,
                    tensor.height,
                    settings.confidence_threshold,
                    settings.max_detections,
                )
            }
        };

        self.diagnostics.last_output_shape = observed_output_shape;
        match detections_result {
            Ok(detections) => {
                self.clear_last_error();
                Ok(detections)
            }
            Err(err) => {
                self.set_last_error_message(err.to_string());
                Err(err)
            }
        }
    }

    fn try_commit_session(
        &self,
        model_path: &std::path::Path,
        providers: Vec<ExecutionProviderDispatch>,
    ) -> Result<Session, InferenceError> {
        Session::builder()
            .map_err(|err| InferenceError::Other(err.to_string()))?
            .with_execution_providers(providers)
            .map_err(|err| InferenceError::Other(err.to_string()))?
            .with_optimization_level(GraphOptimizationLevel::Level3)
            .map_err(|err| InferenceError::Other(err.to_string()))?
            .commit_from_file(model_path)
            .map_err(|err| InferenceError::Other(err.to_string()))
    }
}

impl InferenceBackend for DirectMlInferenceBackend {
    fn backend_name(&self) -> &'static str {
        match &self.provider_state {
            ProviderState::DirectMl { .. } => "DirectML",
            ProviderState::OpenVino { .. } => "OpenVINO",
            ProviderState::CpuFallback => "CPU",
            ProviderState::Failed(_) => "Failed",
            ProviderState::Uninitialized => "Uninitialized",
        }
    }

    fn initialize(&mut self, settings: InferenceSettings) -> Result<ProviderState, InferenceError> {
        self.settings = Some(settings.clone());
        self.session = None;
        self.provider_state = ProviderState::Uninitialized;
        self.class_names.clear();
        self.reset_diagnostics(&settings);
        self.clear_last_error();

        if !settings.model_path.exists() {
            let error = InferenceError::MissingModel(settings.model_path.clone());
            self.set_last_error_message(error.to_string());
            return Err(error);
        }
        if !settings.class_names_path.exists() {
            let error = InferenceError::MissingModel(settings.class_names_path.clone());
            self.set_last_error_message(error.to_string());
            return Err(error);
        }

        self.load_class_names(&settings)?;

        let providers = normalize_execution_providers(&settings.execution_providers);
        let openvino_devices =
            resolve_openvino_device_types(settings.openvino_device_type.as_deref());
        let mut attempt_log = Vec::new();

        for kind in &providers {
            match kind {
                ProviderKind::DirectMl => {
                    let ep = ep::DirectML::default()
                        .with_device_id(settings.selected_device_id as i32)
                        .build();
                    match self.try_commit_session(&settings.model_path, vec![ep]) {
                        Ok(session) => {
                            self.sync_session_diagnostics(&session);
                            self.session = Some(session);
                            self.provider_state = ProviderState::DirectMl {
                                device_id: settings.selected_device_id,
                                cpu_fallback: false,
                            };
                            self.diagnostics.fallback_reason =
                                fallback_reason_from_attempts(&attempt_log);
                            self.clear_last_error();
                            return Ok(self.provider_state.clone());
                        }
                        Err(err) => {
                            warn!("DirectML session failed: {err}");
                            attempt_log.push(format!("directml: {err}"));
                        }
                    }
                }
                ProviderKind::OpenVino => {
                    for device_type in &openvino_devices {
                        let ep = ep::OpenVINO::default()
                            .with_device_type(device_type)
                            .build();
                        match self.try_commit_session(&settings.model_path, vec![ep]) {
                            Ok(session) => {
                                self.sync_session_diagnostics(&session);
                                self.session = Some(session);
                                self.provider_state = ProviderState::OpenVino {
                                    device_type: device_type.clone(),
                                };
                                self.diagnostics.fallback_reason =
                                    fallback_reason_from_attempts(&attempt_log);
                                self.clear_last_error();
                                return Ok(self.provider_state.clone());
                            }
                            Err(err) => {
                                warn!("OpenVINO ({device_type}) session failed: {err}");
                                attempt_log.push(format!("openvino/{device_type}: {err}"));
                            }
                        }
                    }
                }
                ProviderKind::Cpu => {
                    let ep = ep::CPUExecutionProvider::default().build();
                    match self.try_commit_session(&settings.model_path, vec![ep]) {
                        Ok(session) => {
                            self.sync_session_diagnostics(&session);
                            self.session = Some(session);
                            self.provider_state = ProviderState::CpuFallback;
                            self.diagnostics.fallback_reason =
                                fallback_reason_from_attempts(&attempt_log);
                            self.clear_last_error();
                            return Ok(self.provider_state.clone());
                        }
                        Err(err) => {
                            warn!("CPU session failed: {err}");
                            attempt_log.push(format!("cpu: {err}"));
                        }
                    }
                }
            }
        }

        let combined = if attempt_log.is_empty() {
            "no execution providers configured".to_owned()
        } else {
            format!("all providers failed: {}", attempt_log.join("; "))
        };
        self.provider_state = ProviderState::Failed(combined.clone());
        self.diagnostics.fallback_reason = Some(combined.clone());
        self.set_last_error_message(combined.clone());
        Err(InferenceError::Other(combined))
    }

    fn provider_state(&self) -> ProviderState {
        self.provider_state.clone()
    }

    fn infer(&mut self, input: &TensorInputHandle) -> Result<Vec<DetectionResult>, InferenceError> {
        let tensor = input
            .cpu_nchw
            .as_ref()
            .ok_or(InferenceError::MissingPreprocessedTensor)
            .inspect_err(|err| self.set_last_error_message(err.to_string()))?;
        self.run_nchw(tensor)
    }

    fn last_error(&self) -> Option<String> {
        self.last_error.clone()
    }

    fn diagnostics(&self) -> InferenceDiagnostics {
        self.diagnostics.clone()
    }
}

/// Normalize config EP names: `auto` → default order, dedupe, skip unknowns.
#[must_use]
pub fn normalize_execution_providers(raw: &[String]) -> Vec<ProviderKind> {
    let defaults = default_execution_providers();
    let use_defaults = raw.is_empty() || raw.iter().any(|name| name.eq_ignore_ascii_case("auto"));
    let expanded: Vec<&str> = if use_defaults {
        defaults.iter().map(String::as_str).collect()
    } else {
        raw.iter().map(String::as_str).collect()
    };

    let mut out = Vec::new();
    for name in expanded {
        let kind = match name.trim().to_ascii_lowercase().as_str() {
            "directml" | "dml" => Some(ProviderKind::DirectMl),
            "openvino" | "ov" => Some(ProviderKind::OpenVino),
            "cpu" => Some(ProviderKind::Cpu),
            "auto" => None,
            other => {
                warn!("skipping unknown execution provider `{other}`");
                None
            }
        };
        if let Some(kind) = kind
            && !out.contains(&kind)
        {
            out.push(kind);
        }
    }
    out
}

/// OpenVINO device_type list: explicit override, or GPU then NPU.
#[must_use]
pub fn resolve_openvino_device_types(override_type: Option<&str>) -> Vec<String> {
    if let Some(device) = override_type.map(str::trim).filter(|s| !s.is_empty()) {
        return vec![device.to_owned()];
    }
    vec!["GPU".to_owned(), "NPU".to_owned()]
}

fn fallback_reason_from_attempts(attempts: &[String]) -> Option<String> {
    if attempts.is_empty() {
        None
    } else {
        Some(attempts.join("; "))
    }
}

fn describe_outlets(outlets: &[Outlet]) -> Vec<InferenceIoDescriptor> {
    outlets.iter().map(describe_outlet).collect()
}

fn describe_outlet(outlet: &Outlet) -> InferenceIoDescriptor {
    let tensor_shape = outlet
        .dtype()
        .tensor_shape()
        .map(|shape| shape.iter().copied().collect());
    let tensor_element_type = outlet
        .dtype()
        .tensor_type()
        .map(|element| element.to_string());

    InferenceIoDescriptor {
        name: outlet.name().to_owned(),
        value_type: outlet.dtype().to_string(),
        tensor_shape,
        tensor_element_type,
    }
}

fn build_validation_notes(
    inputs: &[InferenceIoDescriptor],
    outputs: &[InferenceIoDescriptor],
    expected_input_size: Option<Size2D>,
) -> Vec<String> {
    let mut notes = Vec::new();

    if inputs.is_empty() {
        notes.push("model reports no inputs".to_owned());
    }
    if outputs.is_empty() {
        notes.push("model reports no outputs".to_owned());
    }
    if inputs.len() > 1 {
        notes.push(format!(
            "model exposes {} inputs; only the first input is used today",
            inputs.len()
        ));
    }
    if outputs.len() > 1 {
        notes.push(format!(
            "model exposes {} outputs; only the first output is parsed today",
            outputs.len()
        ));
    }

    if let Some(input) = inputs.first()
        && let Some(shape) = input.tensor_shape.as_ref()
    {
        if shape.len() != 4 {
            notes.push(format!(
                "input `{}` reports rank {}; preprocessing assumes rank 4 NCHW/NHWC-compatible input",
                input.name,
                shape.len()
            ));
        } else if let Some(expected) = expected_input_size {
            let height = shape[2];
            let width = shape[3];
            if height > 0
                && width > 0
                && (height as u32 != expected.height || width as u32 != expected.width)
            {
                notes.push(format!(
                    "input `{}` expects {width}x{height}; configured preprocessing targets {}x{}",
                    input.name, expected.width, expected.height
                ));
            }
        }
    }

    if let Some(output) = outputs.first() {
        match output.tensor_shape.as_ref() {
            Some(shape) if !is_supported_output_shape(shape) => notes.push(format!(
                "output `{}` reports shape {:?}; current parser supports row-major [..., >=6] or channel-major [N, >=6, anchors]",
                output.name, shape
            )),
            Some(shape) => {
                if let Some(note) = ambiguous_output_shape_note(&output.name, shape) {
                    notes.push(note);
                }
            }
            None => notes.push(format!(
                "output `{}` is not a tensor output; current parser expects tensor detections",
                output.name
            )),
        }
    }

    notes
}

fn is_supported_output_shape(shape: &[i64]) -> bool {
    if shape.is_empty() {
        return false;
    }

    if shape.last().is_some_and(|last| *last == -1 || *last >= 6) {
        return true;
    }

    shape.len() == 3
        && shape
            .get(1)
            .is_some_and(|channel| *channel == -1 || *channel >= 6)
}

fn ambiguous_output_shape_note(name: &str, shape: &[i64]) -> Option<String> {
    if shape.len() == 3 && shape[1] > 0 && shape[1] < 6 && shape[2] >= 128 {
        return Some(format!(
            "output `{name}` shape {:?} is parseable but unusual; verify whether the model is [N, boxes, attrs] or a custom layout",
            shape
        ));
    }

    None
}

fn runtime_shape(shape: &[i64]) -> Result<Vec<usize>, InferenceError> {
    shape
        .iter()
        .map(|dim| {
            usize::try_from(*dim).map_err(|_| {
                InferenceError::Other(format!(
                    "runtime output shape contains invalid dimension {dim}"
                ))
            })
        })
        .collect()
}

fn parse_detections(
    shape: Vec<usize>,
    data: &[f32],
    class_names: &[String],
    width: u32,
    height: u32,
    confidence_threshold: f32,
    max_detections: usize,
) -> Result<Vec<DetectionResult>, InferenceError> {
    if shape.is_empty() || data.is_empty() {
        return Ok(Vec::new());
    }

    if let Some(last) = shape.last().copied()
        && last >= 6
    {
        return Ok(limit_results(
            parse_row_major(
                shape,
                data,
                class_names,
                width,
                height,
                confidence_threshold,
            ),
            max_detections,
        ));
    }

    if shape.len() == 3 && shape[1] >= 6 {
        return Ok(limit_results(
            parse_channel_major(
                shape,
                data,
                class_names,
                width,
                height,
                confidence_threshold,
            ),
            max_detections,
        ));
    }

    Err(InferenceError::Other(format!(
        "unsupported output shape: {shape:?}"
    )))
}

fn parse_row_major(
    shape: Vec<usize>,
    data: &[f32],
    class_names: &[String],
    width: u32,
    height: u32,
    confidence_threshold: f32,
) -> Vec<DetectionResult> {
    let stride = *shape.last().unwrap_or(&6);
    let rows = data.len() / stride;
    let mut results = Vec::new();

    for row in 0..rows {
        let base = row * stride;
        let record = &data[base..base + stride];
        if let Some(result) = parse_record(record, class_names, width, height, confidence_threshold)
        {
            results.push(result);
        }
    }

    results
}

fn parse_channel_major(
    shape: Vec<usize>,
    data: &[f32],
    class_names: &[String],
    width: u32,
    height: u32,
    confidence_threshold: f32,
) -> Vec<DetectionResult> {
    let channels = shape[1];
    let count = shape[2];
    let mut results = Vec::new();

    for idx in 0..count {
        let mut record = vec![0.0f32; channels];
        for channel in 0..channels {
            record[channel] = data[channel * count + idx];
        }
        if let Some(result) =
            parse_record(&record, class_names, width, height, confidence_threshold)
        {
            results.push(result);
        }
    }

    results
}

fn parse_record(
    record: &[f32],
    class_names: &[String],
    width: u32,
    height: u32,
    confidence_threshold: f32,
) -> Option<DetectionResult> {
    if record.len() < 6 {
        return None;
    }

    let (x1, y1, x2, y2, confidence, class_id) = if record.len() == 6 {
        (
            record[0],
            record[1],
            record[2],
            record[3],
            record[4],
            record[5] as i32,
        )
    } else {
        let objectness = record[4];
        let (best_class, best_score) =
            record[5..].iter().copied().enumerate().max_by(|lhs, rhs| {
                lhs.1
                    .partial_cmp(&rhs.1)
                    .unwrap_or(std::cmp::Ordering::Equal)
            })?;
        let cx = record[0];
        let cy = record[1];
        let w = record[2];
        let h = record[3];
        (
            cx - w / 2.0,
            cy - h / 2.0,
            cx + w / 2.0,
            cy + h / 2.0,
            objectness * best_score,
            best_class as i32,
        )
    };

    if confidence < confidence_threshold {
        return None;
    }

    let x1 = x1.clamp(0.0, width as f32);
    let y1 = y1.clamp(0.0, height as f32);
    let x2 = x2.clamp(0.0, width as f32);
    let y2 = y2.clamp(0.0, height as f32);
    let label = class_names
        .get(class_id.max(0) as usize)
        .cloned()
        .unwrap_or_else(|| format!("class-{class_id}"));

    Some(DetectionResult {
        bounding_box: RectI::new(
            x1.round() as i32,
            y1.round() as i32,
            (x2 - x1).max(0.0).round() as u32,
            (y2 - y1).max(0.0).round() as u32,
        ),
        confidence_milli: (confidence.clamp(0.0, 1.0) * 1000.0).round() as u16,
        label,
        class_id,
    })
}

fn limit_results(mut results: Vec<DetectionResult>, max_detections: usize) -> Vec<DetectionResult> {
    results.sort_by(|lhs, rhs| rhs.confidence_milli.cmp(&lhs.confidence_milli));
    results.truncate(max_detections);
    results
}

#[cfg(test)]
mod tests {
    use super::{
        ProviderKind, build_validation_notes, is_supported_output_shape,
        normalize_execution_providers, parse_detections, resolve_openvino_device_types,
    };
    use capture_core::{InferenceIoDescriptor, Size2D};

    #[test]
    fn parses_row_major_xyxy_records() {
        let detections = parse_detections(
            vec![2, 6],
            &[
                10.0, 20.0, 110.0, 120.0, 0.9, 1.0, 5.0, 15.0, 55.0, 65.0, 0.4, 0.0,
            ],
            &["zero".to_owned(), "one".to_owned()],
            320,
            320,
            0.5,
            10,
        )
        .expect("parser should succeed");

        assert_eq!(detections.len(), 1);
        assert_eq!(detections[0].label, "one");
        assert_eq!(detections[0].bounding_box.width, 100);
        assert_eq!(detections[0].bounding_box.height, 100);
    }

    #[test]
    fn parses_channel_major_yolo_records() {
        let detections = parse_detections(
            vec![1, 7, 2],
            &[
                50.0, 100.0, 60.0, 80.0, 40.0, 20.0, 30.0, 20.0, 0.9, 0.3, 0.1, 0.7, 0.8, 0.2,
            ],
            &["person".to_owned(), "vehicle".to_owned()],
            320,
            320,
            0.5,
            10,
        )
        .expect("parser should succeed");

        assert_eq!(detections.len(), 1);
        assert_eq!(detections[0].label, "vehicle");
        assert_eq!(detections[0].confidence_milli, 720);
    }

    #[test]
    fn rejects_unsupported_output_shape() {
        let error = parse_detections(vec![1, 4, 4], &[0.0; 16], &[], 320, 320, 0.5, 10)
            .expect_err("shape should be rejected");

        assert!(
            error
                .to_string()
                .contains("unsupported output shape: [1, 4, 4]")
        );
    }

    #[test]
    fn validation_notes_flag_shape_mismatches() {
        let notes = build_validation_notes(
            &[InferenceIoDescriptor {
                name: "images".to_owned(),
                value_type: "Tensor<f32>(1,3,320,320)".to_owned(),
                tensor_shape: Some(vec![1, 3, 320, 320]),
                tensor_element_type: Some("f32".to_owned()),
            }],
            &[InferenceIoDescriptor {
                name: "output0".to_owned(),
                value_type: "Tensor<f32>(1,4,8400)".to_owned(),
                tensor_shape: Some(vec![1, 4, 8400]),
                tensor_element_type: Some("f32".to_owned()),
            }],
            Some(Size2D::new(640, 640)),
        );

        assert_eq!(notes.len(), 2);
        assert!(
            notes
                .iter()
                .any(|note| note.contains("configured preprocessing"))
        );
        assert!(
            notes
                .iter()
                .any(|note| note.contains("parseable but unusual"))
        );
    }

    #[test]
    fn dynamic_output_shape_is_treated_as_supported() {
        assert!(is_supported_output_shape(&[-1, -1, 84]));
        assert!(is_supported_output_shape(&[1, -1, 8400]));
    }

    #[test]
    fn auto_expands_to_default_provider_order() {
        let providers = normalize_execution_providers(&["auto".to_owned()]);
        assert_eq!(
            providers,
            vec![
                ProviderKind::DirectMl,
                ProviderKind::OpenVino,
                ProviderKind::Cpu
            ]
        );
    }

    #[test]
    fn empty_providers_use_default_order() {
        let providers = normalize_execution_providers(&[]);
        assert_eq!(
            providers,
            vec![
                ProviderKind::DirectMl,
                ProviderKind::OpenVino,
                ProviderKind::Cpu
            ]
        );
    }

    #[test]
    fn dedupes_and_skips_unknown_providers() {
        let providers = normalize_execution_providers(&[
            "DirectML".to_owned(),
            "openvino".to_owned(),
            "directml".to_owned(),
            "unknown-ep".to_owned(),
            "cpu".to_owned(),
        ]);
        assert_eq!(
            providers,
            vec![
                ProviderKind::DirectMl,
                ProviderKind::OpenVino,
                ProviderKind::Cpu
            ]
        );
    }

    #[test]
    fn openvino_device_types_default_to_gpu_then_npu() {
        assert_eq!(
            resolve_openvino_device_types(None),
            vec!["GPU".to_owned(), "NPU".to_owned()]
        );
        assert_eq!(
            resolve_openvino_device_types(Some("NPU")),
            vec!["NPU".to_owned()]
        );
        assert_eq!(
            resolve_openvino_device_types(Some("  GPU.0  ")),
            vec!["GPU.0".to_owned()]
        );
    }
}
