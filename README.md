# display-share

Windows 우선 **화면 캡처 + 비전 파이프라인** 프로젝트입니다.  
Rust 워크스페이스 `capture-first` 위에서 디스플레이 캡처(DXGI Desktop Duplication), HSV 추적, YOLO26 ONNX 추론, egui 데스크톱 셸을 제공합니다.

| 항목 | 내용 |
|------|------|
| 앱 바이너리 | `smartscreencapture` (`capture-first/apps/smartscreencapture`) |
| 플랫폼 | Windows 10/11 (D3D11, DirectML, 선택적 OpenVINO) |
| 라이선스 | [MIT](./LICENSE) (앱 소스) |
| 문서 사이트 | **[GitHub Pages](https://NAMUORI00.github.io/display-share/)** (`wiki` 브랜치) |
| 문서 소스 | [`wiki` 브랜치](../../tree/wiki) |

> 이전 저장소 이름: `C_capture` (이력·로컬 폴더명에 남을 수 있음)

## 빠른 시작

### 요구 사항

- Windows 10/11
- [Rust](https://rustup.rs/) (stable, edition 2024 지원 툴체인)
- Visual Studio Build Tools (C++ 워크로드) 또는 동등한 MSVC 링커
- (선택) [OpenVINO Runtime](https://docs.openvino.ai/) — Intel GPU/NPU EP
- (선택) YOLO 추론 시 `models/yolo26n.onnx`

### 빌드 · 실행

```powershell
# 저장소 루트에서
git clone https://github.com/NAMUORI00/display-share.git
cd display-share

# (선택) YOLO26n ONNX 다운로드
.\scripts\download_yolo26n.ps1

# 앱 실행 (debug)
cargo run --manifest-path capture-first/Cargo.toml -p smartscreencapture

# 릴리스
cargo run --manifest-path capture-first/Cargo.toml -p smartscreencapture --release
```

설정은 저장소 루트의 `config/config.json`, 클래스 라벨은 `models/coco_classes.txt`를 사용합니다.  
재현 절차·아키텍처·라이선스 세부 사항은 [문서 사이트](https://NAMUORI00.github.io/display-share/)를 보세요.

### 테스트

```powershell
cargo test --manifest-path capture-first/Cargo.toml --workspace
```

## 저장소 구조

```text
display-share/
├── capture-first/          # Rust workspace (앱 + crates)
│   ├── apps/smartscreencapture/   # smartscreencapture 바이너리
│   └── crates/             # capture-core, capture-windows, inference-dml, …
├── config/                 # config.json, hsv_settings.json
├── models/                 # ONNX · 클래스 파일 (*.onnx 는 gitignore)
├── scripts/                # 모델 다운로드 등
├── testdata/               # 비전 회귀용 샘플 이미지
├── LICENSE
└── README.md
```

## 문서

긴 설명은 **소스와 분리된 `wiki` 브랜치**에 두고, GitHub Pages로 배포합니다.

| 문서 | 링크 |
|------|------|
| 홈 | https://NAMUORI00.github.io/display-share/ |
| 아키텍처 | https://NAMUORI00.github.io/display-share/#arch |
| 파이프라인 | https://NAMUORI00.github.io/display-share/#pipeline |
| 설계안 | https://NAMUORI00.github.io/display-share/#design |
| 개선점 | https://NAMUORI00.github.io/display-share/#improvements |
| 시작하기 | https://NAMUORI00.github.io/display-share/#start |
| 재현 가이드 | https://NAMUORI00.github.io/display-share/#repro |
| 설정 | https://NAMUORI00.github.io/display-share/#config |
| 모델 가이드 | https://NAMUORI00.github.io/display-share/#model |
| 라이선스 | https://NAMUORI00.github.io/display-share/#license |
| 개발 현황 | https://NAMUORI00.github.io/display-share/#status |

로컬에서 wiki 소스만 보려면:

```powershell
git fetch origin wiki
git switch wiki
# 또는
git show wiki:index.md
```

## 라이선스

- **앱 소스 코드**: [MIT License](./LICENSE)
- **YOLO26 / Ultralytics 모델 자산**: 별도 조건(AGPL-3.0 또는 상업 라이선스 등)이 적용될 수 있습니다. 배포 전 [모델 가이드](./models/README.md)와 [라이선스 문서](https://NAMUORI00.github.io/display-share/#license)를 확인하세요.
- **ONNX Runtime / DirectML / OpenVINO**: 각 벤더 배포 라이선스를 따릅니다.

## 관련 문서 (main 브랜치)

- [`models/README.md`](./models/README.md) — 모델 배치·다운로드
- [`capture-first/README.md`](./capture-first/README.md) — 워크스페이스 영문 요약
- [`capture-first/MIGRATION_TASKS.md`](./capture-first/MIGRATION_TASKS.md) — 구현 백로그
