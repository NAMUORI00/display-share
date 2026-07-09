# 라이선스

## 앱 소스 코드

**MIT License**

- `main` 브랜치 [`LICENSE`](https://github.com/NAMUORI00/display-share/blob/main/LICENSE)
- Cargo workspace: `license = "MIT"` (`capture-first/Cargo.toml`)
- 바이너리: `display-share` 및 워크스페이스 crates

요약 (법적 효력은 영문 전문):

- 사용·복제·수정·배포·판매 허용
- 저작권·허가 고지 유지
- 무보증 (AS IS)

Copyright: `Copyright (c) 2026 NAMUORI00 and display-share contributors`

### 이 wiki 문서

별도 표기가 없으면 문서도 **MIT** 로 취급합니다.  
인용 시 저장소 URL과 `wiki` 브랜치/페이지를 밝혀 주세요.

---

## 서드파티

앱 소스가 MIT여도 **모델·런타임·의존성**은 다른 조건일 수 있습니다.

### YOLO26 / Ultralytics

| 항목 | 내용 |
|------|------|
| 예 | `yolo26n.onnx` |
| 주의 | AGPL-3.0 또는 별도 상업 라이선스 가능 |
| 권고 | 상업 배포 전 Ultralytics 조건을 직접 검토 |

모델은 git에 포함하지 않습니다. 사용자가 직접 다운로드/export 합니다.

### ONNX Runtime (`ort`)

- DirectML / OpenVINO feature 로 바이너리 연동
- 재배포 시 ONNX Runtime 고지 확인

### DirectML / OpenVINO

- Microsoft DirectML, Intel OpenVINO 각 배포 라이선스
- OpenVINO Runtime 미설치 시 soft-fail

### Rust crates

```powershell
cargo install cargo-license
cargo license --manifest-path capture-first/Cargo.toml
```

---

## 배포 체크리스트

- [ ] `LICENSE` (MIT) 포함
- [ ] ONNX 포함 시 Ultralytics 조건 적합
- [ ] ORT / DirectML / OpenVINO 재배포 고지
- [ ] `cargo license` 결과 보관
- [ ] 캡처 영상에 대한 개인정보 정책은 제품 정책으로 별도

이 문서는 안내이며 법률 자문이 아닙니다.
