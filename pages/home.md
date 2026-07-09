---
title: 홈
nav_order: 1
---

# display-share 문서

Windows 우선 **화면 캡처 + 비전 파이프라인** 프로젝트 문서 사이트입니다.  
(저장소 이력명: `C_capture` · 앱 바이너리: `display-share`)

| | |
|--|--|
| 저장소 | [NAMUORI00/display-share](https://github.com/NAMUORI00/display-share) |
| 소스 브랜치 | [`main`](https://github.com/NAMUORI00/display-share/tree/main) |
| 문서 브랜치 | [`wiki`](https://github.com/NAMUORI00/display-share/tree/wiki) |
| 앱 바이너리 | `display-share` (`capture-first/apps/smartscreencapture`) |
| 소스 라이선스 | [MIT](라이선스.md) |

## 무엇을 하나요?

- **디스플레이 캡처**: Windows DXGI Desktop Duplication (WGC 경로 포함·해석은 DXGI 중심)
- **비전**: HSV 마스크 추적, YOLO26 detect ONNX 추론
- **추론 백엔드**: DirectML → OpenVINO → CPU (`ort` EP, soft-fail)
- **UI**: `eframe` / `egui` 데스크톱 셸

## 문서 목차

1. [시작하기](시작하기.md) — 설치 · 빌드 · 실행
2. [재현 가이드](재현-가이드.md) — 재현 환경 · 검증 체크리스트
3. [라이선스](라이선스.md) — MIT 및 서드파티(모델·런타임)
4. [아키텍처](아키텍처.md) — 크레이트 경계 · 파이프라인
5. [설정](설정.md) — `config/config.json` 필드
6. [모델 가이드](모델-가이드.md) — YOLO26 ONNX 준비
7. [개발 현황](개발-현황.md) — 마이그레이션 트랙 요약

## 로컬에서 문서 소스 보기

```powershell
git fetch origin wiki
git switch wiki
# 또는
git show wiki:index.md
```

## main 브랜치의 짧은 문서

| 경로 | 내용 |
|------|------|
| `README.md` | 프로젝트 개요 · 빠른 시작 |
| `LICENSE` | MIT 전문 |
| `models/README.md` | 모델 파일 배치 |
| `capture-first/README.md` | 워크스페이스 영문 요약 |
| `capture-first/MIGRATION_TASKS.md` | 구현 백로그 원문 |
