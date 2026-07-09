---
title: 개발-현황
---

# 개발 현황

`main` 의 [`capture-first/MIGRATION_TASKS.md`](https://github.com/NAMUORI00/display-share/blob/main/capture-first/MIGRATION_TASKS.md) 요약입니다.  
체크박스 원문은 해당 파일을 기준으로 하세요.

## 완료된 큰 덩어리

- 레이어 분리: `graphics-d3d11`, `pipeline`, `CaptureFrame::{D3d11,Cpu}`
- Windows 캡처: `windows-capture` / DXGI 경로, 대상 메타데이터
- 추론: DirectML + CPU + OpenVINO EP 체인, YOLO 파싱·진단
- UI: 대상 열거, Start/Stop, ROI 미리보기, provider/성능 표시, JSON 저장
- 검증: `cargo check` / `cargo test`, headless 파이프라인 테스트

## 진행 중 · 미완

| 영역 | 항목 |
|------|------|
| Capture | OBS/libobs 어댑터, 보호/미지원 대상 fail-closed 강화 |
| Zero-copy | GPU resize·색변환, 셰이더 전처리, 추론 핫패스 CPU readback 제거 |
| Inference | 실제 배포 ONNX에 대한 출력 검증, 라이브 provider fallback 회귀 |
| UI | 텍스처 네이티브 프리뷰 (CPU 업로드 회피) |
| Verification | Win11 실기 스모크, 4K60 / 1440p240 벤치 |

## 현재 블로커(문서 시점)

- 저장소에 배포용 `.onnx` 가 없어 **실모델 출력 검증**은 로컬 자산 배치 후 진행
- 성능 수치 클레임은 반복 가능한 벤치 데이터가 쌓이기 전까지 보수적으로 해석

## 권장 구현 순서 (백로그)

1. DXGI/백엔드 선택 고도화 잔여 작업  
2. ROI resize·preprocess 를 D3D11으로  
3. 추론 경로 CPU readback 제거 (프리뷰/디버그는 선택)  
4. 실제 모델 출력에 맞춘 YOLO 파싱 하드닝  
5. 실기 스모크·벤치마크

## 관련 페이지

- [아키텍처](아키텍처.md)
- [재현-가이드](재현-가이드.md)
