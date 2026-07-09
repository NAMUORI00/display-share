# 아키텍처

Windows 우선 화면 캡처 · 비전 추론 앱 **display-share** 의 전체 구조입니다.  
관련: [파이프라인](파이프라인.md) · [설계안](설계안.md) · [개선점](개선점.md)

## 1. 한 줄 요약

```text
DXGI 캡처 스레드(D3D11) → CPU Preview/Analysis 채널 → UI 펌프 + 비전 워커(HSV/YOLO) → egui
```

- **D3D11은 캡처 스레드에만** 머문다. UI/비전 워커로 디바이스·텍스처를 넘기지 않는다.
- 분석은 캡처 스레드가 만든 **ROI CPU 버퍼(`AnalysisFrame`)** 만 비전 워커에 제출한다.
- 추론은 `InferenceBackend` 계약 뒤에서 provider 체인(DirectML → OpenVINO → CPU)으로 soft-fail 한다.

## 2. 시스템 컨텍스트

```mermaid
flowchart TB
  subgraph Operator["운영자"]
    UI["egui / eframe 셸<br/>display-share"]
  end

  subgraph Host["Windows 10/11"]
    DISP["디스플레이 / Desktop"]
    D3D["D3D11 + DXGI Desktop Duplication"]
    DML["DirectML"]
    OV["OpenVINO Runtime<br/>(선택)"]
    CPU["CPU EP"]
  end

  subgraph Assets["로컬 자산"]
    CFG["config/config.json"]
    ONNX["models/yolo26n.onnx"]
    CLS["models/coco_classes.txt"]
  end

  UI -->|Start/Stop · 설정 저장| CFG
  UI -->|열거 · 세션| D3D
  D3D -->|복제 프레임| DISP
  UI -->|LoadModel / Infer| ONNX
  ONNX --> DML
  ONNX --> OV
  ONNX --> CPU
  CLS --> UI
```

## 3. 워크스페이스 · 크레이트 경계

```mermaid
flowchart LR
  APP["apps/smartscreencapture<br/>display-share"]

  APP --> UI["ui"]
  APP --> CW["capture-windows"]
  APP --> PIPE["pipeline"]
  APP --> INF["inference-dml"]
  APP --> TEL["telemetry"]
  APP --> CFG["config"]

  CW --> CORE["capture-core"]
  CW --> G11["graphics-d3d11"]
  CW --> VG["vision-gpu"]
  PIPE --> CORE
  PIPE --> VG
  PIPE --> CFG
  INF --> CORE
  UI --> CORE
  UI --> CFG
  TEL --> CORE
  CFG --> CORE
  G11 --> CORE
  VG --> CORE
```

| 크레이트 | 책임 | 의도적으로 하지 않는 것 |
|----------|------|-------------------------|
| `capture-core` | 계약·타입·에러·identity | D3D11 / ort 구현 |
| `graphics-d3d11` | 텍스처 핸들, 공유 디바이스 | 캡처 세션 정책 |
| `capture-windows` | DXGI 열거·세션·미리보기/분석 채널 | UI, 추론 |
| `pipeline` | ROI/ModelInput, HSV/YOLO 라우팅, readback 정책 | 스레드 소유 |
| `vision-gpu` | HSV, CPU NCHW preprocess, crop/downscale | 세션 생명주기 |
| `inference-dml` | ort 세션, EP 체인, YOLO 파싱 | 캡처 |
| `config` | JSON ↔ `RuntimeBindings` | 런타임 루프 |
| `telemetry` | FPS/drop 집계 | 로깅 백엔드 |
| `ui` | 운영자 셸 모델·레이아웃 | 캡처 펌프 세부 |
| `display-share` | eframe 셸, 스레드 연결, 창 제외 | 비즈니스 로직 비대화 방지 |

## 4. 런타임 스레드 모델

```mermaid
sequenceDiagram
  participant UI as UI thread<br/>(eframe ~60Hz)
  participant CAP as Capture thread<br/>(DXGI + D3D11)
  participant VW as Vision worker<br/>(CPU path)

  UI->>CAP: start(session, options)
  loop 캡처 루프
    CAP->>CAP: AcquireNextFrame / copy
    CAP-->>UI: PreviewFrame (CPU ColorImage channel)
    CAP-->>UI: AnalysisFrame (ROI CPU)
    Note over CAP: GPU packet 채널은<br/>FPS 집계용으로만 drain
  end
  UI->>VW: try_submit(AnalysisFrame, settings)
  VW->>VW: process_analysis_roi<br/>HSV + YOLO
  VW-->>UI: WorkerEvent::Frame
  UI->>UI: TelemetryHub + UiModel 갱신
```

### 스레드 규칙 (불변 조건)

| 규칙 | 이유 |
|------|------|
| D3D11 디바이스/텍스처는 캡처 스레드 전용 | 크로스 스레드 D3D11 사용 금지 |
| 비전 워커는 `process_analysis_roi` 만 사용 | D3D11 touch 없음 |
| UI는 GPU `CapturePacket` 을 워커에 넘기지 않음 | FPS 카운트만 하고 폐기/스킵 |
| 미리보기 readback은 캡처 스레드에서 생산 | 워커에서 `want_preview_buffer=false` 강제 |

## 5. 캡처 서브시스템

```mermaid
flowchart TB
  EN["enumerate_targets<br/>DXGI outputs + Win32 메타"]
  RES["resolve_backend<br/>DXGI-only 경로"]
  START["DxgiDuplicationBackend::start"]
  SESS["CaptureSession"]
  DEV["SharedD3d11Device"]

  EN --> RES --> START --> SESS
  DEV --> SESS

  SESS --> CH1["try_recv → CapturePacket<br/>(GPU 프레임, 통계)"]
  SESS --> CH2["try_recv_preview → PreviewFrame"]
  SESS --> CH3["try_recv_analysis → AnalysisFrame"]
```

- 현재 구현은 **DXGI Desktop Duplication only** (`capture-windows` 주석 기준).
- `CaptureBackendPreference::WindowsGraphicsCapture` 를 줘도 resolve 결과는 DXGI 로 정규화된다.
- `CaptureOptions::normalize_for_display_capture` / privacy sanitize 로 지원 범위 밖 옵션을 강제한다.

## 6. 비전 · 추론 계층

```mermaid
flowchart LR
  AF["AnalysisFrame<br/>ROI BGRA CPU"]
  HSV["vision-gpu<br/>HSV mask / tune"]
  PRE["cpu_preprocess_nchw<br/>→ ModelInput 640²"]
  INF["inference-dml<br/>ort Session"]
  OUT["DetectionResult[]<br/>HsvMaskStats"]

  AF --> HSV --> OUT
  AF --> PRE --> INF --> OUT
```

### Provider 체인

```mermaid
stateDiagram-v2
  [*] --> Uninitialized
  Uninitialized --> DirectML: try EP
  DirectML --> Ready: commit OK
  DirectML --> OpenVINO: soft-fail
  OpenVINO --> Ready: commit OK
  OpenVINO --> CPU: soft-fail
  CPU --> Ready: commit OK
  CPU --> Failed: all fail
  Ready --> [*]
  Failed --> [*]
```

기본 순서: `["directml", "openvino", "cpu"]` (`auto`/빈 목록도 동일 확장).

## 7. 설정 · 제품 identity

```mermaid
flowchart TB
  JSON["config/config.json"]
  HSVJ["hsv_settings.json"]
  AC["AppConfig"]
  RB["RuntimeBindings"]
  CC["ConcealmentConfig"]

  JSON --> AC
  HSVJ --> AC
  AC --> RB
  AC --> CC
  RB --> FPS["target_fps / buffer_depth"]
  RB --> VIS["hsv + inference settings"]
  CC --> TITLE["window_title / WDA_EXCLUDEFROMCAPTURE"]
```

## 8. 데이터 타입 핵심

| 타입 | 의미 |
|------|------|
| `CaptureFrame::{D3d11,Cpu}` | GPU/CPU 프레임 (downcast 웹 제거) |
| `CaptureRoi` | 운영자 ROI (예: 320×320 center) — **모델 입력과 분리** |
| `ModelInputSize` | YOLO 입력 (예: 640×640) |
| `AnalysisFrame` | 캡처 스레드 생산 ROI CPU 버퍼 |
| `PreviewFrame` | 모니터 뷰용 다운스케일 CPU |
| `TensorInputHandle` | preprocess 결과(+ optional `CpuNchwTensor`) |
| `ProviderState` | UI에 노출되는 EP 상태 |

## 9. 현재 vs 목표 아키텍처

```mermaid
flowchart TB
  subgraph Now["현재 (As-Is)"]
    N1["DXGI GPU texture"] --> N2["ROI CPU readback"]
    N2 --> N3["CPU NCHW preprocess"]
    N3 --> N4["ort infer"]
    N1 --> N5["Preview CPU readback"]
  end

  subgraph Goal["목표 (To-Be) Track 2"]
    G1["DXGI GPU texture"] --> G2["GPU ROI crop"]
    G2 --> G3["GPU resize + color convert"]
    G3 --> G4["DirectML IO binding / GPU tensor"]
    G1 -.->|optional| G5["Preview readback only"]
  end
```

상세 설계는 [설계안](설계안.md), 갭 목록은 [개선점](개선점.md), 프레임 단위 흐름은 [파이프라인](파이프라인.md)을 보세요.
