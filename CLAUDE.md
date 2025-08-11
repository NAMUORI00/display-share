# CLAUDE.md - C_capture 320x320 중심 영역 검출 시스템 완전 가이드

Claude Code (claude.ai/code) 개발자를 위한 C_capture 프로젝트 통합 가이드

## 프로젝트 개요

**C_capture**는 **320x320 중심 영역 실시간 객체 검출 시스템**을 위한 고성능 C++17 전문 애플리케이션으로, 화면 중앙의 320x320 픽셀 영역에서 280+ FPS YOLOv11 객체 검출과 HSV 색상 추적을 수행하는 특화된 컴퓨터 비전 플랫폼입니다.

### 핵심 특징 (Phase 0-3 완료)
- **320x320 중심 영역 특화**: 전체 화면 대신 중앙 320x320 영역에 집중한 고성능 처리
- **280+ FPS YOLO 추론**: YOLOv11 TensorRT 최적화로 실시간 객체 검출 달성  
- **35% 코드 감축**: 7,126줄 → 4,700줄로 아키텍처 단순화 및 성능 최적화
- **전문 GUI 시스템**: ImGui 기반 4패널 모던 인터페이스 (ROI 시각화, 검출 결과, 제어판, 성능 대시보드)
- **간소화된 아키텍처**: 복잡한 팩토리 패턴 제거, 직접 인스턴스화로 성능 향상

## 프로젝트 구조 (320x320 최적화 아키텍처)

```
C_capture/
├── CLAUDE.md                    # 📍 이 파일 - 320x320 시스템 완전 가이드
├── .gitmodules                  # Git submodule 구성 (최적화됨)
├── build/                       # CMake 빌드 결과물 (정적 링킹)
│
└── ScreenMonitor/               # 🎯 320x320 중심 영역 검출 시스템
    ├── CMakeLists.txt          # 최적화된 빌드 설정
    ├── README.md               # 프로젝트 소개
    │
    ├── src/                    # 간소화된 소스 코드 (35% 감축)
    │   ├── main.cpp            # WinMain 진입점 (Phase 2 최적화)
    │   ├── core/               # ConfigManager만 유지
    │   ├── capture/            # CenterRegionCapture, ScreenCaptureLiteDevice
    │   ├── detection/          # HSVColorDetection, YOLOv11TensorRT
    │   ├── gui/                # MainInterface (4패널 모던 GUI)
    │   └── monitoring/         # SimpleMetrics (성능 모니터링)
    │
    ├── include/                # 간소화된 헤더 구조
    │   ├── interfaces/         # 핵심 인터페이스만 유지
    │   ├── core/               # ConfigManager, Constants
    │   ├── capture/            # 320x320 특화 캡처 헤더
    │   ├── detection/          # HSV, YOLOv11 헤더
    │   ├── gui/                # 모던 인터페이스 헤더
    │   └── monitoring/         # SimpleMetrics 헤더
    │
    ├── external/               # 최적화된 submodule 의존성
    │   ├── opencv/             # 최소 모듈만 (core, imgproc, imgcodecs)
    │   ├── imgui/              # docking 브랜치 (4패널 GUI)
    │   ├── googletest/         # 테스트 프레임워크
    │   ├── nlohmann_json/      # JSON 설정 처리
    │   └── screen_capture_lite/ # 고성능 화면 캡처
    │
    ├── tests/                  # 320x320 특화 테스트
    │   ├── helpers/            # 테스트 유틸리티 (최소화)
    │   ├── mocks/              # Mock 객체 (HSV, ScreenCapture)
    │   └── test_*.cpp          # 320x320 시스템 테스트
    │
    ├── config/                 # 간소화된 JSON 설정
    ├── models/                 # YOLOv11 TensorRT 엔진 파일
    └── scripts/                # 개발 스크립트
```

## 빠른 시작 가이드 (320x320 시스템)

### 1. 저장소 복제 및 초기화
```bash
# 320x320 최적화 시스템 복제
git clone <repository-url> C_capture
cd C_capture

# 최적화된 submodule 초기화 (필수)
git submodule update --init --recursive

# 320x320 시스템 상태 확인
git submodule status
```

### 2. 개발 환경 요구사항 (320x320 특화)
- **운영체제**: Windows 10/11 (64-bit) - 320x320 GUI 최적화
- **빌드 도구**: Visual Studio 2019+ (C++17 지원)
- **CMake**: 3.16+ (정적 링킹 최적화)
- **GPU**: NVIDIA RTX (TensorRT 280+ FPS) 또는 CPU 백엔드 자동 전환
- **메모리**: 8GB+ RAM (320x320 버퍼 최적화)

### 3. 빌드 및 실행 (고성능 모드)
```bash
# 320x320 최적화 빌드 구성
cmake -B build -S ScreenMonitor -DCMAKE_BUILD_TYPE=Release

# 정적 링킹 병렬 빌드
cmake --build build --config Release --parallel

# 320x320 검출 시스템 실행
./build/bin/Release/SmartScreenCapture.exe
# 예상 성능: 280+ FPS YOLO, 60+ FPS GUI

# 320x320 특화 테스트 실행
ctest -C Release --test-dir build --verbose
```

### 4. 320x320 시스템 확인
애플리케이션 실행 시 다음 성능 지표를 확인하세요:
- **ROI 추출**: <5ms (320x320 중심 영역)
- **YOLO 추론**: 280+ FPS (TensorRT), 30+ FPS (CPU 백엔드)
- **GUI 응답**: 60+ FPS (모던 4패널 인터페이스)
- **메모리 사용**: <500MB (최적화된 버퍼 관리)

## 320x320 시스템 아키텍처 (간소화 및 최적화)

### 핵심 애플리케이션 플로우 (Phase 2-3 최적화)
320x320 중심 영역 검출 시스템의 고성능 워크플로우:

1. **초기화**: MainInterface가 4패널 ImGui 인터페이스 생성 (Phase 3)
2. **320x320 GUI 설정**: ROI 시각화, 검출 결과, 제어판, 성능 대시보드
3. **CenterRegionCapture**: 화면 중앙 320x320 영역 고속 추출 (<5ms)
4. **병렬 검출**: HSV 색상 추적 + YOLOv11 객체 검출 (280+ FPS)
5. **실시간 업데이트**: 60+ FPS GUI로 검출 결과 시각화

### 핵심 아키텍처 패턴 (35% 감축 후)

#### 4패널 모던 GUI (MainInterface - Phase 3)
- **ROI 시각화 패널**: 320x320 중심 영역 실시간 표시 (1:1 비율)
- **검출 결과 패널**: HSV 좌표 + YOLO 바운딩 박스 실시간 표시
- **제어 패널**: 간소화된 설정 (HSV 튜닝, YOLO 임계값)
- **성능 대시보드**: FPS, 처리 시간, ROI 메트릭 실시간 모니터링

#### 간소화된 직접 인스턴스화 (팩토리 제거)
- **CenterRegionCapture**: 직접 생성으로 오버헤드 제거
- **YOLOv11TensorRT**: GPU/CPU 백엔드 자동 전환
- **HSVColorDetection**: 320x320 최적화된 색상 추적
- **SimpleMetrics**: 경량 성능 모니터링

#### 핵심 인터페이스 (최소화)
- **ICaptureDevice**: 화면 캡처 추상화 (ScreenCaptureLiteDevice)
- **IDetectionAlgorithm**: 검출 알고리즘 통합 (HSV, YOLO)
- **IPerformanceObserver**: 성능 관찰 (SimpleMetrics)

#### 320x320 특화 설정 시스템 (ConfigManager)
- **간소화된 JSON**: 320x320 ROI, HSV 범위, YOLO 설정만 유지
- **실시간 변경**: GUI에서 설정 변경 시 즉시 적용
- **성능 최적화**: 불필요한 설정 계층 제거

## 320x320 소스 코드 아키텍처 (35% 감축)

### 📁 src/ 디렉토리 구조 (간소화)
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

### 핵심 모듈 상세 분석

#### 🚀 main.cpp - 320x320 시스템 진입점 (Phase 2 최적화)
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

#### 📹 CenterRegionCapture.cpp - ✨ 320x320 중심 영역 고속 추출 (신규)
**역할**: 화면 중앙 320x320 픽셀 영역의 초고속 추출 시스템 (<5ms)

**핵심 기능 (Phase 2 완료)**:
- **<5ms 고속 추출**: 화면 중앙 320x320 영역 초고속 처리
- **좌표 변환**: ROI 좌표 ↔ 전체 화면 좌표 양방향 변환
- **메모리 최적화**: 320x320 버퍼 전용 메모리 관리
- **성능 메트릭**: 추출 시간, 메모리 사용량 실시간 모니터링

#### 🔍 YOLOv11TensorRTInference.cpp - 280+ FPS 객체 검출 (완료)
**역할**: 320x320 ROI에서 280+ FPS 실시간 YOLOv11 TensorRT 객체 검출

**고성능 특징 (완료)**:
- **280+ FPS GPU**: TensorRT 엔진 최적화로 실시간 추론
- **30+ FPS CPU**: CUDA 미지원 환경에서 CPU 백엔드 자동 전환
- **실시간 결과**: 바운딩 박스, 신뢰도, 클래스명 실시간 GUI 표시

#### 🖥️ MainInterface.cpp - 876줄 최적화된 4패널 인터페이스
**역할**: 320x320 ROI 시각화 및 검출 결과를 위한 전문 4패널 모던 GUI

**Phase 3 핵심 기능**:
- **ROI 시각화 패널**: 320x320 중심 영역 실시간 표시 (1:1 비율)
- **검출 결과 패널**: HSV 좌표 + YOLO 바운딩 박스 실시간 표시
- **제어 패널**: HSV 튜닝, YOLO 설정, 시작/정지 통합 제어
- **성능 대시보드**: FPS, 처리 시간, ROI 메트릭 실시간 모니터링

## Git Submodule 관리 (320x320 최적화)

### 320x320 최적화 외부 의존성 아키텍처
ScreenMonitor는 **5개의 핵심 Git submodule**을 통해 320x320 중심 영역 검출 시스템에 최적화된 검증된 라이브러리들을 활용합니다.

### 📁 external/ 디렉토리 맵 (Phase 0-3 최적화)
```
external/
├── opencv/                      # 🔍 320x320 ROI 처리 최적화
│   ├── modules/core/            # Mat, ROI 연산 (320x320 전용)
│   ├── modules/imgproc/         # 이미지 처리 (HSV 변환, 필터링)
│   ├── modules/imgcodecs/       # 이미지 코덱 (PNG 저장용)
│   └── [3개 모듈만 빌드]        # 최소 구성으로 빌드 시간 단축
│
├── imgui/                       # 🖥️ 4패널 모던 GUI (Phase 3)
│   ├── imgui.cpp/.h             # 핵심 GUI 시스템
│   ├── backends/                # GLFW + OpenGL3 백엔드
│   ├── imgui_internal.h         # DockBuilder API (4패널 도킹)
│   └── [docking 브랜치 필수]    # ROI 시각화 패널 지원
│
├── googletest/                  # 🧪 320x320 시스템 테스트
│   ├── googletest/              # CenterRegionCapture 단위 테스트
│   ├── googlemock/              # Mock 시스템 (HSV, ScreenCapture)
│   └── [성능 벤치마크 포함]     # 280+ FPS YOLO 검증
│
├── nlohmann_json/               # 📄 간소화된 설정 관리
│   ├── single_include/          # 헤더 전용 (320x320 설정 스키마)
│   └── [설정 파일 최소화]       # ROI, HSV, YOLO 설정만
│
└── screen_capture_lite/         # 📹 고성능 전체 화면 캡처
    ├── src_cpp/                 # 전체 화면 캡처 (320x320 추출용)
    ├── include/                 # CenterRegionCapture 연동 API
    └── [최적화된 버퍼 관리]     # 320x320 전용 메모리 최적화
```

### 320x320 시스템 핵심 의존성
| 모듈 | 320x320 역할 | 최적화 특징 |
|------|------|------|
| **opencv** | 320x320 ROI 처리 | core, imgproc, imgcodecs만 (최소화) |
| **imgui** | 4패널 모던 GUI | docking 브랜치 (ROI 시각화 최적화) |
| **googletest** | 320x320 시스템 테스트 | CenterRegionCapture, YOLO 통합 테스트 |
| **nlohmann_json** | 간소화된 설정 | 320x320 특화 설정 스키마 |
| **screen_capture_lite** | 고성능 전체 화면 캡처 | 320x320 추출 최적화 |

### 320x320 최적화 Submodule 관리
```bash
# 모든 submodule을 320x320 최적화 버전으로 업데이트
git submodule update --remote

# 특정 submodule 업데이트 (예: OpenCV 최소화)
git submodule update --remote ScreenMonitor/external/opencv

# ImGui docking 브랜치 확인 (4패널 GUI 필수)
cd ScreenMonitor/external/imgui && git checkout docking

# 320x320 시스템 submodule 커밋
git add .gitmodules ScreenMonitor/external/
git commit -m "320x320 최적화: opencv 최소화, imgui docking 업데이트"

# Submodule 문제 해결 (320x320 시스템 재초기화)
git submodule deinit --all
git submodule update --init --recursive
```

## 320x320 테스트 전략 (GoogleTest/Mock 기반)

### 🎯 320x320 테스트 철학
- **성능 최우선**: 280+ FPS YOLO, <5ms ROI 추출 성능 검증
- **간소화된 커버리지**: 핵심 모듈 (CenterRegionCapture, YOLOv11, HSV) 90%+ 커버리지
- **실시간 벤치마킹**: 320x320 시스템 특성에 맞는 실시간 성능 테스트
- **GPU 테스트**: TensorRT 280+ FPS 및 CPU 백엔드 30+ FPS 검증

### 📊 320x320 테스트 피라미드 구조 (간소화)
```
           🔺 E2E Tests (5%)
          4패널 GUI, 320x320 워크플로우 테스트
         
        🔺🔺 Integration Tests (15%)
       ROI-YOLO-HSV 통합, GPU/CPU 성능 테스트
      
    🔺🔺🔺 Unit Tests (80%)
   CenterRegionCapture, YOLOv11, HSV 개별 테스트
```

### 📁 tests/ 아키텍처 (Phase 0-3 최적화)
```
tests/
├── test_*.cpp                   # 📋 320x320 특화 단위 테스트
│   ├── test_ConfigManager.cpp   # 간소화된 설정 관리 테스트
│   ├── test_CenterRegionCapture.cpp # ✨ 320x320 ROI 추출 테스트 (<5ms 검증)
│   ├── test_HSVColorDetection.cpp   # HSV 색상 추적 테스트 (320x320 최적화)
│   ├── test_YOLOv11Integration.cpp  # 280+ FPS YOLO 통합 테스트
│   ├── test_MainInterface.cpp   # 4패널 GUI 테스트 (Phase 3)
│   └── test_Performance*.cpp    # 성능 벤치마크 통합 테스트
│
├── helpers/                     # 🛠️ 320x320 테스트 유틸리티 (최소화)
│   ├── ConfigTestHelper.cpp/.h  # 간소화된 설정 테스트 도우미
│   ├── GLTestContext.cpp/.h     # 4패널 GUI 테스트 컨텍스트
│   └── TestImageGenerator.cpp/.h # 320x320 테스트 이미지 생성
│
├── mocks/                       # 🎭 간소화된 Mock 객체
│   ├── MockHSVDetector.cpp/.h   # HSV 검출 Mock (간소화)
│   └── MockScreenCapture.cpp/.h # 화면 캡처 Mock (320x320 특화)
│
├── benchmark_tests.cpp          # ⚡ 280+ FPS YOLO 성능 벤치마크
└── [320x320 테스트 데이터]     # 🗂️ 320x320 테스트 이미지, ROI 설정
```

### 🚀 320x320 시스템 테스트 실행 방법

#### 로컬 개발 환경 (320x320 최적화)
```bash
# 320x320 시스템 전체 테스트 빌드 (Release 필수)
cmake --build build --config Release --parallel

# 320x320 모든 테스트 실행 (성능 검증 포함)
ctest -C Release --test-dir build --verbose

# 320x320 핵심 테스트 개별 실행
./build/bin/tests/test_CenterRegionCapture.exe      # <5ms ROI 추출 테스트
./build/bin/tests/test_YOLOv11Integration.exe       # 280+ FPS YOLO 테스트
./build/bin/tests/test_HSVColorDetection.cpp        # HSV 색상 추적 테스트
./build/bin/tests/test_MainInterface.exe            # 4패널 GUI 테스트 (Phase 3)

# 320x320 성능 벤치마크 실행 (핵심 성능 검증)
./build/bin/tests/benchmark_320x320_tests.exe       # 280+ FPS, <5ms ROI 검증
```

## 320x320 시스템 개발 워크플로우

### 320x320 최적화 개발 사이클
```bash
# 1. 320x320 시스템 최신 동기화
git pull origin main
git submodule update --recursive

# 2. 320x320 기능 브랜치 생성
git checkout -b feature/320x320-개선

# 3. 320x320 코드 수정 및 성능 테스트
cmake --build build --config Release
ctest -C Release --test-dir build
# 성능 확인: YOLO 280+ FPS, GUI 60+ FPS

# 4. 320x320 최적화 커밋
git add .
git commit -m "feat: 320x320 ROI 처리 성능 향상"

# 5. PR 생성 (성능 지표 포함)
git push origin feature/320x320-개선
```

### 320x320 시스템 문제 해결

#### 1. 320x320 ROI 추출 실패
```bash
# 증상: "CenterRegionCapture extraction failed"
# 해결: 화면 해상도가 320x320보다 큰지 확인
# 최소 요구사항: 1024x768 이상

# 디버그: ROI 정보 확인
std::cout << "Source: " << width << "x" << height << std::endl;
std::cout << "ROI: " << roi.x << "," << roi.y << " 320x320" << std::endl;
```

#### 2. YOLO TensorRT 280+ FPS 달성 실패
```bash
# 증상: YOLO 추론 속도가 예상보다 낮음
# 해결 1: TensorRT 엔진 재생성
rm models/engines/*.trt
# GPU 최적화 엔진 자동 생성됨

# 해결 2: GPU 메모리 및 CUDA 버전 확인
nvidia-smi  # GPU 메모리 8GB+ 권장
nvcc --version  # CUDA 11.0+ 필요
```

#### 3. 4패널 GUI 레이아웃 문제
```bash
# 증상: GUI 패널이 올바르게 도킹되지 않음
# 해결: imgui.ini 삭제 후 기본 레이아웃 재생성
rm imgui.ini
# 애플리케이션 재시작으로 기본 4패널 복원
```

#### 4. 성능 저하 문제
```bash
# 증상: GUI 60+ FPS 미달성
# 해결: Release 빌드 및 정적 링킹 확인
cmake -B build -S ScreenMonitor -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel

# OpenGL 3.3+ 지원 확인
glxinfo | grep "OpenGL version"  # Linux
# Windows: GPU 드라이버 업데이트
```

## 320x320 시스템 개발 가이드라인

### 320x320 최적화 코드 스타일
- **C++17 표준**: 고성능 320x320 처리를 위한 모던 C++ 활용
- **Windows GUI**: WinMain 기반 4패널 전문 인터페이스
- **한국어 주석**: 320x320 ROI 처리 로직에 상세한 한국어 설명
- **정적 링킹**: 단일 EXE 배포를 위한 모든 의존성 정적 연결
- **성능 우선**: 280+ FPS YOLO, 60+ FPS GUI 성능 기준

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

## 320x320 시스템 SuperClaude 통합

**SuperClaude Framework v1.0**과 완전 통합된 320x320 중심 영역 검출 시스템으로, 한국어 최적화 개발환경을 제공합니다.

### 320x320 특화 Claude Code 명령어
```bash
# 320x320 시스템 아키텍처 분석
/analyze --scope project --focus 320x320-roi-system

# 고성능 빌드 최적화 (280+ FPS YOLO)
/build --target release --optimize tensorrt --validate performance

# 320x320 코드 품질 개선
/improve --focus performance --scope center-region-capture

# 320x320 특화 테스트 전략
/test --type integration --focus roi-extraction --coverage 320x320

# YOLO TensorRT 성능 분석
/analyze --focus performance --target yolo-tensorrt --benchmark 280fps
```

### 320x320 시스템 한국어 개발 지원
- **한국어 주석**: 320x320 ROI 처리 로직에 상세한 한국어 주석
- **한국 시간대**: 성능 벤치마크 및 로그가 KST 기준으로 출력
- **문화적 맥락**: 한국 개발자의 실시간 시스템 개발 워크플로우 최적화

## 320x320 시스템 상태 및 성숙도 (Phase 0-3 완료)

### ✅ **Production Ready 성능**
- **ROI 추출**: <5ms (320x320 중심 영역)
- **YOLO 추론**: 280+ FPS (TensorRT GPU), 30+ FPS (CPU 백엔드)
- **GUI 응답**: 60+ FPS (4패널 모던 인터페이스)
- **메모리 효율**: <500MB (최적화된 버퍼 관리)
- **코드 감축**: 35% (7,126줄 → 4,700줄)

### 🟢 완성된 시스템 특징
- **🟢 완전한 문서화**: Phase 0-3 변환 과정 및 320x320 특화 가이드 완비
- **🟢 전문 GUI**: ImGui 4패널 모던 인터페이스 (ROI 시각화, 검출 결과, 제어, 성능)
- **🟢 YOLOv11 TensorRT**: 완전 구현된 280+ FPS 객체 검출 (GPU/CPU 자동 전환)
- **🟢 간소화된 아키텍처**: 팩토리 패턴 제거로 직접 인스턴스화 성능 향상

## 320x320 시스템 기여 및 협업

### 새로운 개발자를 위한 320x320 시스템 진입 경로
1. **320x320 시스템 이해**: 이 CLAUDE.md 전체 가이드 학습
2. **고성능 환경 설정**: Git submodule → CMake Release 빌드 → 성능 벤치마크 확인
3. **간소화된 코드 탐색**: src/ 구조를 통한 CenterRegionCapture, YOLOv11 모듈 학습
4. **320x320 기능 개발**: 직접 인스턴스화 패턴으로 확장 및 성능 테스트 작성

### 320x320 시스템 성능 기준
개발 시 다음 성능 기준을 준수하세요:
- **ROI 추출**: <5ms (320x320 중심 영역)
- **YOLO 추론**: 280+ FPS (TensorRT), 30+ FPS (CPU)
- **GUI 응답**: 60+ FPS (4패널 인터페이스)
- **메모리 효율**: <500MB (최적화된 버퍼)

### Phase 0-3 변환 요약
- **Phase 0**: 전체 분석 및 외부 연구 완료
- **Phase 1**: 35% 코드 감축, 아키텍처 간소화
- **Phase 2**: 320x320 중심 캡처 + YOLOv11 TensorRT 완료
- **Phase 3**: 4패널 모던 GUI, 876줄 최적화 완료

---

**C_capture 320x320 시스템** - 고성능 중심 영역 실시간 객체 검출 플랫폼  
Phase 0-3 완료 | 280+ FPS YOLO | SuperClaude Framework v1.0 호환 | 한국어 최적화