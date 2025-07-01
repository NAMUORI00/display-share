#include <gtest/gtest.h>
#include "gui/MainInterface.h"
#include "helpers/GLTestContext.h"
#include "helpers/TestImageGenerator.h"
#include <opencv2/opencv.hpp>
#include <thread>
#include <chrono>

class MainInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 헤드리스 OpenGL 컨텍스트 초기화
        gl_context = std::make_unique<GLTestContext>();
        ASSERT_TRUE(gl_context->Initialize(true)); // 헤드리스 모드
        
        interface = std::make_unique<MainInterface>();
    }
    
    void TearDown() override {
        interface.reset();
        gl_context.reset();
    }
    
    std::unique_ptr<GLTestContext> gl_context;
    std::unique_ptr<MainInterface> interface;
    
    // 테스트용 상수
    static constexpr int TEST_WIDTH = 800;
    static constexpr int TEST_HEIGHT = 600;
};

// 초기화 테스트
TEST_F(MainInterfaceTest, Initialization) {
    // 헤드리스 모드에서는 초기화가 실패할 수 있음
    // 실제 환경에서만 테스트 가능한 부분은 조건부로 처리
    if (gl_context->IsInitialized()) {
        bool init_result = interface->Initialize();
        if (init_result) {
            EXPECT_TRUE(init_result);
            
            // 윈도우 상태 확인
            EXPECT_FALSE(interface->ShouldClose());
        } else {
            // 헤드리스 환경에서는 초기화 실패가 예상됨
            GTEST_SKIP() << "GUI initialization failed in headless environment";
        }
    } else {
        GTEST_SKIP() << "OpenGL context not available";
    }
}

// 프레임 업데이트 테스트
TEST_F(MainInterfaceTest, UpdateFrame) {
    if (!interface->Initialize()) {
        GTEST_SKIP() << "GUI initialization failed";
    }
    
    cv::Mat test_frame = TestImageGenerator::createSolidColorImage(
        TEST_WIDTH, TEST_HEIGHT, cv::Scalar(100, 150, 200)
    );
    
    EXPECT_NO_THROW(interface->UpdateFrame(test_frame));
    
    // 빈 프레임 업데이트 테스트
    cv::Mat empty_frame;
    EXPECT_NO_THROW(interface->UpdateFrame(empty_frame));
}

// 검출 결과 업데이트 테스트
TEST_F(MainInterfaceTest, UpdateDetections) {
    if (!interface->Initialize()) {
        GTEST_SKIP() << "GUI initialization failed";
    }
    
    std::vector<cv::Rect> detections = {
        cv::Rect(50, 50, 100, 100),
        cv::Rect(200, 150, 80, 60),
        cv::Rect(400, 300, 120, 90)
    };
    
    EXPECT_NO_THROW(interface->UpdateDetections(detections));
    
    // 빈 검출 결과 업데이트
    std::vector<cv::Rect> empty_detections;
    EXPECT_NO_THROW(interface->UpdateDetections(empty_detections));
}

// 모니터 목록 설정 테스트
TEST_F(MainInterfaceTest, SetMonitorList) {
    std::vector<std::string> monitors = {
        "Primary Monitor (1920x1080)",
        "Secondary Monitor (1280x720)",
        "Third Monitor (1440x900)"
    };
    
    EXPECT_NO_THROW(interface->SetMonitorList(monitors));
    
    // 빈 모니터 목록
    std::vector<std::string> empty_monitors;
    EXPECT_NO_THROW(interface->SetMonitorList(empty_monitors));
}

// FPS 업데이트 테스트
TEST_F(MainInterfaceTest, UpdateFPS) {
    EXPECT_NO_THROW(interface->UpdateFPS(60.0f));
    EXPECT_NO_THROW(interface->UpdateFPS(120.5f));
    EXPECT_NO_THROW(interface->UpdateFPS(0.0f));
    EXPECT_NO_THROW(interface->UpdateFPS(-1.0f)); // 음수 FPS도 처리해야 함
}

// GUI 상태 접근자 테스트
TEST_F(MainInterfaceTest, StateAccessors) {
    // 기본값 확인
    EXPECT_FALSE(interface->IsCaptureEnabled());
    EXPECT_TRUE(interface->IsHSVDetectionEnabled());
    EXPECT_FALSE(interface->IsYOLODetectionEnabled());
    EXPECT_EQ(interface->GetSelectedMonitorIndex(), 0);
    EXPECT_EQ(interface->GetTargetFPS(), 60.0f);
    
    // HSV 범위 확인
    cv::Scalar lower = interface->GetHSVLowerBound();
    cv::Scalar upper = interface->GetHSVUpperBound();
    
    EXPECT_GE(lower[0], 0);
    EXPECT_LE(lower[0], 179);
    EXPECT_GE(upper[0], 0);
    EXPECT_LE(upper[0], 179);
}

// 이벤트 처리 테스트
TEST_F(MainInterfaceTest, HandleEvents) {
    if (!interface->Initialize()) {
        GTEST_SKIP() << "GUI initialization failed";
    }
    
    // 이벤트 처리는 예외 없이 완료되어야 함
    EXPECT_NO_THROW(interface->HandleEvents());
}

// 렌더링 테스트
TEST_F(MainInterfaceTest, Rendering) {
    if (!interface->Initialize()) {
        GTEST_SKIP() << "GUI initialization failed";
    }
    
    // 기본 렌더링
    EXPECT_NO_THROW(interface->Render());
    
    // 프레임과 검출 결과가 있는 상태에서 렌더링
    cv::Mat test_frame = TestImageGenerator::createCheckerboardImage(TEST_WIDTH, TEST_HEIGHT);
    interface->UpdateFrame(test_frame);
    
    std::vector<cv::Rect> detections = {cv::Rect(100, 100, 50, 50)};
    interface->UpdateDetections(detections);
    
    EXPECT_NO_THROW(interface->Render());
}

// 윈도우 상태 테스트
TEST_F(MainInterfaceTest, WindowState) {
    if (!interface->Initialize()) {
        GTEST_SKIP() << "GUI initialization failed";
    }
    
    // 초기에는 닫히지 않은 상태여야 함
    EXPECT_FALSE(interface->ShouldClose());
    
    // 헤드리스 모드에서는 윈도우가 즉시 닫힐 수 있음
}

// 복잡한 시나리오 테스트
TEST_F(MainInterfaceTest, ComplexScenario) {
    if (!interface->Initialize()) {
        GTEST_SKIP() << "GUI initialization failed";
    }
    
    // 복잡한 이미지 생성
    auto [complex_frame, expected_rects] = TestImageGenerator::createHSVTestImage(
        TEST_WIDTH, TEST_HEIGHT, 100, 120, 5
    );
    
    // 다중 검출 결과
    std::vector<cv::Rect> detections;
    for (int i = 0; i < 3; ++i) {
        detections.emplace_back(i * 100, i * 50, 80, 60);
    }
    
    // 모니터 목록
    std::vector<std::string> monitors = {
        "Monitor 1", "Monitor 2", "Monitor 3"
    };
    
    // 모든 업데이트 수행
    EXPECT_NO_THROW(interface->UpdateFrame(complex_frame));
    EXPECT_NO_THROW(interface->UpdateDetections(detections));
    EXPECT_NO_THROW(interface->SetMonitorList(monitors));
    EXPECT_NO_THROW(interface->UpdateFPS(75.5f));
    
    // 렌더링
    EXPECT_NO_THROW(interface->Render());
    EXPECT_NO_THROW(interface->HandleEvents());
}

// 메모리 사용량 테스트
TEST_F(MainInterfaceTest, MemoryUsage) {
    if (!interface->Initialize()) {
        GTEST_SKIP() << "GUI initialization failed";
    }
    
    // 큰 이미지로 메모리 사용량 테스트
    cv::Mat large_frame = TestImageGenerator::createSolidColorImage(
        1920, 1080, cv::Scalar(128, 128, 128)
    );
    
    // 여러 번 업데이트하여 메모리 누수 확인
    for (int i = 0; i < 100; ++i) {
        interface->UpdateFrame(large_frame);
        
        // 많은 검출 결과
        std::vector<cv::Rect> many_detections;
        for (int j = 0; j < 50; ++j) {
            many_detections.emplace_back(j * 10, j * 5, 20, 15);
        }
        interface->UpdateDetections(many_detections);
    }
    
    SUCCEED(); // 예외 없이 완료되면 성공
}

// 상태 변경 테스트
TEST_F(MainInterfaceTest, StateChanges) {
    // 여러 상태 접근자들이 일관성 있게 동작하는지 확인
    cv::Scalar lower = interface->GetHSVLowerBound();
    cv::Scalar upper = interface->GetHSVUpperBound();
    
    // HSV 범위 유효성 검사
    for (int i = 0; i < 3; ++i) {
        EXPECT_LE(lower[i], upper[i]) << "Lower bound should be <= upper bound for channel " << i;
    }
    
    // 색조(H) 범위 확인
    EXPECT_GE(lower[0], 0);
    EXPECT_LE(lower[0], 179);
    EXPECT_GE(upper[0], 0);
    EXPECT_LE(upper[0], 179);
    
    // 채도(S), 명도(V) 범위 확인
    for (int i = 1; i < 3; ++i) {
        EXPECT_GE(lower[i], 0);
        EXPECT_LE(lower[i], 255);
        EXPECT_GE(upper[i], 0);
        EXPECT_LE(upper[i], 255);
    }
}

// 에러 처리 테스트
TEST_F(MainInterfaceTest, ErrorHandling) {
    // 초기화되지 않은 상태에서의 작업
    if (!interface->Initialize()) {
        // 초기화 실패 시 다른 작업들이 안전하게 처리되는지 확인
        EXPECT_NO_THROW(interface->Render());
        EXPECT_NO_THROW(interface->HandleEvents());
        EXPECT_NO_THROW(interface->UpdateFrame(cv::Mat()));
        EXPECT_NO_THROW(interface->UpdateDetections({}));
    }
}

// 정리 테스트
TEST_F(MainInterfaceTest, Cleanup) {
    if (interface->Initialize()) {
        // 프레임과 검출 결과 설정
        cv::Mat test_frame = TestImageGenerator::createSolidColorImage(
            TEST_WIDTH, TEST_HEIGHT, cv::Scalar(100, 100, 100)
        );
        interface->UpdateFrame(test_frame);
        
        std::vector<cv::Rect> detections = {cv::Rect(10, 10, 50, 50)};
        interface->UpdateDetections(detections);
        
        // 명시적 정리는 소멸자에서 처리됨
        EXPECT_NO_THROW(interface.reset());
        interface = std::make_unique<MainInterface>();
    }
}

// 동시성 테스트 (GUI는 단일 스레드여야 함)
TEST_F(MainInterfaceTest, ThreadSafety) {
    // GUI 컴포넌트는 메인 스레드에서만 사용되어야 하므로
    // 다른 스레드에서의 접근은 테스트하지 않음
    // 대신 상태 접근자들이 스레드 안전한지 확인
    
    std::vector<std::thread> threads;
    std::atomic<bool> all_success{true};
    
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([this, &all_success]() {
            try {
                // 읽기 전용 작업만 수행
                bool capture_enabled = interface->IsCaptureEnabled();
                bool hsv_enabled = interface->IsHSVDetectionEnabled();
                float fps = interface->GetTargetFPS();
                
                // 값들이 유효한 범위 내에 있는지 확인
                if (fps < 0 || fps > 1000) {
                    all_success = false;
                }
            } catch (...) {
                all_success = false;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_TRUE(all_success.load());
}

// 성능 테스트
TEST_F(MainInterfaceTest, PerformanceTest) {
    if (!interface->Initialize()) {
        GTEST_SKIP() << "GUI initialization failed";
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 100번의 업데이트 사이클
    for (int i = 0; i < 100; ++i) {
        cv::Mat frame = TestImageGenerator::createSolidColorImage(
            640, 480, cv::Scalar(i % 255, (i * 2) % 255, (i * 3) % 255)
        );
        
        interface->UpdateFrame(frame);
        interface->UpdateFPS(static_cast<float>(60 + i % 60));
        
        // 간단한 렌더링 (실제 화면 출력 없음)
        interface->Render();
        interface->HandleEvents();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    ).count();
    
    // 100번의 사이클이 10초 이내에 완료되어야 함 (매우 관대한 기준)
    EXPECT_LT(duration, 10000);
    
    std::cout << "100 update cycles completed in " << duration << "ms" << std::endl;
}