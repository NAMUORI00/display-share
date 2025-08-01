# TensorRT Research Materials

NVIDIA TensorRT를 활용한 딥러닝 모델 추론 최적화 전문 자료

## 📋 수록 자료

### ⚡ 엔진 최적화
- **onnx-optimization.md** - ONNX 모델의 TensorRT 엔진 변환 최적화
- **fp16-int8-conversion.md** - 모델 양자화 및 정밀도 최적화
- **engine-tuning.md** - 하드웨어별 엔진 튜닝 가이드

### 🧠 메모리 관리
- **memory-management.md** - GPU 메모리 효율적 활용 방법
- **batch-processing.md** - 배치 처리 최적화 기법
- **dynamic-shapes.md** - 동적 입력 크기 처리 최적화

## 🎯 핵심 성능 목표
- **추론 속도**: FP16 모드에서 < 8ms
- **메모리 사용량**: 엔진 크기 < 500MB
- **정확도 손실**: 원본 모델 대비 < 2%

## 🔍 검색 키워드
- `tensorrt`, `onnx`, `optimization`, `inference-engine`
- `fp16`, `int8`, `quantization`, `memory-optimization`
- `cuda`, `gpu-acceleration`, `batch-processing`