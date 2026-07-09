# Migration Tasks

Backlog for the Rust `capture-first` SmartScreenCapture workspace.

## Track 0: Design Refactor (layer boundaries)

- [x] Split D3D11 implementation out of `capture-core` into `graphics-d3d11`
- [x] Replace `as_any` downcast web with `CaptureFrame::{D3d11,Cpu}` + `GpuTexture` trait
- [x] Extract `pipeline` crate (ROI vs ModelInput, single-readback policy)
- [x] Capture hygiene: session `backend_kind` as source of truth; BGRA canonical; shared D3D11 device cache; module split
- [x] `RuntimeBindings` config→runtime mapping (`frame_buffer_size` → `buffer_depth`)
- [x] Honest `prepare_for_inference` (CPU NCHW preprocess) + `HsvMaskStats`
- [x] App uses `InferenceBackend::infer` only; preprocess owned by vision/pipeline
- [x] TelemetryHub single aggregator; texture create failures return `Result` (no `expect`)

## Track 1: Capture Backends

- [ ] Windows Graphics Capture backend using `windows-capture`
- [ ] Backend selector that can choose `WGC` or `DXGI duplication` per target
- [x] `DXGI duplication` backend implementation behind the existing `CaptureBackend` trait
- [ ] Optional `OBS/libobs` adapter for compatibility fallback
- [x] Capture target metadata for adapter, output name, refresh rate, and backend capability
- [ ] Explicit fail-closed handling for unsupported or protected targets

Completion criteria:
- `Display` capture works with `DXGI duplication`; WGC remains future work
- Backend choice is visible in UI and logs
- Unsupported targets fail without crashes or hooks

## Track 2: Zero-Copy Vision Pipeline

- [x] D3D11 texture capture contract
- [x] GPU ROI crop via `CopySubresourceRegion`
- [ ] GPU resize and color conversion for inference input
- [ ] Shader-based preprocessing path for 320x320 and model input sizes
- [ ] Remove CPU readback from inference hot path
- [x] Make operator preview readback explicit and optional
- [x] Reuse a single CPU readback for preview, HSV, and current CPU inference fallback path

Completion criteria:
- Capture to ROI to inference preprocessing stays on GPU
- CPU readback only happens for debug, preview, save, or compatibility fallback

## Track 3: Inference

- [x] DirectML-first session initialization
- [x] CPU fallback session initialization
- [x] OpenVINO EP in provider chain (Intel GPU/NPU; soft-fail if runtime missing)
- [x] Wire config `execution_providers` into session init (`directml` → `openvino` → `cpu`)
- [x] Basic YOLO output parsing for common row-major and channel-major outputs
- [ ] Validate parsing against the actual shipped YOLO model outputs
- [x] Support model metadata inspection and shape diagnostics in UI/logs
- [x] Add regression tests for output parsing and validation notes
- [x] Add regression tests for provider list normalization / OpenVINO device_type resolution
- [ ] Add regression tests for live provider fallback against a real `.onnx` asset

Completion criteria:
- The shipped ONNX model produces stable detections on Windows 11
- Provider state and fallback cause are visible in UI/logs
- Intel Gram (OpenVINO) and DirectML GPUs share the same YOLO path

Current blocker:
- The repository does not currently contain the shipped `.onnx` model, so real-model output validation remains pending until that asset is available on disk.

## Track 4: UI And App Control

- [x] Target enumeration and Start/Stop flow
- [x] ROI preview in egui
- [x] Performance and provider state display
- [x] Save settings back to JSON
- [x] Preview mode selector and runtime processing toggles
- [x] Better operator diagnostics for backend, target, dropped frames, and last inference error
- [ ] Texture-native preview path that avoids CPU image upload when a renderer bridge is available

Completion criteria:
- The Rust app is operational for operators without a separate UI stack
- Operators can understand backend choice, provider choice, and failure reasons from the UI alone

## Track 5: Verification

- [x] Workspace `cargo check`
- [x] Workspace `cargo test`
- [ ] Windows 11 manual smoke test on real hardware
- [ ] 4K60 latency and frame-drop benchmark
- [ ] 1440p240 stability benchmark
- [x] Headless integration tests for `capture -> ROI -> HSV/YOLO -> telemetry`

Completion criteria:
- Real-hardware results exist for Windows 11 and NVIDIA
- Performance claims are backed by repeatable benchmark data

## Recommended Execution Order

1. Implement `DXGI duplication` backend and runtime backend selection.
2. Move ROI resize and preprocessing fully onto D3D11.
3. Remove inference-path CPU readback and keep preview/debug readback optional.
4. Harden YOLO parsing against the actual model outputs.
5. Add smoke tests and performance benchmarks on real Windows hardware.
