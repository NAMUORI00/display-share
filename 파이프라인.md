# 파이프라인

캡처 → 분석 → 추론 → UI 반영 **프레임 단위 전체 흐름** (현재 구현 기준).

## 1. 엔드투엔드

```mermaid
flowchart TB
  subgraph Cap[캡처 스레드]
    A1[DXGI AcquireNextFrame]
    A2[D3D11 texture]
    A3[ROI crop + CPU<br/>AnalysisFrame]
    A4[Full-frame downscale<br/>PreviewFrame long-edge 1920]
    A5[CapturePacket 채널]
    A1 --> A2 --> A3
    A2 --> A4
    A2 --> A5
  end
  subgraph Ui[UI pump_capture]
    B1[drain GPU packets FPS]
    B2[drain Preview to ColorImage]
    B3[drain Analysis try_submit]
    B4[TelemetryHub snapshot]
  end
  subgraph Vw[비전 워커]
    C1[process_analysis_roi]
    C2[HSV]
    C3[cpu_preprocess_nchw]
    C4[infer]
    C5[WorkerEvent Frame]
    C1 --> C2 --> C5
    C1 --> C3 --> C4 --> C5
  end
  A5 --> B1
  A4 --> B2
  A3 --> B3 --> C1
  C5 --> B4
  B2 --> EG[egui]
  B4 --> EG
```

## 2. Stage 0 — 세션 기동

| 단계 | 위치 | 동작 |
|------|------|------|
| 0.1 | UI | `enumerate_targets` (DXGI outputs) |
| 0.2 | UI | `CaptureOptions` (fps, buffer_depth, preference) |
| 0.3 | capture-windows | normalize + resolve → **DXGI** |
| 0.4 | capture-windows | SharedD3d11Device + Duplication 세션 |
| 0.5 | UI | SessionFingerprint (옵션 변경 시 재시작) |
| 0.6 | UI | `privacy.exclude_own_windows` 가 true 면 창 제외 적용·로그 |

```mermaid
sequenceDiagram
  participant U as UI
  participant B as WindowsCaptureBackend
  participant S as CaptureSession
  U->>B: enumerate_targets
  B-->>U: CaptureTarget list
  U->>B: start target options
  B->>B: normalize DXGI only
  B->>S: create session
  S-->>U: CaptureSession
```

## 3. Stage 1 — 캡처 핫패스

```mermaid
flowchart LR
  F0[Desktop] --> F1[Duplication]
  F1 --> F2[D3D11 Texture]
  F2 --> F3{gates}
  F3 -->|always| F4[packet channel]
  F3 -->|preview on| F5[CPU preview]
  F3 -->|analysis on| F6[ROI CPU AnalysisFrame]
```

| 계약 | 값 |
|------|-----|
| Capture ROI 기본 | center max **320×320** (`pipeline::DEFAULT_CAPTURE_ROI`) |
| Preview long-edge (캡처) | **1920** (`dxgi.rs`) |
| 프레임 정책 | 최신 우선, 중간 프레임 skip/drop 집계 |

## 4. Stage 2 — UI 펌프

```mermaid
flowchart TB
  P[pump_capture] --> R[restart if fingerprint changed]
  P --> D[drain worker events]
  P --> G[sync session gates]
  P --> PK[try_recv packets]
  P --> PV[try_recv_preview]
  P --> AN[try_recv_analysis]
  AN --> SUB{try_submit}
  SUB -->|full| DROP[telemetry on_drop]
  SUB -->|ok| Q[vision queue]
  P --> TEL[performance snapshot]
```

중요: GPU packet 은 워커에 넣지 않음. Analysis 만.

## 5. Stage 3 — 비전 워커

`FramePipeline::process_analysis_roi` — **D3D11 비접촉**

```mermaid
flowchart TB
  IN[AnalysisFrame] --> OK{size gt 0}
  OK -->|no| EMPTY[empty report]
  OK -->|yes| HSVQ{hsv enabled}
  HSVQ -->|yes| HSV[detect_hsv_tune]
  HSVQ --> YQ
  HSV --> YQ{yolo ready}
  YQ -->|yes| NCHW[cpu_preprocess_nchw]
  NCHW --> INF[backend.infer]
  INF --> OUT[WorkerEvent Frame]
  YQ -->|no| OUT
```

워커는 항상:

```text
settings.want_preview_buffer = false
settings.preview_enabled = false
```

## 6. Stage 4 — 추론

```mermaid
flowchart TB
  T[CpuNchwTensor] --> S[Session run]
  S --> O[output tensor]
  O --> P[parse row or channel major]
  P --> F[confidence + max_detections]
  F --> R[DetectionResult list]
```

## 7. Stage 5 — UI 반영

| 입력 | 효과 |
|------|------|
| PreviewFrame | 모니터 ColorImage |
| WorkerEvent::Frame | HSV 오버레이, YOLO 박스, ms, provider |
| ModelReady | provider + diagnostics |
| Failed | last_error / 로그 |
| Telemetry | FPS, drop |

## 8. ROI vs Model Input

```mermaid
flowchart LR
  FULL[Full frame] --> ROI[CaptureRoi 320]
  ROI --> HSV[HSV on ROI]
  ROI --> PRE[CPU preprocess]
  PRE --> MI[ModelInput 640]
  MI --> YOLO[YOLO26]
```

이 분리가 깨지면 좌표 매핑·성능 측정이 틀어집니다.

## 9. 배압

```mermaid
flowchart LR
  CAP[빠른 캡처] --> Q1[analysis channel]
  Q1 --> UI[UI drain latest]
  UI --> Q2[worker bounded queue]
  Q2 -->|full| DROP[drop + telemetry]
  Q2 -->|ok| VW[worker]
```

## 10. 오류

| 원인 | 전파 |
|------|------|
| DXGI access lost | session error → UI disconnect |
| Model init fail | Failed 이벤트 또는 EP soft-fail |
| Infer/parse fail | last_error + diagnostics |
| Missing onnx | autoload skip / 메시지 |

## 관련

- [아키텍처](아키텍처.md) · [설계안](설계안.md) · [설정](설정.md)
