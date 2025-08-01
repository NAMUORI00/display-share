# Testing Strategies

C_capture 프로젝트의 종합적인 테스트 전략 및 품질 보증 방법론

## 🎯 테스트 철학 및 원칙

### 핵심 원칙
1. **실시간 성능 보장**: 모든 테스트가 성능 기준(30+ FPS)을 검증
2. **모듈별 격리**: 각 컴포넌트를 독립적으로 테스트
3. **실제 환경 시뮬레이션**: 프로덕션 환경과 유사한 조건에서 테스트
4. **자동화 우선**: 수동 테스트 최소화, CI/CD 통합

### 테스트 피라미드 구조
```
       /\
      /  \     E2E Tests (10%)
     /____\    - 전체 파이프라인 통합 테스트
    /      \   
   /        \  Integration Tests (20%)
  /          \ - 모듈 간 연동 테스트
 /____________\
 Unit Tests (70%) - 개별 컴포넌트 테스트
```

---

## 🧪 테스트 계층별 전략

### Layer 1: 단위 테스트 (Unit Tests)

#### 테스트 범위
각 클래스와 함수의 독립적 기능 검증

#### 주요 테스트 케이스

##### ConfigManager 테스트
```cpp
class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_config_file_ = createTempConfigFile();
        config_manager_ = std::make_unique<ConfigManager>(temp_config_file_);
    }
    
    void TearDown() override {
        std::filesystem::remove(temp_config_file_);
    }
};

TEST_F(ConfigManagerTest, LoadValidConfig) {
    EXPECT_TRUE(config_manager_->load());
    EXPECT_EQ(config_manager_->getROIWidth(), 320);
    EXPECT_EQ(config_manager_->getROIHeight(), 320);
}
```

##### 성능 기준 테스트
```cpp
TEST_F(ObjectDetectorTest, DetectionPerformance) {
    cv::Mat test_image = createTestImage(320, 320);
    
    auto start = std::chrono::high_resolution_clock::now();
    auto results = detector_->detect(test_image);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 15);  // 15ms 이하
}
```

#### Mock 객체 활용
```cpp
class MockScreenCapture : public IScreenCapture {
public:
    MOCK_METHOD(bool, captureScreen, (cv::Mat& output), (override));
    MOCK_METHOD(bool, captureROI, (cv::Mat& output, int x, int y, int width, int height), (override));
};

TEST_F(MainInterfaceTest, HandleCaptureFailure) {
    auto mock_capture = std::make_unique<MockScreenCapture>();
    EXPECT_CALL(*mock_capture, captureROI(_, _, _, _, _))
        .WillOnce(Return(false));
    
    // 캡처 실패 시 적절한 에러 처리 검증
    interface_->setScreenCapture(std::move(mock_capture));
    EXPECT_FALSE(interface_->processFrame());
}
```

### Layer 2: 통합 테스트 (Integration Tests)

#### 테스트 목표
모듈 간 데이터 흐름 및 협력 검증

#### 파이프라인 통합 테스트
```cpp
class PipelineIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 실제 구현체들을 조합한 파이프라인 구성
        capture_ = std::make_unique<ScreenCaptureLiteDevice>();
        detector_ = std::make_unique<ObjectDetector>(
            std::make_unique<YOLOv11TensorRTInference>()
        );
        interface_ = std::make_unique<MainInterface>(
            std::move(capture_), std::move(detector_)
        );
    }
};

TEST_F(PipelineIntegrationTest, EndToEndPerformance) {
    const int frame_count = 100;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < frame_count; ++i) {
        EXPECT_TRUE(interface_->processFrame());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 평균 30 FPS 이상 달성 확인
    double avg_fps = (frame_count * 1000.0) / total_duration.count();
    EXPECT_GT(avg_fps, 30.0);
}
```

#### 메모리 누수 테스트
```cpp
TEST_F(PipelineIntegrationTest, MemoryLeakDetection) {
    size_t initial_memory = getCurrentMemoryUsage();
    
    // 장시간 실행 시뮬레이션
    for (int i = 0; i < 1000; ++i) {
        interface_->processFrame();
        
        if (i % 100 == 0) {
            // 주기적으로 메모리 사용량 확인
            size_t current_memory = getCurrentMemoryUsage();
            EXPECT_LT(current_memory - initial_memory, 10 * 1024 * 1024);  // 10MB 이하 증가
        }
    }
}
```

### Layer 3: 종단간 테스트 (E2E Tests)

#### 테스트 시나리오
실제 사용자 워크플로우 시뮬레이션

#### GUI 상호작용 테스트
```cpp
class E2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // 완전한 애플리케이션 환경 구성
        app_ = std::make_unique<Application>();
        app_->initialize();
    }
};

TEST_F(E2ETest, UserWorkflowSimulation) {
    // 1. 애플리케이션 시작
    EXPECT_TRUE(app_->start());
    
    // 2. ROI 설정
    app_->setROI(100, 100, 320, 320);
    
    // 3. 검출 시작
    app_->startDetection();
    
    // 4. 실시간 처리 검증
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    auto stats = app_->getPerformanceStats();
    EXPECT_GT(stats.avg_fps, 30.0);
    EXPECT_LT(stats.avg_latency_ms, 33.0);
    
    // 5. 정상 종료
    app_->stop();
}
```

---

## ⚡ 성능 테스트 전략

### 벤치마킹 프레임워크

#### 마이크로 벤치마크
```cpp
class PerformanceBenchmark : public ::testing::Test {
protected:
    struct BenchmarkResult {
        double min_time_ms;
        double max_time_ms;
        double avg_time_ms;
        double std_dev_ms;
    };
    
    BenchmarkResult runBenchmark(std::function<void()> func, int iterations = 1000) {
        std::vector<double> times;
        times.reserve(iterations);
        
        for (int i = 0; i < iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            func();
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration<double, std::milli>(end - start);
            times.push_back(duration.count());
        }
        
        return calculateStatistics(times);
    }
};

TEST_F(PerformanceBenchmark, YOLOInferenceBenchmark) {
    cv::Mat test_image = loadTestImage("test_320x320.jpg");
    
    auto result = runBenchmark([&]() {
        detector_->detect(test_image);
    });
    
    EXPECT_LT(result.avg_time_ms, 10.0);    // 평균 10ms 이하
    EXPECT_LT(result.max_time_ms, 15.0);    // 최대 15ms 이하
    EXPECT_LT(result.std_dev_ms, 2.0);      // 표준편차 2ms 이하 (일관성)
}
```

### 부하 테스트

#### 스트레스 테스트
```cpp
TEST_F(PipelineIntegrationTest, StressTest) {
    const int duration_minutes = 10;
    const auto end_time = std::chrono::steady_clock::now() + 
                         std::chrono::minutes(duration_minutes);
    
    size_t frame_count = 0;
    size_t error_count = 0;
    
    while (std::chrono::steady_clock::now() < end_time) {
        if (!interface_->processFrame()) {
            ++error_count;
        }
        ++frame_count;
        
        // 과도한 CPU 사용 방지
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    double error_rate = static_cast<double>(error_count) / frame_count;
    EXPECT_LT(error_rate, 0.001);  // 에러율 0.1% 이하
}
```

---

## 🤖 자동화된 테스트 환경

### CI/CD 통합

#### GitHub Actions 설정
```yaml
name: Automated Testing

on: [push, pull_request]

jobs:
  test:
    runs-on: windows-latest
    
    steps:
    - uses: actions/checkout@v3
      with:
        submodules: recursive
        
    - name: Setup CUDA
      uses: Jimver/cuda-toolkit@v0.2.8
      
    - name: Configure CMake
      run: cmake -B build -S ScreenMonitor -DCMAKE_BUILD_TYPE=Release
      
    - name: Build
      run: cmake --build build --config Release --parallel
      
    - name: Run Unit Tests
      run: ctest -C Release --test-dir build --output-on-failure
      
    - name: Run Performance Tests
      run: ./build/bin/Release/PerformanceTests.exe
      
    - name: Upload Test Results
      uses: actions/upload-artifact@v3
      with:
        name: test-results
        path: build/test-results/
```

### 자동화된 성능 회귀 검출

#### 성능 기준선 관리
```cpp
class PerformanceRegression : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        baseline_metrics_ = loadBaselineMetrics("performance_baseline.json");
    }
    
    void checkRegression(const std::string& test_name, double current_value) {
        auto baseline = baseline_metrics_[test_name];
        double regression_threshold = baseline * 1.1;  // 10% 허용
        
        EXPECT_LT(current_value, regression_threshold) 
            << "Performance regression detected in " << test_name 
            << ": " << current_value << "ms vs baseline " << baseline << "ms";
    }
};
```

---

## 📊 테스트 커버리지 및 품질 메트릭

### 코드 커버리지 목표
- **단위 테스트**: 90% 이상
- **통합 테스트**: 주요 파이프라인 100%
- **전체 커버리지**: 85% 이상

### 품질 게이트
```cpp
// 각 테스트에서 검증해야 할 품질 기준
struct QualityGates {
    static constexpr double MAX_INFERENCE_TIME_MS = 15.0;
    static constexpr double MIN_FPS = 30.0;
    static constexpr size_t MAX_MEMORY_USAGE_MB = 100;
    static constexpr double MAX_ERROR_RATE = 0.001;
    static constexpr double MAX_CPU_USAGE_PERCENT = 80.0;
};
```

### 성능 트렌드 추적
```python
# 성능 메트릭 수집 및 시각화 스크립트
import json
import matplotlib.pyplot as plt
from datetime import datetime

def track_performance_trend():
    """성능 메트릭을 시간에 따라 추적"""
    results = load_test_results()
    
    dates = [datetime.fromisoformat(r['timestamp']) for r in results]
    fps_values = [r['avg_fps'] for r in results]
    latency_values = [r['avg_latency_ms'] for r in results]
    
    plt.figure(figsize=(12, 6))
    
    plt.subplot(1, 2, 1)
    plt.plot(dates, fps_values)
    plt.axhline(y=30, color='r', linestyle='--', label='Minimum Target')
    plt.title('FPS Trend')
    plt.ylabel('FPS')
    
    plt.subplot(1, 2, 2)
    plt.plot(dates, latency_values)
    plt.axhline(y=33, color='r', linestyle='--', label='Maximum Target')
    plt.title('Latency Trend')
    plt.ylabel('Latency (ms)')
    
    plt.tight_layout()
    plt.savefig('performance_trend.png')
```

---

## 🔧 테스트 도구 및 헬퍼

### 테스트 데이터 생성
```cpp
class TestDataGenerator {
public:
    static cv::Mat createTestImage(int width, int height, int pattern = 0) {
        cv::Mat image(height, width, CV_8UC3);
        
        switch (pattern) {
        case 0: // 체스보드 패턴
            createChessboardPattern(image);
            break;
        case 1: // 노이즈 패턴
            cv::randu(image, cv::Scalar::all(0), cv::Scalar::all(255));
            break;
        case 2: // 그라디언트 패턴
            createGradientPattern(image);
            break;
        }
        
        return image;
    }
    
    static std::vector<Detection> createMockDetections(int count) {
        std::vector<Detection> detections;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 320);
        
        for (int i = 0; i < count; ++i) {
            Detection det;
            det.bbox = cv::Rect(dis(gen), dis(gen), 50, 50);
            det.confidence = 0.9f;
            det.class_id = i % 10;
            detections.push_back(det);
        }
        
        return detections;
    }
};
```

### 성능 측정 유틸리티
```cpp
class PerformanceProfiler {
    std::unordered_map<std::string, std::vector<double>> measurements_;
    
public:
    class ScopedTimer {
        PerformanceProfiler& profiler_;
        std::string name_;
        std::chrono::high_resolution_clock::time_point start_;
        
    public:
        ScopedTimer(PerformanceProfiler& profiler, const std::string& name)
            : profiler_(profiler), name_(name)
            , start_(std::chrono::high_resolution_clock::now()) {}
        
        ~ScopedTimer() {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(end - start_);
            profiler_.record(name_, duration.count());
        }
    };
    
    void record(const std::string& name, double value) {
        measurements_[name].push_back(value);
    }
    
    void generateReport() const {
        for (const auto& [name, values] : measurements_) {
            double avg = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
            double max_val = *std::max_element(values.begin(), values.end());
            double min_val = *std::min_element(values.begin(), values.end());
            
            std::cout << name << ": avg=" << avg << "ms, min=" << min_val 
                     << "ms, max=" << max_val << "ms" << std::endl;
        }
    }
};

// 사용법
#define PROFILE_SCOPE(profiler, name) \
    PerformanceProfiler::ScopedTimer timer(profiler, name)
```

---

## 📋 테스트 실행 가이드

### 로컬 개발 환경
```bash
# 전체 테스트 실행
cmake --build build --config Release
ctest -C Release --test-dir build --verbose

# 특정 테스트 그룹 실행
ctest -C Release --test-dir build -R "Unit.*"
ctest -C Release --test-dir build -R "Integration.*"
ctest -C Release --test-dir build -R "Performance.*"

# 병렬 테스트 실행
ctest -C Release --test-dir build --parallel 4
```

### 성능 테스트 실행
```bash
# 벤치마크 테스트 (더 긴 시간 소요)
./build/bin/Release/BenchmarkTests.exe

# 스트레스 테스트 (10분간 실행)
./build/bin/Release/StressTests.exe --duration=600
```

---

**관련 문서**
- [최적화 이력](optimization-history.md) - 성능 개선 과정
- [아키텍처 결정사항](architecture-decisions.md) - 테스트 가능한 설계
- [성능 목표](../quick-reference/performance-targets/) - 테스트 기준점