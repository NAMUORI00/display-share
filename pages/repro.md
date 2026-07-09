# 재현 가이드

제3자가 **동일하게 빌드·테스트·기본 동작을 재현**하기 위한 절차입니다.

## 1. 환경 기록

| 항목 | 권장 |
|------|------|
| OS | Windows 11 (10 가능) |
| Rust | `rustup` stable |
| 셸 | PowerShell |
| 커밋 | `git rev-parse HEAD` 기록 |

```powershell
rustc --version
cargo --version
git rev-parse HEAD
```

## 2. 소스

```powershell
git clone https://github.com/NAMUORI00/display-share.git
cd display-share
git switch main
git pull
```

문서만:

```powershell
git fetch origin wiki
git show wiki:pages/repro.md
```

## 3. 자산

```powershell
.\scripts\download_yolo26n.ps1   # YOLO 재현 시
Test-Path models\yolo26n.onnx
Test-Path models\coco_classes.txt
Test-Path config\config.json
```

기본 설정에서 HSV/YOLO 는 `enabled: false` 일 수 있습니다.  
추론 재현 시 `vision_algorithms.yolo26_detection.enabled` 를 `true` 로 켭니다.

## 4. 빌드

```powershell
cargo build --manifest-path capture-first/Cargo.toml -p smartscreencapture --release
```

## 5. 자동 테스트

```powershell
cargo test --manifest-path capture-first/Cargo.toml --workspace
```

| 크레이트 | 검증 예 |
|----------|---------|
| `capture-core` | privacy/options sanitize 등 |
| `capture-windows` | DXGI resolve, smoke 캡처 |
| `config` | JSON merge, runtime bindings, privacy 기본값 |
| `inference-dml` | provider 정규화, YOLO 파싱 |
| `pipeline` | ROI vs model input, headless HSV |
| `vision-gpu` | HSV 회귀 |

> DXGI smoke 는 디스플레이/GPU 환경에 의존할 수 있습니다.

## 6. 수동 스모크

1. 루트에서 `smartscreencapture` 실행  
2. 디스플레이 열거  
3. Start → 미리보기 갱신  
4. (선택) HSV / YOLO — provider 상태 확인  
5. 설정 저장 round-trip  
6. Stop 후 정상 종료  

체크리스트:

- [ ] `cargo test --workspace` 통과  
- [ ] 크래시 없이 기동 (제목 SmartScreenCapture)  
- [ ] 디스플레이 프레임 수신  
- [ ] (모델 있을 때) YOLO 세션 또는 soft-fail 사유 표시  
- [ ] `privacy.exclude_own_windows_from_capture` 기본 false 동작 이해  
- [ ] 설정 저장 반영  

## 7. 재현 보고서 템플릿

```text
- commit:
- OS / GPU / 드라이버:
- rustc:
- OpenVINO 설치 여부:
- yolo26n.onnx 출처:
- cargo test:
- 수동 스모크 (해상도, FPS, drop):
```

## 8. 알려진 한계

- ONNX 는 저장소에 없음 → 별도 다운로드  
- 추론 핫패스에 CPU readback/preprocess 존재 (제로카피 미완)  
- 4K60 / 1440p240 공식 벤치 수치 없음  
- WGC 백엔드 미구현 (DXGI only)  

→ [개선점](개선점.md) · [개발 현황](개발-현황.md)

## 관련

- [시작하기](시작하기.md) · [설정](설정.md) · [모델 가이드](모델-가이드.md)
