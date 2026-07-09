---
title: 재현-가이드
---

# 재현 가이드

이 문서는 제3자가 **동일 환경에서 빌드·테스트·기본 동작을 재현**하기 위한 절차입니다.

## 1. 환경 고정

| 항목 | 권장 값 |
|------|---------|
| OS | Windows 11 (Windows 10도 가능) |
| Rust | `rustup` stable 최신 |
| 셸 | PowerShell 5.1+ 또는 PowerShell 7 |
| 저장소 커밋 | 재현 시 `git rev-parse HEAD` 를 기록 |

```powershell
rustc --version
cargo --version
git rev-parse HEAD
```

## 2. 소스 준비

```powershell
git clone https://github.com/NAMUORI00/display-share.git
cd display-share
git switch main
git pull
```

문서만 필요하면:

```powershell
git fetch origin wiki
git show wiki:재현-가이드.md
```

## 3. 자산 준비

```powershell
# YOLO (선택 — 추론 기능 재현 시 필요)
.\scripts\download_yolo26n.ps1

# 존재 확인
Test-Path models\yolo26n.onnx
Test-Path models\coco_classes.txt
Test-Path config\config.json
```

기본 설정에서 YOLO/HSV는 꺼져 있을 수 있습니다 (`enabled: false`).  
추론 재현 시 `config/config.json`에서 `vision_algorithms.yolo26_detection.enabled` 를 `true` 로 바꿉니다.

## 4. 빌드 재현

```powershell
cargo build --manifest-path capture-first/Cargo.toml -p display-share --release
```

성공 시 바이너리 예:

```text
capture-first/target/release/display-share.exe
```

## 5. 자동 테스트 재현

```powershell
cargo test --manifest-path capture-first/Cargo.toml --workspace
```

주요 단위·통합 테스트 영역:

| 크레이트 | 예시 검증 |
|----------|-----------|
| `capture-core` | privacy sanitize 기본값 |
| `capture-windows` | DXGI resolve, smoke 캡처 |
| `config` | JSON merge, runtime bindings |
| `inference-dml` | provider 정규화, YOLO 출력 파싱 |
| `pipeline` | ROI vs model input, headless HSV 경로 |
| `vision-gpu` | HSV 검출 회귀 |

> 실기 디스플레이 캡처 smoke 테스트는 GPU/디스플레이 상태에 따라 환경 의존적일 수 있습니다.

## 6. 수동 스모크 (운영자 경로)

1. 저장소 루트에서 `display-share` 실행  
2. UI에서 캡처 대상(디스플레이) 열거 확인  
3. Start → 미리보기/모니터 뷰 갱신 확인  
4. (선택) HSV 또는 YOLO 활성화 후 처리 시간·provider 상태 확인  
5. 설정 저장 후 `config/config.json` 반영 확인  
6. Stop 후 정상 종료

체크리스트:

- [ ] `cargo test --workspace` 통과
- [ ] 앱이 크래시 없이 기동
- [ ] 디스플레이 캡처 프레임 수신
- [ ] (모델 있을 때) YOLO 세션 생성 또는 soft-fail 사유가 UI/로그에 표시
- [ ] 설정 저장 round-trip

## 7. 재현 보고서에 넣을 것

다른 사람에게 결과를 공유할 때 아래를 함께 적으면 좋습니다.

```text
- commit: <sha>
- OS / GPU / 드라이버:
- rustc:
- OpenVINO 설치 여부:
- yolo26n.onnx 출처 (스크립트 / 자체 export):
- cargo test 결과:
- 수동 스모크 메모 (해상도, FPS, drop):
```

## 8. 알려진 한계

- 저장소에 **ONNX 바이너리는 커밋되지 않음** — 별도 다운로드 필요
- GPU 전처리(리사이즈·색 변환) 제로카피 경로는 아직 미완 (CPU preprocess 경로 사용)
- 4K60 / 1440p240 벤치마크 수치는 아직 공식 재현 데이터로 고정되어 있지 않음  
  → 상세는 [개발-현황](개발-현황.md)

## 관련 페이지

- [시작하기](시작하기.md)
- [모델-가이드](모델-가이드.md)
- [설정](설정.md)
