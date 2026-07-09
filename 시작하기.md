---
title: 시작하기
---

# 시작하기

## 요구 사항

| 구분 | 내용 |
|------|------|
| OS | Windows 10 또는 11 (캡처·D3D11·DirectML 경로) |
| 언어 툴체인 | [Rust](https://rustup.rs/) stable (workspace `edition = "2024"`) |
| 링커 | MSVC — Visual Studio Build Tools의 **C++ 데스크톱 개발** 권장 |
| GPU | DirectML 가능 GPU 권장 (없으면 CPU EP로 soft-fail) |
| 선택 | [OpenVINO Runtime](https://docs.openvino.ai/latest/openvino_docs_install_guides_installing_openvino_from_archive_windows.html) (Intel GPU/NPU) |
| 선택 | YOLO 사용 시 `models/yolo26n.onnx` |

## 클론

```powershell
git clone https://github.com/NAMUORI00/display-share.git
cd display-share
git switch main
```

## (선택) 모델 다운로드

ONNX는 git에 포함되지 않습니다 (`models/*.onnx` ignore).

```powershell
.\scripts\download_yolo26n.ps1
```

직접 받을 경우:

```text
https://github.com/ultralytics/assets/releases/download/v8.4.0/yolo26n.onnx
→ models/yolo26n.onnx
```

클래스 파일 `models/coco_classes.txt`는 저장소에 포함되어 있습니다.

## 빌드

```powershell
# 워크스페이스 체크
cargo check --manifest-path capture-first/Cargo.toml --workspace

# display-share 바이너리
cargo build --manifest-path capture-first/Cargo.toml -p display-share
cargo build --manifest-path capture-first/Cargo.toml -p display-share --release
```

## 실행

앱은 저장소 **루트**를 기준으로 `config/config.json`을 찾습니다.  
루트에서 실행하는 것을 권장합니다.

```powershell
cargo run --manifest-path capture-first/Cargo.toml -p display-share
# 또는 release
cargo run --manifest-path capture-first/Cargo.toml -p display-share --release
```

로그 레벨 (기본은 quiet / `error`):

```powershell
$env:RUST_LOG = "info"
cargo run --manifest-path capture-first/Cargo.toml -p display-share
```

## 설정 위치

| 파일 | 역할 |
|------|------|
| `config/config.json` | 메인 설정 (비전, 성능, GUI, concealment 등) |
| `config/hsv_settings.json` | HSV 피커 보조 설정 |
| `models/yolo26n.onnx` | YOLO26 detect 모델 |
| `models/coco_classes.txt` | 클래스 이름 (COCO 순서) |

필드 설명은 [설정](설정.md) 페이지를 보세요.

## 다음 단계

- 깨끗한 재현·검증: [재현-가이드](재현-가이드.md)
- 모델·EP 문제: [모델-가이드](모델-가이드.md)
- 코드 구조: [아키텍처](아키텍처.md)
