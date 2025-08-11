# CLAUDE.md - 320x320 소스 코드 아키텍처 가이드

ScreenMonitor 320x320 중심 영역 검출 시스템 소스 코드 구조 및 개발 가이드 (Phase 0-3 완료)

## 320x320 소스 코드 아키텍처 개요 (35% 감축)

ScreenMonitor는 **간소화된 C++17 아키텍처**를 기반으로 한 320x320 중심 영역 실시간 검출 시스템입니다. Phase 0-3를 통해 7,126줄에서 4,700줄로 35% 코드 감축을 달성했으며, 직접 인스턴스화를 통한 성능 최적화를 완료했습니다.

### 핵심 설계 원칙 (간소화된 아키텍처)
- **직접 인스턴스화**: 팩토리 패턴 제거로 성능 향상 (오버헤드 제거)
- **320x320 특화**: 중심 영역 처리에 최적화된 모듈 구조
- **성능 우선**: 280+ FPS YOLO, 60+ FPS GUI 달성
- **한국어 주석**: 320x320 ROI 처리 로직에 상세한 한국어 주석 표준

## 320x320 모듈별 아키텍처 (간소화)

### 📁 src/ 디렉토리 구조 (35% 감축 후)
```
src/
├── main.cpp                     # 🚀 WinMain 진입점 (Phase 2 최적화)
├── core/                        # 🔧 핵심 시스템 로직 (간소화)
│   └── ConfigManager.cpp        # 320x320 특화 JSON 설정 관리
├── capture/                     # 📹 320x320 특화 캡처 시스템
│   ├── CenterRegionCapture.cpp  # ✨ 320x320 중심 영역 고속 추출 (<5ms)
│   └── ScreenCaptureLiteDevice.cpp # 전체 화면 캡처 wrapper
├── detection/                   # 🔍 고성능 검출 알고리즘 (간소화)
│   ├── HSVColorDetection.cpp    # HSV 색상 추적 (320x320 최적화)
│   └── YOLOv11TensorRTInference.cpp # 280+ FPS YOLO 객체 검출
├── gui/                         # 🖥️ 4패널 모던 GUI (Phase 3)
│   └── MainInterface.cpp        # 876줄 최적화된 4패널 인터페이스
└── monitoring/                  # 📊 성능 모니터링 (완료)
    └── SimpleMetrics.cpp        # 경량 성능 메트릭 (60+ FPS GUI)
```

**🗑️ 제거된 구성요소 (35% 감축):**
- ❌ **ColorDetector.cpp** → HSVColorDetection으로 통합
- ❌ **ObjectDetector.cpp** → YOLOv11로 완전 대체
- ❌ **factories/ComponentFactory.cpp** → 직접 인스턴스화로 성능 최적화
- ❌ **복잡한 인터페이스 계층** → 핵심 인터페이스만 유지

## 320x320 핵심 모듈 상세 분석

### 🚀 main.cpp - 320x320 시스템 진입점 (Phase 2 최적화)

**역할**: 320x320 중심 영역 검출 시스템의 Windows GUI 애플리케이션 초기화

```cpp
// Phase 2: 320x320 CENTER REGION CAPTURE SYSTEM
// 고성능 WinMain 기반 GUI 애플리케이션
// 4패널 ImGui 인터페이스 및 320x320 ROI 초기화
```

**핵심 책임 (Phase 2 최적화)**:
- **320x320 시스템 초기화**: WinMain 진입점에서 ROI 시스템 부트스트랩
- **4패널 GUI 설정**: ImGui 컨텍스트 및 OpenGL 3.3+ 렌더링 초기화
- **직접 인스턴스화**: MainInterface 직접 생성 (팩토리 패턴 제거)
- **성능 최적화**: Release 빌드 기준 최적화된 리소스 관리

**Phase 2 개발 특징**:
- **최소한의 초기화**: 320x320 시스템에 필요한 핵심 로직만 포함
- **직접 위임**: 모든 320x320 GUI 로직을 MainInterface로 직접 위임
- **고성능 보장**: 280+ FPS YOLO, 60+ FPS GUI를 위한 예외 처리

### 🔧 core/ - 320x320 핵심 시스템 로직 (간소화)

#### ConfigManager.cpp - 320x320 특화 설정 관리
**역할**: 320x320 중심 영역 검출 시스템의 간소화된 JSON 설정 관리

```cpp
// 320x320 ROI 특화 JSON 스키마 및 타입 안전성
// 4패널 GUI에서 실시간 설정 변경 지원
// 간소화된 설정: center_region, hsv_detection, yolo_v11, gui, performance
```

**주요 기능 (간소화)**:
- **320x320 특화 스키마**: ROI 위치, 크기, HSV 범위, YOLO 임계값
- **실시간 GUI 연동**: 4패널 제어판에서 변경 시 즉시 적용
- **성능 우선 설정**: 280+ FPS YOLO, 60+ FPS GUI 최적화 파라미터
- **기본값 최적화**: 320x320 시스템에 최적화된 안전한 기본값

**간소화된 개발 패턴**:
```cpp
// 320x320 설정 추가 시 (간소화된 방식)
1. config/config.json에 320x320 관련 설정 추가
2. ConfigManager에 간단한 getter/setter 추가
3. MainInterface 제어 패널에 실시간 UI 추가
4. 직접 인스턴스화로 즉시 반영 (팩토리 없음)
```

### 📹 capture/ - 320x320 특화 캡처 시스템

#### CenterRegionCapture.cpp - ✨ 320x320 중심 영역 고속 추출 (신규)
**역할**: 화면 중앙 320x320 픽셀 영역의 초고속 추출 시스템 (<5ms)

```cpp
// 320x320 중심 영역 전용 고속 추출 클래스
// 전체 화면에서 중앙 ROI만 선택적 추출
// 좌표 변환 및 성능 메트릭 통합 관리
```

**핵심 기능 (Phase 2 완료)**:
- **<5ms 고속 추출**: 화면 중앙 320x320 영역 초고속 처리
- **좌표 변환**: ROI 좌표 ↔ 전체 화면 좌표 양방향 변환
- **메모리 최적화**: 320x320 버퍼 전용 메모리 관리
- **성능 메트릭**: 추출 시간, 메모리 사용량 실시간 모니터링

#### ScreenCaptureLiteDevice.cpp - 전체 화면 캡처 wrapper
**역할**: screen_capture_lite 라이브러리 래퍼로 전체 화면 캡처 제공

```cpp
// ICaptureDevice 인터페이스 구현
// 전체 화면 캡처 후 CenterRegionCapture로 ROI 추출
// 크로스플랫폼 캡처 안정성 보장
```

**간소화된 기능**:
- **전체 화면 캡처**: 하드웨어 가속 전체 화면 획득
- **320x320 연동**: CenterRegionCapture와 직접 연동
- **안정성 우선**: 캡처 실패 시 자동 복구

**320x320 캡처 시스템 추가 방법**:
```cpp
1. CenterRegionCapture 클래스 직접 생성 (팩토리 없음)
2. ExtractCenterRegion() 메서드로 ROI 추출
3. 좌표 변환은 TransformToScreenCoordinates() 사용
4. 성능 메트릭은 GetPerformanceMetrics() 활용
```

### 🔍 detection/ - 320x320 고성능 검출 알고리즘 (간소화)

320x320 ROI 전용 검출 알고리즘, `IDetectionAlgorithm` 인터페이스로 통합 API 제공

#### HSVColorDetection.cpp - 320x320 색상 추적 (유지됨)
**역할**: 320x320 ROI에서 HSV 색상 공간 기반 고성능 색상 추적

```cpp
// 320x320 영역 전용 HSV 색상 검출
// 4패널 GUI에서 실시간 HSV 범위 튜닝
// 조명 변화에 강인한 색상 추적 알고리즘
```

**320x320 최적화 특징**:
- **실시간 좌표 추출**: 320x320 영역 내 HSV 매칭 포인트 좌표
- **GUI 연동**: 제어 패널에서 HSV 범위 실시간 조정
- **성능 최적화**: 320x320 픽셀만 처리로 고속 검출

#### YOLOv11TensorRTInference.cpp - 280+ FPS 객체 검출 (완료)
**역할**: 320x320 ROI에서 280+ FPS 실시간 YOLOv11 TensorRT 객체 검출

```cpp
// YOLOv11 모델 + TensorRT GPU 가속 (280+ FPS)
// CPU 백엔드 자동 전환 (30+ FPS)
// 320x320 입력 크기로 최적화된 추론
```

**고성능 특징 (완료)**:
- **280+ FPS GPU**: TensorRT 엔진 최적화로 실시간 추론
- **30+ FPS CPU**: CUDA 미지원 환경에서 CPU 백엔드 자동 전환
- **실시간 결과**: 바운딩 박스, 신뢰도, 클래스명 실시간 GUI 표시

**🗑️ 제거된 알고리즘 (35% 감축)**:
- ❌ **ColorDetector.cpp** → HSVColorDetection으로 통합
- ❌ **ObjectDetector.cpp** → YOLOv11로 완전 대체

**320x320 검출 알고리즘 추가 (간소화된 방식)**:
```cpp
1. IDetectionAlgorithm 인터페이스 구현
   - detect() 메서드: 320x320 ROI 검출 수행
   - configure() 메서드: 320x320 특화 파라미터 설정
   - getResults() 메서드: ROI 좌표 결과 반환

2. 직접 인스턴스화 (팩토리 제거)
   - MainInterface에서 직접 std::make_unique 생성

3. config.json에 320x320 설정 추가
   - 간소화된 스키마에 ROI 특화 파라미터만 추가

4. 4패널 GUI에 제어 UI 추가
   - 제어 패널에 실시간 설정 UI 통합
```

### 🖥️ gui/ - 4패널 모던 GUI (Phase 3 완료)

#### MainInterface.cpp - 876줄 최적화된 4패널 인터페이스
**역할**: 320x320 ROI 시각화 및 검출 결과를 위한 전문 4패널 모던 GUI

```cpp
// Phase 3: 4패널 모던 GUI (876줄, 11% 감축)
// 320x320 ROI 시각화 패널 + 검출 결과 + 제어 + 성능 대시보드
// 전문 다크 테마 및 실시간 검출 결과 오버레이
```

**Phase 3 핵심 기능**:
- **ROI 시각화 패널**: 320x320 중심 영역 실시간 표시 (1:1 비율)
- **검출 결과 패널**: HSV 좌표 + YOLO 바운딩 박스 실시간 표시
- **제어 패널**: HSV 튜닝, YOLO 설정, 시작/정지 통합 제어
- **성능 대시보드**: FPS, 처리 시간, ROI 메트릭 실시간 모니터링

### 📊 monitoring/ - 성능 모니터링 (완료)

#### SimpleMetrics.cpp - 경량 성능 메트릭
**역할**: 320x320 시스템의 60+ FPS GUI를 위한 경량 성능 모니터링

```cpp
// 헤더 기반 경량 메트릭 시스템
// GUI 60+ FPS 유지를 위한 최소 오버헤드
// ROI 추출, YOLO 추론, GUI 렌더링 성능 추적
```

**🗑️ 제거된 구성요소 (팩토리 패턴 완전 제거)**:
- ❌ **factories/ComponentFactory.cpp** → 직접 인스턴스화로 대체
- ❌ **복잡한 의존성 주입** → 간단한 std::make_unique 생성

## 320x320 시스템 성능 지표 (Phase 0-3 달성)

### ✅ **Production Ready 성능**
- **ROI 추출**: <5ms (320x320 중심 영역)
- **YOLO 추론**: 280+ FPS (TensorRT GPU), 30+ FPS (CPU 백엔드)
- **GUI 응답**: 60+ FPS (4패널 모던 인터페이스)
- **메모리 효율**: <500MB (최적화된 버퍼 관리)
- **코드 감축**: 35% (7,126줄 → 4,700줄)

## 320x320 개발 가이드라인

### 간소화된 개발 패턴
```cpp
// 320x320 시스템의 직접 인스턴스화 패턴
auto centerCapture = std::make_unique<CenterRegionCapture>();
auto yoloDetector = std::make_unique<YOLOv11TensorRTInference>();
auto hsvDetector = std::make_unique<HSVColorDetection>();

// 320x320 ROI 추출 및 검출
cv::Mat roiFrame;
if (centerCapture->ExtractCenterRegion(fullFrame, roiFrame)) {
    auto yoloResults = yoloDetector->detect(roiFrame);
    auto hsvResults = hsvDetector->detect(roiFrame);
}
```

### Phase 0-3 변환 요약
- **Phase 0**: 전체 분석 및 외부 연구 완료
- **Phase 1**: 35% 코드 감축, 아키텍처 간소화
- **Phase 2**: 320x320 중심 캡처 + YOLOv11 TensorRT 완료
- **Phase 3**: 4패널 모던 GUI, 876줄 최적화 완료

---

**320x320 ScreenMonitor 소스 코드 아키텍처** - 간소화된 C++17 설계  
Phase 0-3 완료 | 280+ FPS YOLO | 60+ FPS GUI | 한국어 주석 표준