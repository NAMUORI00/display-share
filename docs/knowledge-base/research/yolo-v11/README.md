# YOLO v11 Research Materials

YOLO v11 객체 검출 모델의 최적화 및 TensorRT 통합에 대한 전문 자료 모음

## 📋 수록 자료

### 🎯 최적화 기법
- **optimization-techniques.md** - YOLO v11 모델 최적화 종합 가이드
- **performance-benchmarks.md** - 다양한 하드웨어 환경에서의 성능 벤치마크
- **best-practices.md** - 프로덕션 환경 배포 베스트 프랙티스

### ⚡ TensorRT 통합
- **tensorrt-integration.md** - ONNX → TensorRT 엔진 변환 및 최적화
- **engine-optimization.md** - TensorRT 엔진 최적화 고급 기법
- **memory-management.md** - GPU 메모리 효율적 관리 방법

## 🎯 핵심 성능 목표
- **추론 속도**: 320x320 입력에서 < 10ms (RTX 3080 기준)
- **메모리 사용량**: GPU 메모리 < 2GB
- **정확도 유지**: mAP@0.5 > 0.85 (원본 모델 대비 95% 이상)

## 🔍 검색 키워드
- `yolo-v11`, `object-detection`, `tensorrt`, `optimization`
- `inference-speed`, `model-quantization`, `onnx-conversion`
- `gpu-memory`, `batch-processing`, `real-time-detection`