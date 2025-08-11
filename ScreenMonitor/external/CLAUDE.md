# CLAUDE.md - 320x320 최적화 Git Submodule 관리 가이드

ScreenMonitor 320x320 중심 영역 검출 시스템의 외부 의존성 및 Git submodule 전문 관리 가이드

## 320x320 최적화 외부 의존성 아키텍처

ScreenMonitor는 **5개의 핵심 Git submodule**을 통해 320x320 중심 영역 검출 시스템에 최적화된 검증된 라이브러리들을 활용합니다. Phase 0-3을 통해 35% 코드 감축과 함께 의존성도 최적화되었습니다.

### 🎯 320x320 특화 설계 철학
- **성능 최적화**: 280+ FPS YOLO, 60+ FPS GUI를 위한 최소 의존성
- **버전 고정**: 320x320 시스템 안정성을 위한 검증된 커밋 고정 관리
- **모듈 최소화**: OpenCV 3개 모듈만 사용 (core, imgproc, imgcodecs)
- **정적 링킹**: 단일 실행파일 배포를 통한 <500MB 메모리 최적화

## 320x320 최적화 Submodule 구조

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

## 핵심 Submodule 상세 분석

### 🔍 OpenCV - 320x320 ROI 처리 최적화

**Repository**: https://github.com/opencv/opencv.git  
**320x320 사용 목적**: 중심 영역 ROI 추출, HSV 색상 검출, 이미지 처리

#### 320x320 특화 빌드 구성
```cmake
# OpenCV 320x320 최적화 빌드 - 3개 모듈만 사용
set(BUILD_LIST "core,imgproc,imgcodecs" CACHE STRING "")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(BUILD_TESTS OFF CACHE BOOL "")
set(BUILD_EXAMPLES OFF CACHE BOOL "")
set(BUILD_DOCS OFF CACHE BOOL "")
set(WITH_IPP OFF CACHE BOOL "")          # 320x320에서 불필요
set(WITH_TBB OFF CACHE BOOL "")          # 단순 ROI 처리로 충분
set(WITH_CUDA OFF CACHE BOOL "")         # YOLO에서 TensorRT 사용
```

**320x320 핵심 모듈**:
- **core**: 320x320 Mat 구조, ROI 연산 (cv::Rect, cv::Point)
- **imgproc**: HSV 변환, 색상 필터링, 320x320 리사이징
- **imgcodecs**: 320x320 이미지 저장 (PNG, JPEG 디버깅용)

**320x320 개발 가이드**:
```cpp
// 🟢 320x320 특화: 필요한 헤더만 포함
#include <opencv2/core.hpp>             // 320x320 Mat 연산
#include <opencv2/imgproc.hpp>          // HSV 변환, ROI 처리

// 🟢 320x320 메모리 효율적인 사용
cv::Mat fullFrame(1080, 1920, CV_8UC3, buffer_ptr);    
cv::Rect centerROI(800, 380, 320, 320);                // 중심 320x320
cv::Mat roi = fullFrame(centerROI);                     // 복사 없이 ROI 참조

// 🟢 HSV 변환 (320x320 픽셀만)
cv::Mat hsvROI;
cv::cvtColor(roi, hsvROI, cv::COLOR_BGR2HSV);
```

### 🖥️ ImGui - 4패널 모던 GUI (Phase 3 완료)

**Repository**: https://github.com/ocornut/imgui.git  
**브랜치**: **docking** (필수!)  
**320x320 사용 목적**: ROI 시각화, 검출 결과, 제어판, 성능 대시보드

#### 중요한 브랜치 관리
```bash
# ⚠️ 중요: master 브랜치가 아닌 docking 브랜치 사용
cd external/imgui
git checkout docking
git pull origin docking

# docking 브랜치 확인
git branch -v
# * docking abc1234 [origin/docking] Docking: 최신 기능
```

**핵심 구성 요소**:
- **imgui.cpp/.h**: 핵심 GUI 시스템 및 위젯
- **imgui_internal.h**: 도킹 API 접근 (DockBuilder)
- **backends/**: GLFW + OpenGL3 백엔드 구현
- **imgui_demo.cpp**: 모든 GUI 기능 예제

**빌드 통합**:
```cmake
# ImGui 빌드 설정
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
```

### 🧪 GoogleTest - 테스트 프레임워크

**Repository**: https://github.com/google/googletest.git  
**사용 목적**: 단위 테스트, 통합 테스트, 모킹

#### 테스트 아키텍처 통합
```cmake
# GoogleTest 빌드 설정
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(external/googletest)

# 테스트 타겟 생성
add_executable(tests ${TEST_SOURCES})
target_link_libraries(tests gtest gtest_main gmock)
```

**주요 기능**:
- **GTest**: ASSERT/EXPECT 매크로, 테스트 픽스처
- **GMock**: Mock 객체 생성 및 행위 검증
- **통합 실행**: CTest와 연동한 자동화된 테스트

### 📄 nlohmann_json - JSON 처리

**Repository**: https://github.com/nlohmann/json.git  
**사용 목적**: 설정 파일 파싱 및 직렬화

#### 헤더 전용 라이브러리 특징
```cpp
// 🟢 단일 헤더 포함으로 즉시 사용 가능
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// 설정 파일 로딩 예제
json config;
std::ifstream config_file("config.json");
config_file >> config;

// 타입 안전한 값 접근
int fps = config["performance"]["target_fps"].get<int>();
bool enabled = config["vision_algorithms"]["hsv_detection"]["enabled"];
```

### 📹 screen_capture_lite - 화면 캡처

**Repository**: https://github.com/smasherprog/screen_capture_lite.git  
**사용 목적**: 크로스플랫폼 고성능 화면 캡처

#### 핵심 API 활용
```cpp
#include "ScreenCapture.h"

// 모니터 열거 및 선택
auto monitors = SL::Screen_Capture::GetMonitors();
auto selected_monitor = monitors[0];

// 캡처 시작
auto capture = SL::Screen_Capture::CreateCaptureConfiguration([](){
    return SL::Screen_Capture::GetMonitors();
})
->onFrameChanged([](const SL::Screen_Capture::Image& img, const SL::Screen_Capture::Monitor& monitor){
    // 프레임 처리 로직
})
->start_capturing();
```

## Git Submodule 관리 워크플로우

### 🚀 초기 설정 및 복제

#### 새로운 환경에서 프로젝트 시작
```bash
# 1. 메인 저장소 복제
git clone --recursive <repository-url> C_capture
cd C_capture

# 2. 모든 submodule 초기화 (필수)
git submodule update --init --recursive

# 3. 중요: ImGui docking 브랜치 전환
cd ScreenMonitor/external/imgui
git checkout docking
cd ../../..

# 4. 빌드 테스트
cmake -B build -S ScreenMonitor -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

#### Submodule 상태 확인
```bash
# 전체 submodule 상태 확인
git submodule status

# 예상 출력 예시:
# +a1b2c3d4 ScreenMonitor/external/opencv (4.8.0-123-ga1b2c3d4)
# +e5f6g7h8 ScreenMonitor/external/imgui (v1.89.2-docking-456-ge5f6g7h8)
# +i9j0k1l2 ScreenMonitor/external/googletest (release-1.12.1-789-gi9j0k1l2)
```

### 🔄 Submodule 업데이트 전략

#### 개별 Submodule 업데이트
```bash
# 특정 submodule을 최신 버전으로 업데이트
cd ScreenMonitor/external/opencv
git fetch origin
git checkout v4.8.1  # 안정적인 태그로 업데이트
cd ../../..

# 변경사항을 메인 저장소에 커밋
git add ScreenMonitor/external/opencv
git commit -m "OpenCV를 v4.8.1로 업데이트"
```

#### 전체 Submodule 일괄 업데이트
```bash
# ⚠️ 주의: 호환성 검증 후 실행
git submodule update --remote

# ImGui docking 브랜치 확인 (필수)
cd ScreenMonitor/external/imgui
git checkout docking
cd ../../..

# 빌드 테스트로 호환성 검증
cmake --build build --config Release
ctest -C Release --test-dir build
```

### 🔒 버전 고정 및 안정성 관리

#### 특정 커밋으로 고정
```bash
# 안정적인 버전으로 고정 (권장)
cd ScreenMonitor/external/opencv
git checkout 4.8.0-rc1  # 안정적인 릴리스 후보
cd ../imgui  
git checkout docking    # 최신 docking 기능
git reset --hard abc123 # 검증된 커밋으로 고정
cd ../../..

# 고정된 버전 커밋
git add ScreenMonitor/external/
git commit -m "Submodule을 검증된 안정 버전으로 고정"
```

#### 호환성 매트릭스 관리
```bash
# 검증된 버전 조합 (권장)
OpenCV: v4.8.0+
ImGui: docking 브랜치 (2023년 12월 이후)
GoogleTest: v1.12.1+
nlohmann_json: v3.11.0+
screen_capture_lite: master 최신
```

## CMake 빌드 통합

### 🏗️ 통합 빌드 설정

#### 루트 CMakeLists.txt 구조
```cmake
# Submodule 존재 확인
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/CMakeLists.txt")
    message(FATAL_ERROR "Git submodule이 초기화되지 않았습니다. 다음 명령을 실행하세요:\n"
                        "git submodule update --init --recursive")
endif()

# OpenCV 최소 빌드 설정
set(BUILD_LIST "core,imgproc,imgcodecs" CACHE STRING "")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
add_subdirectory(external/opencv)

# ImGui 정적 라이브러리 빌드
add_library(imgui STATIC
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

# nlohmann_json 헤더 전용
add_subdirectory(external/nlohmann_json)

# GoogleTest (테스트용)
if(BUILD_TESTING)
    add_subdirectory(external/googletest)
endif()

# screen_capture_lite
add_subdirectory(external/screen_capture_lite)
```

### 📦 정적 링킹 최적화

#### 배포용 단일 실행파일 생성
```cmake
# 정적 링크 설정
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET SmartScreenCapture PROPERTY WIN32_EXECUTABLE TRUE)

# 의존성 라이브러리 링크
target_link_libraries(SmartScreenCapture PRIVATE
    opencv_core opencv_imgproc opencv_imgcodecs
    imgui
    nlohmann_json::nlohmann_json
    screen_capture_lite_static
    opengl32 gdi32 user32    # Windows 시스템 라이브러리
)
```

## 문제 해결 및 트러블슈팅

### 🚨 일반적인 문제와 해결책

#### 1. Submodule 초기화 실패
```bash
# 증상: "external/opencv not found" 오류
# 원인: Git submodule이 초기화되지 않음
# 해결:
git submodule update --init --recursive
git submodule foreach git reset --hard HEAD
```

#### 2. ImGui DockBuilder API 오류
```bash
# 증상: "DockBuilder 관련 함수를 찾을 수 없음"
# 원인: master 브랜치 사용 또는 internal 헤더 누락
# 해결:
cd ScreenMonitor/external/imgui
git checkout docking
git pull origin docking

# 소스 코드에서 확인:
#include <imgui_internal.h>  // DockBuilder API 접근용
```

#### 3. OpenCV 빌드 크기 문제
```bash
# 증상: 빌드 시간 과도하게 오래 걸림, 실행파일 크기 거대
# 원인: 모든 OpenCV 모듈이 빌드됨
# 해결: CMake에서 최소 모듈만 선택
set(BUILD_LIST "core,imgproc,imgcodecs" CACHE STRING "")
set(BUILD_TESTS OFF CACHE BOOL "")
set(BUILD_EXAMPLES OFF CACHE BOOL "")
```

#### 4. GoogleTest 링크 오류
```bash
# 증상: "LNK2019: 해결되지 않은 외부 기호" 오류
# 원인: 정적/동적 런타임 라이브러리 불일치
# 해결:
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
```

#### 5. screen_capture_lite 성능 문제
```bash
# 증상: 캡처 성능 저하 또는 메모리 누수
# 원인: 잘못된 프레임 버퍼 관리
# 해결: 적절한 생명주기 관리
```

### 🔧 고급 트러블슈팅

#### Submodule 상태 복구
```bash
# 심각한 submodule 문제 시 완전 재설정
git submodule deinit --all --force
git submodule update --init --recursive

# 특정 submodule만 재설정
git submodule deinit external/opencv --force
git submodule update --init external/opencv
```

#### 빌드 캐시 정리
```bash
# CMake 캐시 완전 삭제
rm -rf build/
rm -rf external/*/build/

# 재빌드
cmake -B build -S ScreenMonitor -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

#### 의존성 버전 호환성 테스트
```bash
# 각 submodule의 버전 정보 확인
cd ScreenMonitor/external/opencv && git describe --tags
cd ../imgui && git log --oneline -1
cd ../googletest && git describe --tags
cd ../nlohmann_json && git describe --tags
cd ../screen_capture_lite && git log --oneline -1
```

## 의존성 업그레이드 가이드

### 📈 안전한 업그레이드 절차

#### 1단계: 백업 및 브랜치 생성
```bash
# 현재 상태 백업
git checkout -b submodule-upgrade-backup

# 업그레이드 브랜치 생성
git checkout -b feature/submodule-upgrade-202X-XX
```

#### 2단계: 개별 테스트
```bash
# 각 submodule을 개별적으로 업그레이드 및 테스트
cd ScreenMonitor/external/opencv
git fetch origin
git checkout v4.9.0

# 부분 빌드 테스트
cd ../../..
cmake --build build --target opencv_core --config Release
```

#### 3단계: 통합 테스트
```bash
# 전체 빌드 테스트
cmake --build build --config Release

# 테스트 실행
ctest -C Release --test-dir build --verbose

# 애플리케이션 기능 테스트
./build/bin/Release/SmartScreenCapture.exe
```

#### 4단계: 성능 검증
```bash
# 성능 벤치마크 실행 (있는 경우)
./build/bin/tests/benchmark_tests.exe

# 메모리 사용량 모니터링
# GPU 메모리 사용량 확인 (CUDA 환경)
```

### 🎯 업그레이드 체크리스트

- [ ] **백업 완료**: 현재 작업 상태 백업
- [ ] **개별 빌드**: 각 submodule 개별 빌드 성공
- [ ] **통합 빌드**: 전체 프로젝트 빌드 성공
- [ ] **테스트 실행**: 모든 단위/통합 테스트 통과
- [ ] **기능 검증**: 주요 기능 정상 동작 확인
- [ ] **성능 측정**: 성능 저하 없음 확인
- [ ] **메모리 검증**: 메모리 누수 없음 확인
- [ ] **호환성 확인**: 다른 시스템에서도 빌드 성공

---

**320x320 ScreenMonitor 외부 의존성 관리** - Git Submodule 최적화 가이드  
Phase 0-3 완료 | 280+ FPS YOLO | 60+ FPS GUI | 체계적 버전 관리 | 최소 의존성 최적화