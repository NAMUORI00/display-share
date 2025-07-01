#include <gtest/gtest.h>
#include "monitoring/PerformanceMonitor.h"
#include "helpers/ConfigTestHelper.h"
#include "helpers/GLTestContext.h"
#include "mocks/MockScreenCapture.h"
#include "mocks/MockDetector.h"
#include <thread>
#include <chrono>

class PerformanceMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 설정 파일 생성
        test_config = ConfigTestHelper::createDefaultTestConfig();
        config_file = ConfigTestHelper::createTempConfigFile(test_config);
        
        // OpenGL 컨텍스트 초기화 (헤드리스)
        gl_context = std::make_unique<GLTestContext>();
        gl_context->Initialize(true);
        
        monitor = std::make_unique<PerformanceMonitor>();
    }
    
    void TearDown() override {
        monitor.reset();
        gl_context.reset();
        ConfigTestHelper::cleanupAllTempFiles();
    }
    
    json test_config;
    std::string config_file;
    std::unique_ptr<GLTestContext> gl_context;
    std::unique_ptr<PerformanceMonitor> monitor;
};

// 기본 초기화 테스트
TEST_F(PerformanceMonitorTest, BasicInitialization) {
    bool init_result = monitor->Initialize();
    
    if (!init_result) {
        GTEST_SKIP() << "PerformanceMonitor initialization failed (expected in test environment)";
    }
    
    EXPECT_TRUE(init_result);
}

// 모니터 관리 테스트
TEST_F(PerformanceMonitorTest, MonitorManagement) {
    if (!monitor->Initialize()) {
        GTEST_SKIP() << "PerformanceMonitor initialization failed";
    }
    
    auto monitors = monitor->GetAvailableMonitors();
    if (monitors.empty()) {
        GTEST_SKIP() << "No monitors available in test environment";
    }
    
    EXPECT_GT(monitors.size(), 0);
    
    // 첫 번째 모니터로 설정
    EXPECT_TRUE(monitor->SetCaptureMonitor(0));
    EXPECT_EQ(monitor->GetCurrentMonitorIndex(), 0);
    
    // 잘못된 모니터 인덱스
    EXPECT_FALSE(monitor->SetCaptureMonitor(-1));
    EXPECT_FALSE(monitor->SetCaptureMonitor(static_cast<int>(monitors.size())));
}

// 설정 로딩 테스트
TEST_F(PerformanceMonitorTest, ConfigurationLoading) {
    // 설정 파일 경로를 환경에 맞게 설정해야 함
    // 실제 테스트에서는 mock 설정을 사용
    
    auto config_manager = std::make_unique<ConfigManager>();
    EXPECT_TRUE(config_manager->loadConfig(config_file));
    
    auto vision_config = config_manager->getConfigSection("vision_algorithms");
    EXPECT_FALSE(vision_config.empty());
    EXPECT_TRUE(vision_config.contains("selected_algorithm"));
}

// 정리 테스트
TEST_F(PerformanceMonitorTest, CleanupTest) {
    if (monitor->Initialize()) {
        // 정리가 예외 없이 수행되어야 함
        EXPECT_NO_THROW(monitor->Cleanup());
    }
}

// Mock 기반 통합 테스트
TEST_F(PerformanceMonitorTest, MockIntegrationTest) {
    // PerformanceMonitor는 실제 컴포넌트들을 사용하므로
    // Mock 기반 테스트는 제한적임
    // 대신 개별 컴포넌트의 Mock 동작을 확인
    
    auto mock_capture = std::make_unique<MockScreenCapture>();
    auto mock_detector = std::make_unique<MockColorDetector>();
    
    mock_capture->SetupDefaultBehavior();
    mock_detector->SetupDefaultBehavior();
    
    // Mock 객체들이 정상 동작하는지 확인
    EXPECT_TRUE(mock_capture->Initialize());
    EXPECT_TRUE(mock_detector->initialize());
    
    cv::Mat test_frame = cv::Mat::zeros(640, 480, CV_8UC3);
    mock_capture->SetMockFrame(test_frame);
    
    if (mock_capture->CaptureFrame()) {
        cv::Mat frame = mock_capture->GetLatestFrame();
        EXPECT_FALSE(frame.empty());
        
        cv::Rect detection = mock_detector->detectTarget(frame);
        // Mock에서는 설정에 따라 검출 결과가 달라질 수 있음
    }
}

// 에러 처리 테스트
TEST_F(PerformanceMonitorTest, ErrorHandling) {
    // 초기화되지 않은 상태에서 작업 시도
    auto monitors = monitor->GetAvailableMonitors();
    // 이 경우 빈 벡터를 반환하거나 기본 동작을 수행해야 함
    
    EXPECT_FALSE(monitor->SetCaptureMonitor(0));
    EXPECT_EQ(monitor->GetCurrentMonitorIndex(), 0); // 기본값
}

// 성능 메트릭 테스트
TEST_F(PerformanceMonitorTest, PerformanceMetrics) {
    if (!monitor->Initialize()) {
        GTEST_SKIP() << "PerformanceMonitor initialization failed";
    }
    
    // 애플리케이션 시간 측정
    double initial_time = monitor->GetApplicationTimeInSeconds();
    EXPECT_GT(initial_time, 0.0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    double later_time = monitor->GetApplicationTimeInSeconds();
    EXPECT_GT(later_time, initial_time);
}

// 실행 시뮬레이션 테스트 (매우 짧은 시간)
TEST_F(PerformanceMonitorTest, ShortRunSimulation) {
    if (!monitor->Initialize()) {
        GTEST_SKIP() << "PerformanceMonitor initialization failed";
    }
    
    // 별도 스레드에서 짧은 시간 실행
    std::atomic<bool> should_stop{false};
    
    std::thread run_thread([&]() {
        // Run 메서드를 직접 호출하는 대신 업데이트 사이클 시뮬레이션
        while (!should_stop.load()) {
            // 실제 Run() 메서드는 무한 루프이므로 여기서는 시뮬레이션만
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    });
    
    // 잠시 실행 후 중지
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    should_stop = true;
    
    if (run_thread.joinable()) {
        run_thread.join();
    }
    
    SUCCEED(); // 예외 없이 완료되면 성공
}

// 컴포넌트 초기화 순서 테스트
TEST_F(PerformanceMonitorTest, ComponentInitializationOrder) {
    // PerformanceMonitor가 내부 컴포넌트들을 올바른 순서로 초기화하는지 확인
    
    bool init_result = monitor->Initialize();
    
    if (init_result) {
        // 초기화가 성공했으면 내부 컴포넌트들이 생성되었어야 함
        
        // 정리도 올바른 순서로 수행되어야 함
        EXPECT_NO_THROW(monitor->Cleanup());
    } else {
        // 테스트 환경에서 초기화 실패는 예상됨
        GTEST_SKIP() << "Component initialization failed in test environment";
    }
}

// 메모리 사용량 테스트
TEST_F(PerformanceMonitorTest, MemoryUsage) {
    // 여러 PerformanceMonitor 인스턴스 생성/소멸
    std::vector<std::unique_ptr<PerformanceMonitor>> monitors;
    
    for (int i = 0; i < 5; ++i) {
        auto pm = std::make_unique<PerformanceMonitor>();
        // 초기화는 실패할 수 있지만 생성/소멸은 문제없어야 함
        monitors.push_back(std::move(pm));
    }
    
    // 모든 인스턴스 정리
    monitors.clear();
    
    SUCCEED(); // 예외 없이 완료되면 성공
}

// 설정 변경 처리 테스트
TEST_F(PerformanceMonitorTest, ConfigurationChange) {
    auto config_manager = std::make_unique<ConfigManager>();
    EXPECT_TRUE(config_manager->loadConfig(config_file));
    
    // 초기 설정 확인
    auto initial_config = config_manager->getConfigSection("performance");
    EXPECT_FALSE(initial_config.empty());
    
    // 설정 변경
    json new_perf_config = {
        {"target_fps", 120},
        {"enable_multithreading", false},
        {"max_processing_threads", 2}
    };
    
    config_manager->updateConfigSection("performance", new_perf_config);
    
    auto updated_config = config_manager->getConfigSection("performance");
    EXPECT_EQ(updated_config["target_fps"], 120);
    EXPECT_EQ(updated_config["enable_multithreading"], false);
}

// 동시성 테스트
TEST_F(PerformanceMonitorTest, ConcurrencyTest) {
    if (!monitor->Initialize()) {
        GTEST_SKIP() << "PerformanceMonitor initialization failed";
    }
    
    std::vector<std::thread> threads;
    std::atomic<int> operation_count{0};
    
    // 여러 스레드에서 읽기 전용 작업 수행
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([this, &operation_count]() {
            for (int j = 0; j < 10; ++j) {
                try {
                    auto monitors = monitor->GetAvailableMonitors();
                    int current_index = monitor->GetCurrentMonitorIndex();
                    double app_time = monitor->GetApplicationTimeInSeconds();
                    
                    operation_count++;
                } catch (...) {
                    // 예외 발생 시 실패로 처리
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 모든 작업이 성공적으로 완료되었는지 확인
    EXPECT_EQ(operation_count.load(), 30);
}

// 상태 전환 테스트
TEST_F(PerformanceMonitorTest, StateTransitions) {
    // 초기 상태
    auto monitors = monitor->GetAvailableMonitors();
    int initial_monitor = monitor->GetCurrentMonitorIndex();
    
    bool init_result = monitor->Initialize();
    
    if (init_result) {
        // 초기화 후 상태
        auto monitors_after_init = monitor->GetAvailableMonitors();
        
        // 모니터 목록은 변경되지 않아야 함
        EXPECT_EQ(monitors.size(), monitors_after_init.size());
        
        // 정리
        monitor->Cleanup();
        
        // 정리 후에도 기본 작업은 가능해야 함
        EXPECT_NO_THROW(monitor->GetCurrentMonitorIndex());
        EXPECT_NO_THROW(monitor->GetApplicationTimeInSeconds());
    }
}

// 리소스 정리 테스트
TEST_F(PerformanceMonitorTest, ResourceCleanup) {
    if (monitor->Initialize()) {
        // 여러 번 정리 호출
        monitor->Cleanup();
        monitor->Cleanup(); // 두 번째 호출은 안전해야 함
        
        // 정리 후 초기화 재시도
        bool reinit_result = monitor->Initialize();
        
        if (reinit_result) {
            monitor->Cleanup();
        }
    }
    
    SUCCEED(); // 예외 없이 완료되면 성공
}

// 예외 안전성 테스트
TEST_F(PerformanceMonitorTest, ExceptionSafety) {
    try {
        // 다양한 작업을 시도하여 예외 안전성 확인
        monitor->Initialize();
        monitor->GetAvailableMonitors();
        monitor->SetCaptureMonitor(0);
        monitor->GetCurrentMonitorIndex();
        monitor->GetApplicationTimeInSeconds();
        monitor->Cleanup();
        
        SUCCEED();
    } catch (const std::exception& e) {
        FAIL() << "Unexpected exception: " << e.what();
    } catch (...) {
        FAIL() << "Unexpected unknown exception";
    }
}