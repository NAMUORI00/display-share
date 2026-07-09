# capture-first

Rust workspace for the Windows-first SmartScreenCapture app.

Current scope:

- Zero-copy oriented capture contracts
- Windows capture backend using DXGI Desktop Duplication
- Multi-vendor ONNX inference via `ort`: **DirectML → OpenVINO → CPU** (config `execution_providers`)
- Intel Gram (Arc iGPU / NPU) via OpenVINO; NVIDIA/AMD/Intel via DirectML; CPU fallback
- `eframe/egui` desktop shell
- JSON config under `config/config.json` and class labels under `models/`
- Optional own-window capture exclusion is off by default and logged when enabled

## Inference providers

Default order in `vision_algorithms.yolo26_detection.execution_providers`:

```json
["directml", "openvino", "cpu"]
```

- Use `"auto"` (or empty) to expand to the default order.
- Optional `openvino_device_type`: `"GPU"`, `"NPU"`, `"GPU.0"`, etc. When omitted, OpenVINO tries `GPU` then `NPU`.
- OpenVINO EP needs the [OpenVINO Runtime](https://docs.openvino.ai/latest/openvino_docs_install_guides_installing_openvino_from_archive_windows.html) installed and discoverable on the machine. If the EP is unavailable, initialization soft-fails to the next provider (no panic).

Remaining work is tracked in [MIGRATION_TASKS.md](./MIGRATION_TASKS.md).
