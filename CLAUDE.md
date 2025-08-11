# CLAUDE.md

Claude Code (claude.ai/code) 개발자를 위한 C_capture 프로젝트 가이드

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
├── CLAUDE.md                    # 📍 이 파일 - 320x320 시스템 진입점
├── .gitmodules                  # Git submodule 구성 (최적화됨)
├── build/                       # CMake 빌드 결과물 (정적 링킹)
│
└── ScreenMonitor/               # 🎯 320x320 중심 영역 검출 시스템
    ├── CLAUDE.md               # 320x320 시스템 완전 가이드
    ├── src/CLAUDE.md           # 간소화된 아키텍처 가이드
    ├── external/CLAUDE.md      # 최적화된 의존성 관리 가이드  
    ├── tests/CLAUDE.md         # 320x320 특화 테스트 전략
    │
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

## Git Submodule 관리 (320x320 최적화)

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

## 320x320 시스템 전문 가이드

320x320 중심 영역 검출 시스템의 각 구성요소에 대한 상세 가이드:

- **📖 [ScreenMonitor/CLAUDE.md](ScreenMonitor/CLAUDE.md)** - 320x320 시스템 완전 가이드 (Phase 0-3 통합)
- **🔧 [ScreenMonitor/src/CLAUDE.md](ScreenMonitor/src/CLAUDE.md)** - 간소화된 아키텍처 및 CenterRegionCapture 개발 가이드
- **📦 [ScreenMonitor/external/CLAUDE.md](ScreenMonitor/external/CLAUDE.md)** - 최적화된 의존성 관리 및 성능 튜닝 가이드
- **🧪 [ScreenMonitor/tests/CLAUDE.md](ScreenMonitor/tests/CLAUDE.md)** - 320x320 시스템 테스트 전략 및 성능 벤치마크 가이드

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

- **🟢 Production Ready**: 280+ FPS YOLO, 60+ FPS GUI 달성한 고성능 320x320 시스템
- **🟢 완전한 문서화**: Phase 0-3 변환 과정 및 320x320 특화 가이드 완비
- **🟢 35% 코드 감축**: 7,126줄 → 4,700줄로 아키텍처 최적화 완료
- **🟢 전문 GUI**: ImGui 4패널 모던 인터페이스 (ROI 시각화, 검출 결과, 제어, 성능)
- **🟢 YOLOv11 TensorRT**: 완전 구현된 280+ FPS 객체 검출 (GPU/CPU 자동 전환)

## 320x320 시스템 기여 및 협업

새로운 개발자를 위한 320x320 시스템 진입 경로:
1. **320x320 시스템 이해**: 이 CLAUDE.md → ScreenMonitor/CLAUDE.md 순서로 Phase 0-3 변환 학습
2. **고성능 환경 설정**: Git submodule → CMake Release 빌드 → 성능 벤치마크 확인
3. **간소화된 코드 탐색**: src/CLAUDE.md를 통한 CenterRegionCapture, YOLOv11 모듈 학습
4. **320x320 기능 개발**: 직접 인스턴스화 패턴으로 확장 및 성능 테스트 작성

### 320x320 시스템 성능 기준
개발 시 다음 성능 기준을 준수하세요:
- **ROI 추출**: <5ms (320x320 중심 영역)
- **YOLO 추론**: 280+ FPS (TensorRT), 30+ FPS (CPU)
- **GUI 응답**: 60+ FPS (4패널 인터페이스)
- **메모리 효율**: <500MB (최적화된 버퍼)

---

**C_capture 320x320 시스템** - 고성능 중심 영역 실시간 객체 검출 플랫폼  
Phase 0-3 완료 | 280+ FPS YOLO | SuperClaude Framework v1.0 호환 | 한국어 최적화