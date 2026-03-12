# YOLO26 모델 가이드

이 디렉토리는 `YOLO26 detect ONNX` 모델과 클래스 파일을 두는 위치입니다.

## 지원 범위

- 지원 모델: `YOLO26 detect`
- 권장 입력: `640x640`
- 권장 배치: `batch=1`
- 실행 백엔드: `ONNX Runtime CUDA -> CPU fallback`

다음 항목은 이 가이드 범위에 포함하지 않습니다.

- segmentation / pose / tracking head

## 권장 파일 구조

```text
models/
├── README.md
├── yolo26n.onnx
└── coco_classes.txt
```

필요하면 `yolo26s.onnx`, `yolo26m.onnx` 같은 추가 변형을 둘 수 있지만, 기본 문서와 설정 예시는 `yolo26n.onnx`를 기준으로 합니다.

## 모델 준비

Ultralytics가 제공하는 YOLO26 export 경로를 사용해 ONNX 파일을 준비합니다.

```bash
pip install ultralytics
yolo export model=yolo26n.pt format=onnx imgsz=640 batch=1 dynamic=False
```

가능하면 프로젝트에서 검증한 export 계약 하나만 유지하세요. 입력 크기, 배치, output 형식이 달라지면 런타임이 해당 모델을 거부할 수 있습니다.

## 배치 위치

생성된 ONNX 파일을 이 폴더에 두고 설정에서 경로를 맞춥니다.

```text
ScreenMonitor/models/yolo26n.onnx
ScreenMonitor/models/coco_classes.txt
```

기본 설정값:

- 모델 경로: `models/yolo26n.onnx`
- 클래스 파일: `models/coco_classes.txt`

## 클래스 파일

`coco_classes.txt`는 모델이 학습된 클래스 순서와 동일해야 합니다. COCO 계열 모델을 쓰면 일반적으로 80개 클래스 목록을 사용합니다.

클래스 파일이 누락되면 GUI 또는 초기화 단계에서 오류가 발생할 수 있습니다.

## GPU 사용 방식

이 프로젝트의 문서상 GPU 사용 정책은 단순합니다.

- 앱 하나는 GPU 하나만 선택
- 가능한 경우 CUDA provider로 실행
- 선택한 GPU에서 세션 생성에 실패하면 CPU fallback
- 앱 내부에서 여러 GPU에 추론을 분산하지 않음

다중 GPU 시스템에서는 각 앱 인스턴스가 서로 다른 GPU를 선택하는 사용 형태를 가정합니다.

## 문제 해결

### 모델 초기화 실패

- `yolo26n.onnx` 경로가 맞는지 확인
- CMake configure가 ONNX Runtime GPU 번들을 정상적으로 내려받았는지 확인
- CUDA provider DLL이 실행 파일 옆에 배치됐는지 확인
- 모델이 `detect ONNX / batch=1 / 640x640` 계약과 크게 다르지 않은지 확인

### GPU가 선택되지 않음

- NVIDIA GPU와 드라이버가 정상인지 확인
- ONNX Runtime GPU 패키지와 CUDA 런타임 버전 조합이 맞는지 확인
- 실패 시 CPU fallback으로 동작할 수 있으므로 provider 상태를 GUI에서 확인

## 라이선스 메모

YOLO26 모델 사용에는 Ultralytics 라이선스 조건이 적용될 수 있습니다. 상업 배포 전에는 AGPL-3.0 또는 별도 상업 라이선스 조건을 직접 검토하세요.
