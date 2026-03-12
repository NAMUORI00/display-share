# SmartScreenCapture

실시간 화면 캡처, 중심 ROI 처리, HSV 검출, YOLO26 기반 객체 검출을 제공하는 Windows 우선 C++ 애플리케이션입니다.

## 현재 범위

- 화면 캡처: `screen_capture_lite` 기반 실시간 캡처
- GUI: ImGui 기반 설정 및 상태 표시
- 색상 검출: OpenCV HSV 파이프라인
- 객체 검출: `YOLO26 + ONNX Runtime`
- 실행 정책: `CUDA Execution Provider` 우선, 실패 시 `CPU Execution Provider` fallback
- GPU 선택: 앱 인스턴스당 GPU 1개 선택 지원

다음 기능은 문서 범위에서 약속하지 않습니다.

- 앱 내부 멀티 GPU 로드밸런싱
- YOLO26 segmentation / pose / tracking head 지원

## YOLO26 실행 모델

이 프로젝트는 YOLO 경로를 `YOLO26 ONNX Runtime` 기준으로 정리합니다.

- 대상 플랫폼: Windows + NVIDIA CUDA GPU
- 기본 우선순위: `CUDA -> CPU fallback`
- 세션 생성 실패 또는 CUDA provider 문제 발생 시 CPU 세션으로 재생성
- 사용자는 GUI에서 GPU를 선택할 수 있으며, 선택 단위는 `앱당 1개 GPU`입니다
- 다중 GPU 시스템에서는 여러 앱 인스턴스를 서로 다른 GPU에 배치하는 사용 시나리오를 가정합니다

## 모델 계약

문서는 다음 계약만 전제로 합니다.

- 모델 형식: `YOLO26 detect ONNX`
- 배치: `batch=1`
- 입력 크기: `640x640`
- 후처리: NMS-free end-to-end export 기준

지원하지 않는 출력 계약은 런타임에서 실패 처리될 수 있습니다.

## 빌드 전제

핵심 의존성은 소스 트리의 서브모듈로 관리합니다. YOLO26 추론은 pinned ONNX Runtime GPU 패키지를 사용하며, 기본 CMake 설정은 패키지가 없으면 configure 단계에서 자동으로 내려받습니다.

주요 구성:

- `external/opencv`
- `external/imgui`
- `external/googletest`
- `external/nlohmann_json`
- `external/screen_capture_lite`
- `external/screen_capture_lite/glfw`
- build 디렉토리 아래 `_deps/onnxruntime/win-x64-gpu` 기본 경로 또는 사용자가 지정한 `ORT_ROOT`

자동 다운로드를 끄려면 `-DSC_AUTO_DOWNLOAD_ONNXRUNTIME=OFF`와 함께 `-DORT_ROOT=...`를 지정하면 됩니다.

## ONNX Runtime 패키징 가정

배포와 로컬 실행은 다음 가정을 둡니다.

- ONNX Runtime GPU 패키지는 pinned Windows x64 바이너리를 사용
- 필요한 `onnxruntime` DLL과 CUDA provider 관련 DLL을 실행 파일 옆에 함께 배치
- CUDA가 사용 가능하면 GPU로 실행하고, provider 초기화가 실패하면 CPU fallback으로 계속 동작
- 문서상 지원 범위는 `ONNX Runtime GPU bundle` 기반입니다

구체적인 모델 파일 배치는 [`models/README.md`](/c:/Users/yskim/Project/C_capture/ScreenMonitor/models/README.md)를 참조하세요.

## 빌드

```powershell
cd ScreenMonitor
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

주의:

- Windows 우선 문서입니다
- 첫 configure 시 ONNX Runtime GPU 번들을 자동 다운로드할 수 있습니다
- CUDA fallback은 "GPU 실패 시 CPU로 계속 실행"을 의미하며, CPU 모드 성능은 별도로 보장하지 않습니다

## 실행

```powershell
.\build\bin\SmartScreenCapture.exe
```

GUI에서 확인할 수 있어야 하는 항목:

- YOLO26 활성화 여부
- ONNX 모델 경로
- 클래스 파일 경로
- 선택된 GPU
- 현재 provider 상태
- CPU fallback 여부
- 마지막 오류 메시지

## 설정

YOLO 설정 섹션은 `vision_algorithms.yolo26_detection` 하나만 사용합니다.

예시:

```json
{
  "vision_algorithms": {
    "selected_algorithm": "yolo26",
    "yolo26_detection": {
      "enabled": true,
      "onnx_model_path": "models/yolo26n.onnx",
      "class_names_path": "models/coco_classes.txt",
      "confidence_threshold": 0.25,
      "max_detections": 100,
      "input_size": [640, 640],
      "execution_providers": ["cuda", "cpu"],
      "selected_gpu_id": 0
    }
  }
}
```

## 프로젝트 메모

- 이 문서는 현재 전환 방향에 맞춘 운영 가이드입니다
- 레거시 YOLO 설명은 의도적으로 제거했습니다
- 구현이 추가되더라도 문서 범위 밖 기능은 지원 약속으로 간주하지 않습니다
