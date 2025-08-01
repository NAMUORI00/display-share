# CLAUDE.md - 테스트 전략 및 품질 보증 가이드

ScreenMonitor 테스트 아키텍처, 전략, 및 품질 보증 시스템

## 테스트 전략 개요

ScreenMonitor는 **GoogleTest/GoogleMock 기반의 포괄적 테스트 전략**을 통해 고품질 소프트웨어를 보장합니다. 실시간 화면 캡처 및 컴퓨터 비전 시스템의 특성을 고려한 계층적 테스트 접근법을 채택합니다.

### 🎯 테스트 철학
- **품질 우선**: 기능 구현 전 테스트 케이스 작성 (TDD 지향)
- **실용적 커버리지**: 핵심 비즈니스 로직 80%+ 커버리지 목표
- **성능 테스트**: 실시간 시스템 특성에 맞는 성능 벤치마킹
- **통합 테스트**: GUI 및 외부 시스템과의 통합 검증

### 📊 테스트 피라미드 구조
```
           🔺 E2E Tests (5%)
          GUI 통합, 시나리오 테스트
         
        🔺🔺 Integration Tests (15%)
       모듈 간 상호작용, 성능 테스트
      
    🔺🔺🔺 Unit Tests (80%)
   개별 클래스, 함수, 알고리즘 테스트
```

## 테스트 디렉토리 구조

### 📁 tests/ 아키텍처
```
tests/
├── test_*.cpp                   # 📋 단위 테스트 파일들
│   ├── test_ConfigManager.cpp   # 설정 관리 테스트
│   ├── test_ColorDetector.cpp   # 색상 검출 알고리즘 테스트
│   ├── test_ObjectDetector.cpp  # 객체 검출 테스트
│   ├── test_YOLOv11.cpp         # YOLO 추론 테스트
│   ├── test_MainInterface.cpp   # GUI 컴포넌트 테스트
│   └── test_Integration*.cpp    # 통합 테스트
│
├── helpers/                     # 🛠️ 테스트 유틸리티
│   ├── ConfigTestHelper.cpp/.h  # 설정 테스트 도우미
│   ├── GLTestContext.cpp/.h     # OpenGL 테스트 컨텍스트
│   └── TestImageGenerator.cpp/.h # 테스트용 이미지 생성
│
├── mocks/                       # 🎭 Mock 객체
│   ├── MockDetector.cpp/.h      # 검출 알고리즘 Mock
│   └── MockScreenCapture.cpp/.h # 화면 캡처 Mock
│
├── benchmark_tests.cpp          # ⚡ 성능 벤치마크
└── [테스트 데이터 및 리소스]   # 🗂️ 테스트용 이미지, 설정
```

## GoogleTest/GoogleMock 활용

### 🧪 기본 테스트 구조

#### 단위 테스트 템플릿
```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "core/ConfigManager.h"

// 테스트 픽스처 클래스
class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 각 테스트 실행 전 초기화
        config_manager_ = std::make_unique<ConfigManager>();
        test_config_path_ = "test_config.json";
        CreateTestConfigFile();
    }
    
    void TearDown() override {
        // 각 테스트 실행 후 정리
        std::filesystem::remove(test_config_path_);
    }
    
    void CreateTestConfigFile() {
        // 테스트용 설정 파일 생성
        nlohmann::json test_config = {
            {"performance", {{"target_fps", 60}}},
            {"vision_algorithms", {{"hsv_detection", {{"enabled", true}}}}}
        };
        
        std::ofstream file(test_config_path_);
        file << test_config.dump(2);
    }
    
    std::unique_ptr<ConfigManager> config_manager_;
    std::string test_config_path_;
};

// 개별 테스트 케이스
TEST_F(ConfigManagerTest, LoadValidConfigFile) {
    // Arrange - 준비
    ASSERT_TRUE(std::filesystem::exists(test_config_path_));
    
    // Act - 실행
    bool result = config_manager_->LoadConfig(test_config_path_);
    
    // Assert - 검증
    EXPECT_TRUE(result);
    EXPECT_EQ(config_manager_->GetTargetFPS(), 60);
    EXPECT_TRUE(config_manager_->IsHSVDetectionEnabled());
}

TEST_F(ConfigManagerTest, HandleInvalidConfigFile) {
    // 유효하지 않은 설정 파일 처리 테스트
    std::string invalid_path = "nonexistent_config.json";
    
    bool result = config_manager_->LoadConfig(invalid_path);
    
    EXPECT_FALSE(result);
    // 기본값으로 폴백되었는지 확인
    EXPECT_EQ(config_manager_->GetTargetFPS(), 30); // 기본값
}
```

### 🎭 Mock 객체 활용

#### Mock 클래스 정의
```cpp
// mocks/MockDetector.h
#pragma once
#include <gmock/gmock.h>
#include "interfaces/IDetectionAlgorithm.h"

class MockDetectionAlgorithm : public IDetectionAlgorithm {
public:
    MOCK_METHOD(std::vector<DetectionResult>, detect, 
                (const cv::Mat& frame), (override));
    MOCK_METHOD(void, configure, 
                (const nlohmann::json& config), (override));
    MOCK_METHOD(bool, isInitialized, (), (const, override));
    MOCK_METHOD(std::string, getAlgorithmName, (), (const, override));
};

// 사용 예제
TEST_F(DetectionSystemTest, ProcessFrameWithMockDetector) {
    // Mock 객체 생성
    auto mock_detector = std::make_shared<MockDetectionAlgorithm>();
    
    // Mock 동작 설정
    DetectionResult expected_result{cv::Rect(100, 100, 50, 50), 0.95f, "object"};
    EXPECT_CALL(*mock_detector, detect(::testing::_))
        .WillOnce(::testing::Return(std::vector<DetectionResult>{expected_result}));
    
    EXPECT_CALL(*mock_detector, isInitialized())
        .WillRepeatedly(::testing::Return(true));
    
    // 테스트 실행
    detection_system_->SetDetector(mock_detector);
    cv::Mat test_frame = CreateTestFrame();
    
    auto results = detection_system_->ProcessFrame(test_frame);
    
    // 결과 검증
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].bounding_box, expected_result.bounding_box);
    EXPECT_FLOAT_EQ(results[0].confidence, expected_result.confidence);
}
```

### 🖼️ 컴퓨터 비전 테스트 특화

#### 이미지 기반 테스트
```cpp
// helpers/TestImageGenerator.cpp 활용
class ColorDetectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        detector_ = std::make_unique<ColorDetector>();
        image_generator_ = std::make_unique<TestImageGenerator>();
    }
    
    std::unique_ptr<ColorDetector> detector_;
    std::unique_ptr<TestImageGenerator> image_generator_;
};

TEST_F(ColorDetectorTest, DetectRedObjectInTestImage) {
    // 빨간색 사각형이 포함된 테스트 이미지 생성
    cv::Mat test_image = image_generator_->CreateImageWithRedRectangle(
        cv::Size(640, 480), cv::Rect(200, 150, 100, 80));
    
    // 빨간색 검출 설정
    ColorRange red_range{
        cv::Scalar(0, 120, 120),    // HSV 하한
        cv::Scalar(10, 255, 255)    // HSV 상한
    };
    detector_->SetColorRange(red_range);
    
    // 검출 실행
    auto results = detector_->detect(test_image);
    
    // 결과 검증
    ASSERT_FALSE(results.empty());
    
    // 검출된 영역이 실제 사각형과 유사한지 확인 (오차 허용)
    cv::Rect expected_rect(200, 150, 100, 80);
    cv::Rect detected_rect = results[0].bounding_box;
    
    EXPECT_NEAR(detected_rect.x, expected_rect.x, 10);
    EXPECT_NEAR(detected_rect.y, expected_rect.y, 10);
    EXPECT_NEAR(detected_rect.width, expected_rect.width, 20);
    EXPECT_NEAR(detected_rect.height, expected_rect.height, 20);
}

TEST_F(ColorDetectorTest, NoDetectionInEmptyImage) {
    // 빈 이미지 (검출 대상 없음)
    cv::Mat empty_image = cv::Mat::zeros(480, 640, CV_8UC3);
    
    auto results = detector_->detect(empty_image);
    
    EXPECT_TRUE(results.empty());
}
```

## 성능 및 벤치마크 테스트

### ⚡ 실시간 성능 요구사항 테스트

#### 성능 벤치마크 구조
```cpp
// benchmark_tests.cpp
#include <benchmark/benchmark.h>
#include <chrono>

class PerformanceBenchmark : public ::testing::Test {
protected:
    static constexpr int TARGET_FPS = 60;
    static constexpr double MAX_FRAME_TIME_MS = 1000.0 / TARGET_FPS; // ~16.67ms
    
    // 성능 측정 헬퍼
    template<typename Func>
    double MeasureExecutionTime(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / 1000.0; // ms로 변환
    }
};

TEST_F(PerformanceBenchmark, HSVColorDetectionPerformance) {
    auto detector = std::make_unique<HSVColorDetection>();
    cv::Mat test_frame = CreateTestFrame(1920, 1080); // Full HD
    
    // 워밍업 (캐시 로딩 등)
    for (int i = 0; i < 10; ++i) {
        detector->detect(test_frame);
    }
    
    // 실제 성능 측정 (10회 평균)
    double total_time = 0.0;
    const int num_iterations = 10;
    
    for (int i = 0; i < num_iterations; ++i) {
        double execution_time = MeasureExecutionTime([&]() {
            detector->detect(test_frame);
        });
        total_time += execution_time;
    }
    
    double average_time = total_time / num_iterations;
    
    // 성능 요구사항 검증
    EXPECT_LT(average_time, MAX_FRAME_TIME_MS) 
        << "HSV 검출이 너무 느립니다. 평균 " << average_time 
        << "ms (목표: < " << MAX_FRAME_TIME_MS << "ms)";
    
    // 성능 리포트 출력
    std::cout << "[성능] HSV 검출 평균 시간: " << average_time << "ms" << std::endl;
    std::cout << "[성능] 예상 FPS: " << (1000.0 / average_time) << std::endl;
}

TEST_F(PerformanceBenchmark, YOLOv11InferencePerformance) {
    // GPU 추론 성능 테스트 (CUDA 환경에서만)
    #ifdef CUDA_ENABLED
    auto yolo_detector = std::make_unique<YOLOv11TensorRTInference>();
    
    // GPU 워밍업
    cv::Mat gpu_test_frame = CreateTestFrame(640, 640); // YOLO 입력 크기
    for (int i = 0; i < 5; ++i) {
        yolo_detector->detect(gpu_test_frame);
    }
    
    double inference_time = MeasureExecutionTime([&]() {
        yolo_detector->detect(gpu_test_frame);
    });
    
    // GPU 추론은 더 관대한 시간 허용 (모델 복잡도 고려)
    EXPECT_LT(inference_time, 50.0) // 50ms 이하
        << "YOLO 추론이 너무 느립니다: " << inference_time << "ms";
    #else
    GTEST_SKIP() << "CUDA가 비활성화되어 YOLO 성능 테스트를 건너뜁니다.";
    #endif
}
```

### 📊 메모리 사용량 테스트
```cpp
TEST_F(PerformanceBenchmark, MemoryUsageTest) {
    // 메모리 사용량 모니터링 (Windows 전용)
    #ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    size_t initial_memory = pmc.WorkingSetSize;
    
    // 대량의 프레임 처리 시뮬레이션
    auto detector = std::make_unique<HSVColorDetection>();
    for (int i = 0; i < 100; ++i) {
        cv::Mat frame = CreateTestFrame(1920, 1080);
        detector->detect(frame);
        // 명시적으로 프레임 해제
        frame.release();
    }
    
    // 메모리 정리 후 측정
    cv::Mat().copyTo(cv::Mat()); // OpenCV 메모리 정리
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    size_t final_memory = pmc.WorkingSetSize;
    
    size_t memory_increase = final_memory - initial_memory;
    size_t max_allowed_increase = 50 * 1024 * 1024; // 50MB
    
    EXPECT_LT(memory_increase, max_allowed_increase)
        << "메모리 사용량이 과도하게 증가했습니다: " 
        << (memory_increase / 1024 / 1024) << "MB";
    #endif
}
```

## GUI 및 통합 테스트

### 🖥️ ImGui 인터페이스 테스트

#### GUI 컴포넌트 테스트
```cpp
// helpers/GLTestContext 활용
class MainInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 OpenGL 컨텍스트 생성
        gl_context_ = std::make_unique<GLTestContext>();
        ASSERT_TRUE(gl_context_->Initialize());
        
        // 테스트용 MainInterface 생성
        main_interface_ = std::make_unique<MainInterface>();
        main_interface_->Initialize();
    }
    
    void TearDown() override {
        main_interface_->Cleanup();
        gl_context_->Cleanup();
    }
    
    std::unique_ptr<GLTestContext> gl_context_;
    std::unique_ptr<MainInterface> main_interface_;
};

TEST_F(MainInterfaceTest, InitializationCreatesAllPanels) {
    // GUI 초기화 후 모든 패널이 생성되는지 확인
    main_interface_->Render();
    
    // ImGui 윈도우 존재 확인 (ImGui 내부 API 활용)
    EXPECT_TRUE(ImGui::FindWindowByName("Capture Settings") != nullptr);
    EXPECT_TRUE(ImGui::FindWindowByName("Detection Settings") != nullptr);
    EXPECT_TRUE(ImGui::FindWindowByName("Screen Preview") != nullptr);
    EXPECT_TRUE(ImGui::FindWindowByName("Performance Monitor") != nullptr);
    EXPECT_TRUE(ImGui::FindWindowByName("Console") != nullptr);
}

TEST_F(MainInterfaceTest, ConfigurationChangesReflectedInGUI) {
    // 설정 변경이 GUI에 반영되는지 테스트
    auto& config = ConfigManager::GetInstance();
    
    // 초기값 확인
    int initial_fps = config.GetTargetFPS();
    
    // GUI에서 설정 변경 시뮬레이션
    main_interface_->SetTargetFPS(120);
    main_interface_->Render();
    
    // 설정이 실제로 변경되었는지 확인
    EXPECT_EQ(config.GetTargetFPS(), 120);
    
    // 원래값으로 복원
    main_interface_->SetTargetFPS(initial_fps);
}
```

## 테스트 실행 및 CI/CD 통합

### 🚀 테스트 실행 방법

#### 로컬 개발 환경
```bash
# 전체 테스트 빌드
cmake --build build --config Release --parallel

# 모든 테스트 실행
ctest -C Release --test-dir build --verbose

# 특정 테스트만 실행
./build/bin/tests/test_ConfigManager.exe
./build/bin/tests/test_ColorDetector.exe

# 성능 벤치마크 실행
./build/bin/tests/benchmark_tests.exe

# 테스트 결과 XML 출력 (CI/CD용)
ctest -C Release --test-dir build --output-on-failure --output-junit test_results.xml
```

#### Visual Studio 통합
```bash
# Visual Studio Test Explorer에서 실행
# 테스트 > 테스트 탐색기에서 모든 테스트 확인 가능
# 개별 테스트 실행, 디버깅, 커버리지 측정 지원
```

### 🔄 CI/CD 파이프라인 통합

#### GitHub Actions 워크플로우 예제
```yaml
# .github/workflows/test.yml
name: Tests
on: [push, pull_request]

jobs:
  test:
    runs-on: windows-latest
    
    steps:
    - uses: actions/checkout@v3
      with:
        submodules: recursive
    
    - name: Setup CMake
      uses: lukka/get-cmake@latest
    
    - name: Configure Build
      run: cmake -B build -S ScreenMonitor -DCMAKE_BUILD_TYPE=Release
    
    - name: Build Project
      run: cmake --build build --config Release --parallel
    
    - name: Run Tests
      run: ctest -C Release --test-dir build --output-on-failure
    
    - name: Upload Test Results
      uses: actions/upload-artifact@v3
      if: always()
      with:
        name: test-results
        path: build/Testing/
```

### 📈 테스트 커버리지 측정

#### OpenCppCoverage (Windows)
```bash
# 커버리지 측정 도구 설치
# winget install OpenCppCoverage

# 커버리지 리포트 생성
OpenCppCoverage.exe --sources ScreenMonitor\src --export_type cobertura:coverage.xml -- ctest -C Debug --test-dir build
```

## 새로운 테스트 작성 가이드

### ✍️ 테스트 작성 체크리스트

#### 새로운 기능 테스트 추가
```cpp
// 1. 테스트 파일 생성: test_NewFeature.cpp
#include <gtest/gtest.h>
#include "새로운기능/NewFeature.h"

class NewFeatureTest : public ::testing::Test {
protected:
    void SetUp() override {
        feature_ = std::make_unique<NewFeature>();
    }
    
    std::unique_ptr<NewFeature> feature_;
};

// 2. 행복한 경로 테스트 (Happy Path)
TEST_F(NewFeatureTest, NormalOperationReturnsExpectedResult) {
    // 정상적인 입력에 대한 예상 동작 테스트
}

// 3. 경계값 테스트 (Boundary Tests)
TEST_F(NewFeatureTest, HandlesMinimumInputCorrectly) {
    // 최소값 입력 처리 테스트
}

TEST_F(NewFeatureTest, HandlesMaximumInputCorrectly) {
    // 최대값 입력 처리 테스트
}

// 4. 오류 조건 테스트 (Error Conditions)
TEST_F(NewFeatureTest, ThrowsExceptionOnInvalidInput) {
    // 잘못된 입력에 대한 예외 처리 테스트
    EXPECT_THROW(feature_->ProcessInvalidInput(), std::invalid_argument);
}

// 5. 성능 테스트 (Performance Test)
TEST_F(NewFeatureTest, PerformanceWithinAcceptableRange) {
    // 성능 요구사항 검증
}
```

#### Mock 객체 생성 가이드
```cpp
// 1. 인터페이스 분석
// 2. Mock 클래스 정의 (mocks/ 디렉토리)
// 3. MOCK_METHOD 매크로 사용
// 4. 테스트에서 EXPECT_CALL 설정
// 5. 실제 동작과 Mock 동작 일치성 확인
```

### 🎯 테스트 품질 가이드라인

#### AAA 패턴 준수
```cpp
TEST_F(ExampleTest, DescriptiveTestName) {
    // Arrange (준비) - 테스트 조건 설정
    auto input_data = CreateTestInput();
    auto expected_result = CalculateExpectedOutput(input_data);
    
    // Act (실행) - 테스트할 동작 수행
    auto actual_result = system_under_test_->ProcessData(input_data);
    
    // Assert (검증) - 결과 확인
    EXPECT_EQ(actual_result, expected_result);
    EXPECT_TRUE(system_under_test_->IsInValidState());
}
```

#### 테스트 명명 규칙
```cpp
// ✅ 좋은 테스트 이름: 동작_조건_예상결과
TEST_F(ColorDetectorTest, Detect_RedObjectInImage_ReturnsValidBoundingBox)
TEST_F(ConfigManagerTest, LoadConfig_WithInvalidPath_ReturnsFalse)
TEST_F(YOLODetectorTest, Inference_EmptyFrame_ReturnsEmptyResults)

// ❌ 나쁜 테스트 이름
TEST_F(ColorDetectorTest, Test1)
TEST_F(ConfigManagerTest, TestConfig)
```

## 품질 보증 매트릭스

### 📊 품질 지표 목표

| 항목 | 목표 | 측정 방법 |
|------|------|-----------|
| **단위 테스트 커버리지** | 80%+ | OpenCppCoverage |
| **통합 테스트 커버리지** | 60%+ | 수동 계산 |
| **성능 테스트** | 모든 핵심 알고리즘 | 벤치마크 실행 |
| **메모리 누수** | 0건 | 자동 테스트 |
| **테스트 실행 시간** | < 5분 | CI/CD 모니터링 |

### 🔍 품질 게이트 조건

#### PR 머지 조건
- [ ] **모든 테스트 통과**: 단위, 통합, 성능 테스트
- [ ] **커버리지 유지**: 새로운 코드 80%+ 커버리지
- [ ] **성능 저하 없음**: 벤치마크 결과 이전 버전 대비 5% 이내
- [ ] **메모리 안정성**: 메모리 누수 검사 통과
- [ ] **정적 분석**: 코드 품질 도구 검사 통과

---

**ScreenMonitor 테스트 시스템** - GoogleTest/Mock 기반 포괄적 품질 보증  
TDD 지향 개발 | 성능 중심 검증 | CI/CD 완전 통합