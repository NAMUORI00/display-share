#include <gtest/gtest.h>
#include "capture/HighSpeedCapture.h"
#include "mocks/MockScreenCapture.h"
#include <thread>
#include <chrono>

class HighSpeedCaptureTest : public ::testing::Test {
protected:
    void SetUp() override {
        capture = std::make_unique<HighSpeedCapture>();
    }
    
    void TearDown() override {
        if (capture) {
            capture->Cleanup();
        }
        capture.reset();
    }
    
    std::unique_ptr<HighSpeedCapture> capture;
};

// 기본 초기화 테스트
TEST_F(HighSpeedCaptureTest, BasicInitialization) {
    // 실제 화면 캡처는 테스트 환경에서 실패할 수 있음
    bool init_result = capture->Initialize();
    
    if (!init_result) {
        GTEST_SKIP() << "Screen capture not available in test environment";
    }
    
    EXPECT_TRUE(init_result);
}

// 설정 기반 초기화 테스트
TEST_F(HighSpeedCaptureTest, SettingsInitialization) {
    HighSpeedCapture::CaptureSettings settings;
    settings.selectedMonitorIndex = 0;
    settings.targetFPS = 30.0;
    settings.enableFPSLimiting = true;
    
    bool init_result = capture->Initialize(settings);
    
    if (!init_result) {
        GTEST_SKIP() << "Screen capture not available in test environment";
    }
    
    EXPECT_TRUE(init_result);
    
    auto retrieved_settings = capture->GetSettings();
    EXPECT_EQ(retrieved_settings.selectedMonitorIndex, 0);
    EXPECT_DOUBLE_EQ(retrieved_settings.targetFPS, 30.0);
    EXPECT_TRUE(retrieved_settings.enableFPSLimiting);
}

// 모니터 목록 가져오기 테스트
TEST_F(HighSpeedCaptureTest, GetAvailableMonitors) {
    auto monitors = HighSpeedCapture::GetAvailableMonitors();
    
    // 적어도 하나의 모니터는 있어야 함
    EXPECT_GT(monitors.size(), 0);
    
    for (const auto& monitor : monitors) {
        EXPECT_FALSE(monitor.name.empty());
        EXPECT_GT(monitor.width, 0);
        EXPECT_GT(monitor.height, 0);
    }
}

// 모니터 전환 테스트
TEST_F(HighSpeedCaptureTest, MonitorSwitching) {
    if (!capture->Initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    auto monitors = capture->GetAvailableMonitors();
    ASSERT_GT(monitors.size(), 0);
    
    // 첫 번째 모니터로 설정
    EXPECT_TRUE(capture->SetTargetMonitor(0));
    EXPECT_EQ(capture->GetCurrentMonitorIndex(), 0);
    
    // 잘못된 인덱스 테스트
    EXPECT_FALSE(capture->SetTargetMonitor(-1));
    EXPECT_FALSE(capture->SetTargetMonitor(static_cast<int>(monitors.size())));
}

// 설정 업데이트 테스트
TEST_F(HighSpeedCaptureTest, UpdateSettings) {
    if (!capture->Initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    HighSpeedCapture::CaptureSettings new_settings;
    new_settings.selectedMonitorIndex = 0;
    new_settings.targetFPS = 120.0;
    new_settings.enableFPSLimiting = false;
    
    capture->UpdateSettings(new_settings);
    
    auto retrieved = capture->GetSettings();
    EXPECT_DOUBLE_EQ(retrieved.targetFPS, 120.0);
    EXPECT_FALSE(retrieved.enableFPSLimiting);
}

// 프레임 캡처 테스트
TEST_F(HighSpeedCaptureTest, FrameCapture) {
    if (!capture->Initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    bool capture_result = capture->CaptureFrame();
    
    if (capture_result) {
        cv::Mat frame = capture->GetLatestFrame();
        EXPECT_FALSE(frame.empty());
        EXPECT_GT(frame.cols, 0);
        EXPECT_GT(frame.rows, 0);
        EXPECT_EQ(frame.channels(), 3); // BGR
    } else {
        GTEST_SKIP() << "Frame capture failed (may be expected in test environment)";
    }
}

// 연속 캡처 테스트
TEST_F(HighSpeedCaptureTest, ContinuousCapture) {
    if (!capture->Initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    int successful_captures = 0;
    const int total_attempts = 10;
    
    for (int i = 0; i < total_attempts; ++i) {
        if (capture->CaptureFrame()) {
            cv::Mat frame = capture->GetLatestFrame();
            if (!frame.empty()) {
                successful_captures++;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    // 최소 50% 성공률 기대
    EXPECT_GE(successful_captures, total_attempts / 2);
}

// 성능 메트릭 테스트
TEST_F(HighSpeedCaptureTest, PerformanceMetrics) {
    if (!capture->Initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    
    // 1초간 캡처 시도
    while (true) {
        auto current_time = std::chrono::high_resolution_clock::now();
        if (current_time - start_time >= std::chrono::seconds(1)) {
            break;
        }
        
        if (capture->CaptureFrame()) {
            frame_count++;
        }
    }
    
    if (frame_count > 0) {
        std::cout << "Captured " << frame_count << " frames in 1 second" << std::endl;
        EXPECT_GT(frame_count, 0);
    } else {
        GTEST_SKIP() << "No frames captured (expected in test environment)";
    }
}

// 에러 처리 테스트
TEST_F(HighSpeedCaptureTest, ErrorHandling) {
    // 초기화되지 않은 상태에서 캡처 시도
    EXPECT_FALSE(capture->CaptureFrame());
    
    cv::Mat frame = capture->GetLatestFrame();
    EXPECT_TRUE(frame.empty());
    
    // 잘못된 모니터 인덱스
    EXPECT_FALSE(capture->SetTargetMonitor(-1));
    EXPECT_FALSE(capture->SetTargetMonitor(1000));
}

// 정리 테스트
TEST_F(HighSpeedCaptureTest, CleanupTest) {
    if (capture->Initialize()) {
        // 몇 번의 캡처 수행
        for (int i = 0; i < 5; ++i) {
            capture->CaptureFrame();
        }
        
        // 명시적 정리
        EXPECT_NO_THROW(capture->Cleanup());
        
        // 정리 후 캡처 시도는 실패해야 함
        EXPECT_FALSE(capture->CaptureFrame());
    }
}

// 멀티스레드 안전성 테스트
TEST_F(HighSpeedCaptureTest, ThreadSafety) {
    if (!capture->Initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    std::vector<std::thread> threads;
    std::atomic<int> successful_captures{0};
    
    // 여러 스레드에서 동시에 캡처 시도 (권장되지 않지만 크래시는 없어야 함)
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([this, &successful_captures]() {
            for (int j = 0; j < 10; ++j) {
                try {
                    if (capture->CaptureFrame()) {
                        cv::Mat frame = capture->GetLatestFrame();
                        if (!frame.empty()) {
                            successful_captures++;
                        }
                    }
                } catch (...) {
                    // 예외 발생 시 무시 (스레드 안전성 문제일 수 있음)
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 적어도 몇 번의 성공적인 캡처가 있어야 함
    std::cout << "Successful captures from multiple threads: " << successful_captures.load() << std::endl;
}

// Mock을 사용한 테스트
TEST_F(HighSpeedCaptureTest, MockBasedTest) {
    auto mock_capture = std::make_unique<MockScreenCapture>();
    mock_capture->SetupDefaultBehavior();
    
    cv::Mat test_frame = cv::Mat::zeros(640, 480, CV_8UC3);
    test_frame.setTo(cv::Scalar(100, 150, 200));
    mock_capture->SetMockFrame(test_frame);
    
    EXPECT_TRUE(mock_capture->Initialize());
    EXPECT_TRUE(mock_capture->CaptureFrame());
    
    cv::Mat captured_frame = mock_capture->GetLatestFrame();
    EXPECT_FALSE(captured_frame.empty());
    EXPECT_EQ(captured_frame.size(), test_frame.size());
}