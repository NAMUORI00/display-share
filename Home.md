# C_capture 위키

Windows 우선 화면 캡처 · 비전 파이프라인 프로젝트 **C_capture** 의 문서 홈입니다.

이 브랜치(`wiki`)에는 **위키 페이지만** 있습니다. 소스 코드·설정·모델 스크립트는 `main` 브랜치를 보세요.

| | |
|--|--|
| 저장소 | [NAMUORI00/C_capture](https://github.com/NAMUORI00/C_capture) |
| 소스 브랜치 | [`main`](https://github.com/NAMUORI00/C_capture/tree/main) |
| 앱 바이너리 | `display-share` (`capture-first/apps/smartscreencapture`) |
| 소스 라이선스 | MIT |

## 무엇을 하나요?

- **디스플레이 캡처**: Windows DXGI Desktop Duplication (WGC 경로 포함·해석은 DXGI 중심)
- **비전**: HSV 마스크 추적, YOLO26 detect ONNX 추론
- **추론 백엔드**: DirectML → OpenVINO → CPU (`ort` EP, soft-fail)
- **UI**: `eframe` / `egui` 데스크톱 셸 (운영자 콘솔, 미리보기, 설정 저장)

## 문서 목차

1. [시작하기](시작하기) — 도구 설치, 빌드, 실행
2. [재현-가이드](재현-가이드) — 깨끗한 재현 절차와 검증 체크리스트
3. [라이선스](라이선스) — MIT 및 서드파티(모델·런타임) 주의
4. [아키텍처](아키텍처) — 워크스페이스·크레이트·데이터 흐름
5. [설정](설정) — `config/config.json` 필드
6. [모델-가이드](모델-가이드) — YOLO26 ONNX 준비
7. [개발-현황](개발-현황) — 마이그레이션 트랙 요약

## 로컬에서 이 위키 보기

```powershell
git fetch origin wiki
git switch wiki
# 또는 main을 유지한 채
git show wiki:Home.md
```

GitHub에서 `wiki` 브랜치를 열면 마크다운 페이지를 바로 읽을 수 있습니다.

## main 브랜치의 짧은 문서

| 경로 | 내용 |
|------|------|
| `README.md` | 프로젝트 개요 · 빠른 시작 |
| `LICENSE` | MIT 전문 |
| `models/README.md` | 모델 파일 배치 |
| `capture-first/README.md` | 워크스페이스 영문 요약 |
| `capture-first/MIGRATION_TASKS.md` | 구현 백로그 원문 |
