---
title: 모델-가이드
---

# 모델 가이드

`main` 브랜치의 [`models/README.md`](https://github.com/NAMUORI00/display-share/blob/main/models/README.md) 와 동일한 주제를 위키에서 정리한 페이지입니다.

## 지원 범위

| 항목 | 값 |
|------|-----|
| 모델 | YOLO26 **detect** |
| 입력 | 640×640 권장 |
| 배치 | `batch=1` |
| 백엔드 | DirectML → OpenVINO → CPU |

포함하지 않음: segmentation / pose / tracking head.

## 권장 파일 구조

```text
models/
├── README.md
├── yolo26n.onnx      # gitignore
└── coco_classes.txt  # 추적됨
```

## 준비 방법

### A. 공식 ONNX 다운로드 (권장)

```powershell
# 저장소 루트 (main)
.\scripts\download_yolo26n.ps1
```

URL:

```text
https://github.com/ultralytics/assets/releases/download/v8.4.0/yolo26n.onnx
```

### B. 로컬 export

```bash
pip install ultralytics
yolo export model=yolo26n.pt format=onnx imgsz=640 batch=1 dynamic=False
```

입력 크기·배치·출력 레이아웃이 달라지면 런타임이 거부할 수 있습니다.  
가능하면 프로젝트에서 검증한 export 계약 하나만 유지하세요.

## 설정 연동

`config/config.json`:

```json
"yolo26_detection": {
  "enabled": true,
  "onnx_model_path": "models/yolo26n.onnx",
  "class_names_path": "models/coco_classes.txt",
  "input_size": [640, 640],
  "execution_providers": ["directml", "openvino", "cpu"],
  "selected_gpu_id": 0,
  "openvino_device_type": null
}
```

## GPU / NPU

- 앱 하나당 디바이스 하나 (`selected_gpu_id` / OpenVINO `device_type`)
- provider 실패 시 다음으로 soft-fail
- 멀티 GPU 추론 분산 없음
- OpenVINO는 런타임이 PATH/설치되어 있어야 EP 사용 가능

## 문제 해결

| 증상 | 확인 |
|------|------|
| 모델 초기화 실패 | 경로, ONNX 계약(640/batch1), ort/DirectML |
| GPU 미선택 | `selected_gpu_id`, OpenVINO `device_type`, UI provider 상태 |
| 클래스 오류 | `coco_classes.txt` 순서·존재 |

## 라이선스

모델 가중치/ONNX는 Ultralytics 등 **별도 라이선스**일 수 있습니다.  
→ [라이선스](라이선스.md)
