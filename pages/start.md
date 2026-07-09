# 시작하기

SmartScreenCapture 앱(`display-share`)을 Windows에서 빌드·실행하는 방법입니다.

## 요구 사항

| 구분 | 내용 |
|------|------|
| OS | Windows 10 또는 11 |
| 툴체인 | [Rust](https://rustup.rs/) stable (`edition = "2024"`) |
| 링커 | MSVC (Visual Studio Build Tools — C++ 데스크톱 개발) |
| GPU | DirectML 가능 GPU 권장 (없으면 CPU EP soft-fail) |
| 선택 | [OpenVINO Runtime](https://docs.openvino.ai/) — Intel GPU/NPU |
| 선택 | YOLO 사용 시 `models/yolo26n.onnx` |

## 클론

```powershell
git clone https://github.com/NAMUORI00/display-share.git
cd display-share
git switch main
```

## (선택) YOLO 모델

ONNX는 git에 없습니다 (`models/*.onnx` ignore).

```powershell
.\scripts\download_yolo26n.ps1
```

또는:

```text
https://github.com/ultralytics/assets/releases/download/v8.4.0/yolo26n.onnx
→ models/yolo26n.onnx
```

클래스 목록 `models/coco_classes.txt` 는 저장소에 포함됩니다.

## 빌드

저장소 **루트**에서:

```powershell
cargo check --manifest-path capture-first/Cargo.toml --workspace
cargo build --manifest-path capture-first/Cargo.toml -p display-share
cargo build --manifest-path capture-first/Cargo.toml -p display-share --release
```

릴리스 바이너리 예:

```text
capture-first/target/release/display-share.exe
```

## 실행

앱은 리포 루트의 `config/config.json` 을 찾습니다. **루트에서 실행**하세요.

```powershell
cargo run --manifest-path capture-first/Cargo.toml -p display-share
# release
cargo run --manifest-path capture-first/Cargo.toml -p display-share --release
```

로그 (기본은 quiet / `error` 수준):

```powershell
$env:RUST_LOG = "info"
cargo run --manifest-path capture-first/Cargo.toml -p display-share
```

## 첫 실행 체크

1. 창 제목 **SmartScreenCapture** 로 기동  
2. 디스플레이 목록 열거  
3. Start → 모니터 미리보기 갱신  
4. (선택) HSV / YOLO 토글 — YOLO는 모델 파일 필요  
5. 설정 저장 → `config/config.json` 반영  

## 설정 파일

| 경로 | 역할 |
|------|------|
| `config/config.json` | 메인 설정 (`privacy`, 비전, 성능 …) |
| `config/hsv_settings.json` | HSV 피커 보조 |
| `models/yolo26n.onnx` | YOLO26 detect |
| `models/coco_classes.txt` | COCO 클래스명 |

필드 상세: [설정](설정.md)

## 문제 해결

| 증상 | 확인 |
|------|------|
| 설정/모델 못 찾음 | 저장소 루트에서 실행했는지 |
| YOLO 초기화 실패 | `models/yolo26n.onnx` 존재, [모델 가이드](모델-가이드.md) |
| OpenVINO 안 뜸 | Runtime 설치 여부 — 없으면 다음 EP로 soft-fail |
| 캡처 실패 | 디스플레이/GPU 드라이버, DXGI 세션 권한 |

## 다음

- [재현 가이드](재현-가이드.md) · [아키텍처](아키텍처.md) · [파이프라인](파이프라인.md)
