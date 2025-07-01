# YOLO v11 모델 가이드

## YOLO v11 ONNX 모델 다운로드

이 디렉토리는 YOLO v11 ONNX 모델 파일들을 저장하는 곳입니다.

### 1. 준비사항

```bash
# Python 및 Ultralytics 설치
pip install ultralytics
```

### 2. YOLO v11 모델 다운로드 및 변환

#### 2.1 사전 훈련된 모델 다운로드
```bash
# 나노 모델 (가장 빠름, 정확도 낮음)
yolo export model=yolo11n.pt format=onnx imgsz=640 opset=12

# 스몰 모델 (균형)
yolo export model=yolo11s.pt format=onnx imgsz=640 opset=12

# 미디엄 모델 (정확도 중점)
yolo export model=yolo11m.pt format=onnx imgsz=640 opset=12

# 라지 모델 (최고 정확도, 가장 느림)
yolo export model=yolo11l.pt format=onnx imgsz=640 opset=12
```

#### 2.2 수동 다운로드 (Python 스크립트)
```python
from ultralytics import YOLO

# 모델 로드 및 ONNX 변환
model = YOLO("yolo11n.pt")  # 나노 모델
model.export(format="onnx", imgsz=640, opset=12)

# 변환된 파일을 models/ 디렉토리로 이동
import shutil
shutil.move("yolo11n.onnx", "models/yolo11n.onnx")
```

### 3. 모델 파일 구조

변환 완료 후 다음과 같은 구조가 되어야 합니다:

```
models/
├── README.md           # 이 파일
├── yolo11n.onnx       # 나노 모델 (권장)
├── yolo11s.onnx       # 스몰 모델 (선택사항)
├── yolo11m.onnx       # 미디엄 모델 (선택사항)
├── yolo11l.onnx       # 라지 모델 (선택사항)
└── coco_classes.txt   # COCO 클래스 이름 (자동 생성)
```

### 4. 모델 검증

```python
import onnx

# ONNX 모델 검증
model = onnx.load('models/yolo11n.onnx')
onnx.checker.check_model(model)
print("모델 검증 완료!")
```

### 5. 권장 설정

- **개발/테스트**: yolo11n.onnx (가장 빠름)
- **일반 사용**: yolo11s.onnx (균형점)
- **고정확도 필요시**: yolo11m.onnx 또는 yolo11l.onnx

### 6. 지원하는 클래스

YOLO v11 모델은 COCO 데이터셋의 80개 클래스를 지원합니다:
- person, bicycle, car, motorcycle, airplane, bus, train, truck
- boat, traffic light, fire hydrant, stop sign, parking meter, bench
- bird, cat, dog, horse, sheep, cow, elephant, bear, zebra, giraffe
- backpack, umbrella, handbag, tie, suitcase, frisbee, skis, snowboard
- sports ball, kite, baseball bat, baseball glove, skateboard, surfboard
- tennis racket, bottle, wine glass, cup, fork, knife, spoon, bowl
- banana, apple, sandwich, orange, broccoli, carrot, hot dog, pizza
- donut, cake, chair, couch, potted plant, bed, dining table, toilet
- tv, laptop, mouse, remote, keyboard, cell phone, microwave, oven
- toaster, sink, refrigerator, book, clock, vase, scissors, teddy bear
- hair drier, toothbrush

### 7. 문제 해결

#### 7.1 모델 로딩 실패
- OpenCV 버전이 4.10.0 이상인지 확인
- ONNX 파일이 손상되지 않았는지 확인
- opset 버전 조정 (10, 11, 12 중 시도)

#### 7.2 성능 문제
- GPU 가속 활성화 여부 확인
- 입력 이미지 크기 조정 (640x640 권장)
- 모델 크기별 성능 차이 고려

### 8. 라이센스

YOLO v11 모델은 AGPL-3.0 라이센스를 따릅니다.
상업적 사용을 위해서는 Ultralytics 라이센스를 구매해야 할 수 있습니다.