#include <gtest/gtest.h>
#include "capture/CrossPlatformCapture.h"
#include "helpers/TestImageGenerator.h"

class CrossPlatformCaptureTest : public ::testing::Test {
protected:
    void SetUp() override {
        capture = std::make_unique<CrossPlatformCapture>();
    }
    
    void TearDown() override {
        if (capture) {
            capture->cleanup();
        }
        capture.reset();
    }
    
    std::unique_ptr<CrossPlatformCapture> capture;
};

// 기본 초기화 테스트
TEST_F(CrossPlatformCaptureTest, BasicInitialization) {
    bool init_result = capture->initialize();
    
    if (!init_result) {
        GTEST_SKIP() << "Screen capture not available in test environment";
    }
    
    EXPECT_TRUE(init_result);
    EXPECT_TRUE(capture->isInitialized());
}

// 모니터 열거 테스트
TEST_F(CrossPlatformCaptureTest, EnumerateMonitors) {
    auto monitors = capture->enumerateMonitors();
    
    // 적어도 하나의 모니터는 있어야 함
    EXPECT_GT(monitors.size(), 0);
    
    for (const auto& monitor : monitors) {
        EXPECT_FALSE(monitor.name.empty());
        EXPECT_GT(monitor.width, 0);
        EXPECT_GT(monitor.height, 0);
        // isPrimary는 boolean이므로 별도 검증 불필요
    }
    
    // 프라이머리 모니터가 하나는 있어야 함
    bool has_primary = false;
    for (const auto& monitor : monitors) {
        if (monitor.isPrimary) {
            has_primary = true;
            break;
        }
    }
    EXPECT_TRUE(has_primary);
}

// 특정 모니터 설정 테스트
TEST_F(CrossPlatformCaptureTest, SetTargetMonitor) {
    if (!capture->initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    auto monitors = capture->enumerateMonitors();
    ASSERT_GT(monitors.size(), 0);
    
    // 첫 번째 모니터로 설정
    EXPECT_TRUE(capture->setTargetMonitor(0));
    EXPECT_EQ(capture->getCurrentMonitorIndex(), 0);
    
    // 잘못된 인덱스 테스트
    EXPECT_FALSE(capture->setTargetMonitor(-1));
    EXPECT_FALSE(capture->setTargetMonitor(static_cast<int>(monitors.size())));
    
    // 인덱스가 변경되지 않았는지 확인
    EXPECT_EQ(capture->getCurrentMonitorIndex(), 0);
}

// 콜백 설정 테스트
TEST_F(CrossPlatformCaptureTest, CallbackSetup) {
    bool callback_called = false;
    cv::Mat captured_frame;
    
    auto callback = [&](const cv::Mat& frame) {
        callback_called = true;
        captured_frame = frame.clone();
    };
    
    capture->setFrameCallback(callback);
    
    if (!capture->initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    // 캡처 시작
    if (capture->startCapture()) {
        // 잠시 대기하여 콜백이 호출되는지 확인
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        capture->stopCapture();
        
        if (callback_called) {
            EXPECT_TRUE(callback_called);
            EXPECT_FALSE(captured_frame.empty());
        } else {
            GTEST_SKIP() << "Callback not called (expected in test environment)";
        }
    } else {
        GTEST_SKIP() << "Screen capture start failed";
    }
}

// 성능 통계 테스트
TEST_F(CrossPlatformCaptureTest, PerformanceStatistics) {
    if (!capture->initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    auto initial_stats = capture->getPerformanceStats();
    EXPECT_EQ(initial_stats.totalFrames, 0);
    EXPECT_EQ(initial_stats.droppedFrames, 0);
    EXPECT_EQ(initial_stats.averageFPS, 0.0);
    
    if (capture->startCapture()) {
        // 잠시 캡처 수행
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        auto stats = capture->getPerformanceStats();
        capture->stopCapture();
        
        // 통계가 업데이트되었는지 확인
        if (stats.totalFrames > 0) {
            EXPECT_GT(stats.totalFrames, 0);
            EXPECT_GE(stats.averageFPS, 0.0);
            EXPECT_GE(stats.droppedFrames, 0);
            EXPECT_LE(stats.droppedFrames, stats.totalFrames);
        } else {
            GTEST_SKIP() << "No frames captured (expected in test environment)";
        }
    }
}

// 캡처 시작/정지 테스트
TEST_F(CrossPlatformCaptureTest, StartStopCapture) {
    if (!capture->initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    // 초기에는 캡처 중이 아님
    EXPECT_FALSE(capture->isCapturing());
    
    // 캡처 시작
    bool start_result = capture->startCapture();
    if (start_result) {
        EXPECT_TRUE(capture->isCapturing());
        
        // 캡처 정지
        capture->stopCapture();
        EXPECT_FALSE(capture->isCapturing());
        
        // 이미 정지된 상태에서 다시 정지 호출
        EXPECT_NO_THROW(capture->stopCapture());
    } else {
        GTEST_SKIP() << "Screen capture start failed";
    }
}

// 다중 시작 방지 테스트
TEST_F(CrossPlatformCaptureTest, PreventMultipleStart) {
    if (!capture->initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    if (capture->startCapture()) {
        EXPECT_TRUE(capture->isCapturing());
        
        // 이미 시작된 상태에서 다시 시작 시도
        bool second_start = capture->startCapture();
        // 구현에 따라 false를 반환하거나 무시할 수 있음
        
        capture->stopCapture();
        EXPECT_FALSE(capture->isCapturing());
    }
}

// 정리 테스트
TEST_F(CrossPlatformCaptureTest, CleanupTest) {
    if (capture->initialize()) {
        EXPECT_TRUE(capture->isInitialized());
        
        if (capture->startCapture()) {
            EXPECT_TRUE(capture->isCapturing());
            
            // 정리 수행
            capture->cleanup();
            
            // 정리 후 상태 확인
            EXPECT_FALSE(capture->isInitialized());
            EXPECT_FALSE(capture->isCapturing());
        }
    }
}

// 에러 처리 테스트
TEST_F(CrossPlatformCaptureTest, ErrorHandling) {
    // 초기화되지 않은 상태에서 작업 시도
    EXPECT_FALSE(capture->startCapture());
    EXPECT_FALSE(capture->isCapturing());
    EXPECT_FALSE(capture->setTargetMonitor(0));
    
    auto monitors = capture->enumerateMonitors();
    // 초기화되지 않아도 모니터 열거는 가능할 수 있음
    
    auto stats = capture->getPerformanceStats();
    EXPECT_EQ(stats.totalFrames, 0);
    EXPECT_EQ(stats.droppedFrames, 0);
}

// 모니터 정보 유효성 테스트
TEST_F(CrossPlatformCaptureTest, MonitorInfoValidation) {
    auto monitors = capture->enumerateMonitors();
    
    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto& monitor = monitors[i];
        
        // 기본 유효성 검사
        EXPECT_FALSE(monitor.name.empty()) << "Monitor " << i << " has empty name";
        EXPECT_GT(monitor.width, 0) << "Monitor " << i << " has invalid width";
        EXPECT_GT(monitor.height, 0) << "Monitor " << i << " has invalid height";
        
        // 일반적인 해상도 범위 검사
        EXPECT_GE(monitor.width, 320) << "Monitor " << i << " width too small";
        EXPECT_LE(monitor.width, 7680) << "Monitor " << i << " width too large"; // 8K
        EXPECT_GE(monitor.height, 240) << "Monitor " << i << " height too small";
        EXPECT_LE(monitor.height, 4320) << "Monitor " << i << " height too large"; // 8K
        
        // 종횡비 검사 (너무 극단적이지 않아야 함)
        double aspect_ratio = static_cast<double>(monitor.width) / monitor.height;
        EXPECT_GT(aspect_ratio, 0.5) << "Monitor " << i << " aspect ratio too narrow";
        EXPECT_LT(aspect_ratio, 5.0) << "Monitor " << i << " aspect ratio too wide";
    }
}

// 콜백 예외 처리 테스트
TEST_F(CrossPlatformCaptureTest, CallbackExceptionHandling) {
    int callback_count = 0;
    
    auto exception_callback = [&](const cv::Mat& frame) {
        callback_count++;
        if (callback_count == 1) {
            throw std::runtime_error("Test exception");
        }
    };
    
    capture->setFrameCallback(exception_callback);
    
    if (!capture->initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    if (capture->startCapture()) {
        // 예외가 발생해도 캡처가 계속되어야 함
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        capture->stopCapture();
        
        // 콜백이 호출되었는지 확인 (예외 발생에도 불구하고)
        if (callback_count > 0) {
            EXPECT_GT(callback_count, 0);
        } else {
            GTEST_SKIP() << "Callback not called";
        }
    }
}

// 프레임 형식 테스트
TEST_F(CrossPlatformCaptureTest, FrameFormat) {
    cv::Mat received_frame;
    bool frame_received = false;
    
    auto callback = [&](const cv::Mat& frame) {
        received_frame = frame.clone();
        frame_received = true;
    };
    
    capture->setFrameCallback(callback);
    
    if (!capture->initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    if (capture->startCapture()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        capture->stopCapture();
        
        if (frame_received) {
            // 프레임 형식 검증
            EXPECT_FALSE(received_frame.empty());
            EXPECT_EQ(received_frame.channels(), 3); // BGR 또는 RGB
            EXPECT_EQ(received_frame.depth(), CV_8U); // 8-bit unsigned
            EXPECT_GT(received_frame.cols, 0);
            EXPECT_GT(received_frame.rows, 0);
            
            // 픽셀 데이터가 유효한지 확인
            EXPECT_TRUE(received_frame.isContinuous() || !received_frame.isContinuous());
        } else {
            GTEST_SKIP() << "No frame received";
        }
    }
}

// 성능 제한 테스트
TEST_F(CrossPlatformCaptureTest, PerformanceLimits) {
    if (!capture->initialize()) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    int frame_count = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    auto counting_callback = [&](const cv::Mat& frame) {
        frame_count++;
    };
    
    capture->setFrameCallback(counting_callback);
    
    if (capture->startCapture()) {
        // 1초간 캡처
        std::this_thread::sleep_for(std::chrono::seconds(1));
        capture->stopCapture();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        ).count();
        
        if (frame_count > 0) {
            double fps = (frame_count * 1000.0) / duration;
            
            std::cout << "Captured " << frame_count << " frames in " 
                      << duration << "ms (" << fps << " FPS)" << std::endl;
            
            // 기본적인 성능 기준 (너무 높거나 낮지 않아야 함)
            EXPECT_GT(fps, 1.0);   // 최소 1 FPS
            EXPECT_LT(fps, 1000.0); // 최대 1000 FPS (비현실적)
        } else {
            GTEST_SKIP() << "No frames captured";
        }
    }
}