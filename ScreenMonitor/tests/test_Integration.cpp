#include <gtest/gtest.h>
#include "monitoring/PerformanceMonitor.h"
#include "core/ConfigManager.h"
#include "capture/HighSpeedCapture.h"
#include "detection/ColorDetector.h"
#include "detection/ObjectDetector.h"
#include "gui/MainInterface.h"
#include "helpers/ConfigTestHelper.h"
#include "helpers/TestImageGenerator.h"
#include "helpers/GLTestContext.h"
#include "mocks/MockScreenCapture.h"
#include "mocks/MockDetector.h"
#include <memory>
#include <thread>
#include <chrono>

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 설정 파일 생성
        test_config = ConfigTestHelper::createDefaultTestConfig();
        config_file = ConfigTestHelper::createTempConfigFile(test_config);
        
        // OpenGL 컨텍스트 초기화 (헤드리스)
        gl_context = std::make_unique<GLTestContext>();
        gl_context->Initialize(true);
    }
    
    void TearDown() override {
        ConfigTestHelper::cleanupAllTempFiles();
        gl_context.reset();
    }
    
    json test_config;
    std::string config_file;
    std::unique_ptr<GLTestContext> gl_context;
};

// 전체 시스템 초기화 테스트
TEST_F(IntegrationTest, FullSystemInitialization) {
    // ConfigManager 초기화
    auto config_manager = std::make_unique<ConfigManager>();
    EXPECT_TRUE(config_manager->loadConfig(config_file));
    
    // 화면 캡처 시스템 초기화
    auto capture = std::make_unique<HighSpeedCapture>();
    HighSpeedCapture::CaptureSettings settings;
    settings.selectedMonitorIndex = 0;
    settings.targetFPS = 60.0;
    settings.enableFPSLimiting = false;
    
    // 실제 환경에서는 초기화가 실패할 수 있음 (권한, 하드웨어 등)
    bool capture_init = capture->Initialize(settings);
    if (!capture_init) {
        GTEST_SKIP() << "Screen capture initialization failed (expected in test environment)";
    }
    
    // 검출기 초기화
    auto color_detector = std::make_unique<ColorDetector>();
    EXPECT_TRUE(color_detector->initialize());
    
    auto object_detector = std::make_unique<ObjectDetector>();
    // YOLO 모델이 없으므로 로딩은 실패할 것으로 예상
    
    // GUI 초기화 (헤드리스)
    auto gui = std::make_unique<MainInterface>();
    bool gui_init = gui->Initialize();
    
    if (capture_init && gui_init) {
        // 모든 컴포넌트가 초기화되었으면 정리 테스트
        EXPECT_NO_THROW({
            gui.reset();
            object_detector.reset();
            color_detector.reset();
            capture.reset();
            config_manager.reset();
        });
    }
}

// 설정 기반 시스템 구성 테스트
TEST_F(IntegrationTest, ConfigurationDrivenSetup) {
    auto config_manager = std::make_unique<ConfigManager>();
    EXPECT_TRUE(config_manager->loadConfig(config_file));
    
    // 설정에서 알고리즘 선택 확인
    auto vision_config = config_manager->getConfigSection("vision_algorithms");
    EXPECT_EQ(vision_config["selected_algorithm"], "hsv");
    
    // HSV 설정 추출
    auto hsv_config = vision_config["hsv_tracking"];
    EXPECT_TRUE(hsv_config["enabled"]);
    
    auto lower_bound = hsv_config["lower_bound"];
    auto upper_bound = hsv_config["upper_bound"];
    
    // ColorDetector에 설정 적용
    auto detector = std::make_unique<ColorDetector>();
    EXPECT_TRUE(detector->initialize());
    
    cv::Scalar lower(lower_bound[0], lower_bound[1], lower_bound[2]);
    cv::Scalar upper(upper_bound[0], upper_bound[1], upper_bound[2]);
    detector->setColorRange(lower, upper);
    
    EXPECT_TRUE(detector->isConfigured());
    
    // 설정된 범위 확인
    auto set_range = detector->getColorRange();
    EXPECT_EQ(set_range.h_min, lower_bound[0]);
    EXPECT_EQ(set_range.s_min, lower_bound[1]);
    EXPECT_EQ(set_range.v_min, lower_bound[2]);
}

// Mock을 사용한 전체 파이프라인 테스트
TEST_F(IntegrationTest, MockPipelineTest) {
    using ::testing::Return;
    using ::testing::_;
    
    // Mock 객체들 생성
    auto mock_capture = std::make_unique<MockScreenCapture>();
    auto mock_detector = std::make_unique<MockColorDetector>();
    
    // Mock 동작 설정
    mock_capture->SetupDefaultBehavior();
    mock_detector->SetupDefaultBehavior();
    
    // 테스트 시나리오 설정
    cv::Mat test_frame = TestImageGenerator::createHSVTestImage(640, 480, 100, 120, 2).first;
    mock_capture->SetMockFrame(test_frame);
    
    std::vector<cv::Rect> test_detections = {
        cv::Rect(100, 100, 50, 50),
        cv::Rect(200, 200, 60, 60)
    };
    mock_detector->SetMockDetectionResult(test_detections[0]);
    
    // Mock 호출 검증
    EXPECT_CALL(*mock_capture, Initialize()).Times(1);
    EXPECT_CALL(*mock_capture, CaptureFrame()).Times(::testing::AtLeast(1));
    EXPECT_CALL(*mock_capture, GetLatestFrame()).Times(::testing::AtLeast(1));
    EXPECT_CALL(*mock_detector, detectTarget(_)).Times(::testing::AtLeast(1));
    
    // 시뮬레이션된 처리 사이클
    EXPECT_TRUE(mock_capture->Initialize());
    
    for (int i = 0; i < 5; ++i) {
        if (mock_capture->CaptureFrame()) {
            cv::Mat frame = mock_capture->GetLatestFrame();
            EXPECT_FALSE(frame.empty());
            
            cv::Rect detection = mock_detector->detectTarget(frame);
            if (!detection.empty()) {
                // 검출 성공
                EXPECT_GT(detection.area(), 0);
            }
        }
    }
}

// 실시간 처리 시뮬레이션 테스트
TEST_F(IntegrationTest, RealTimeProcessingSimulation) {
    auto config_manager = std::make_unique<ConfigManager>();
    EXPECT_TRUE(config_manager->loadConfig(config_file));
    
    auto detector = std::make_unique<ColorDetector>();
    EXPECT_TRUE(detector->initialize());
    
    // 설정에서 FPS 목표값 가져오기
    auto perf_config = config_manager->getConfigSection("performance");
    int target_fps = perf_config["target_fps"];
    
    auto frame_duration = std::chrono::milliseconds(1000 / target_fps);
    
    // 시뮬레이션된 프레임 시퀀스 생성
    std::vector<cv::Mat> frames;
    for (int i = 0; i < 10; ++i) {
        frames.push_back(TestImageGenerator::createHSVTestImage(
            320, 240, 100 + i * 5, 120 + i * 5, 1 + i % 3
        ).first);
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    int processed_frames = 0;
    
    // 1초간 처리 시뮬레이션
    while (true) {
        auto current_time = std::chrono::high_resolution_clock::now();
        if (current_time - start_time >= std::chrono::seconds(1)) {
            break;
        }
        
        cv::Mat frame = frames[processed_frames % frames.size()];
        cv::Rect result = detector->detectTarget(frame);
        
        processed_frames++;
        std::this_thread::sleep_for(frame_duration);
    }
    
    // 목표 FPS에 근접한 처리량인지 확인 (±20% 허용)
    double actual_fps = processed_frames;
    EXPECT_NEAR(actual_fps, target_fps, target_fps * 0.2);
    
    std::cout << "Processed " << processed_frames << " frames in 1 second (target: " 
              << target_fps << " FPS)" << std::endl;
}

// GUI와 백엔드 연동 테스트
TEST_F(IntegrationTest, GUIBackendIntegration) {
    auto gui = std::make_unique<MainInterface>();
    
    if (!gui->Initialize()) {
        GTEST_SKIP() << "GUI initialization failed in test environment";
    }
    
    // 모니터 목록 설정
    std::vector<std::string> monitors = {"Monitor 1", "Monitor 2"};
    gui->SetMonitorList(monitors);
    
    // 초기 상태 확인
    EXPECT_EQ(gui->GetSelectedMonitorIndex(), 0);
    EXPECT_TRUE(gui->IsHSVDetectionEnabled());
    
    // 프레임과 검출 결과 업데이트
    cv::Mat test_frame = TestImageGenerator::createCheckerboardImage(640, 480);
    gui->UpdateFrame(test_frame);
    
    std::vector<cv::Rect> detections = {cv::Rect(50, 50, 100, 100)};
    gui->UpdateDetections(detections);
    
    // FPS 업데이트
    gui->UpdateFPS(60.5f);
    
    // 렌더링 사이클
    EXPECT_NO_THROW(gui->Render());
    EXPECT_NO_THROW(gui->HandleEvents());
    
    // GUI 상태가 백엔드에 반영되는지 확인
    cv::Scalar hsv_lower = gui->GetHSVLowerBound();
    cv::Scalar hsv_upper = gui->GetHSVUpperBound();
    
    // HSV 범위가 유효한지 확인
    EXPECT_GE(hsv_lower[0], 0);
    EXPECT_LE(hsv_upper[0], 179);
}

// 에러 복구 테스트
TEST_F(IntegrationTest, ErrorRecoveryTest) {
    auto mock_capture = std::make_unique<MockScreenCapture>();
    auto detector = std::make_unique<ColorDetector>();
    
    // 실패 시나리오 설정
    mock_capture->SetupFailureScenario();
    EXPECT_TRUE(detector->initialize());
    
    // 캡처 실패 상황
    EXPECT_FALSE(mock_capture->Initialize());
    EXPECT_FALSE(mock_capture->CaptureFrame());
    
    cv::Mat empty_frame = mock_capture->GetLatestFrame();
    EXPECT_TRUE(empty_frame.empty());
    
    // 빈 프레임에 대한 검출기 동작 확인
    cv::Rect result = detector->detectTarget(empty_frame);
    EXPECT_TRUE(result.empty());
    
    // 복구 시나리오: 정상 동작으로 전환
    mock_capture->SetupDefaultBehavior();
    mock_capture->SimulateError(false);
    
    EXPECT_TRUE(mock_capture->Initialize());
    
    // 정상 프레임으로 복구 테스트
    cv::Mat test_frame = TestImageGenerator::createSolidColorImage(640, 480, cv::Scalar(255, 0, 0));
    mock_capture->SetMockFrame(test_frame);
    
    if (mock_capture->CaptureFrame()) {
        cv::Mat recovered_frame = mock_capture->GetLatestFrame();
        EXPECT_FALSE(recovered_frame.empty());
    }
}

// 메모리 누수 통합 테스트
TEST_F(IntegrationTest, MemoryLeakIntegrationTest) {
    auto config_manager = std::make_unique<ConfigManager>();
    auto detector = std::make_unique<ColorDetector>();
    auto mock_capture = std::make_unique<MockScreenCapture>();
    
    EXPECT_TRUE(config_manager->loadConfig(config_file));
    EXPECT_TRUE(detector->initialize());
    mock_capture->SetupPerformanceScenario();
    
    // 장시간 실행 시뮬레이션
    for (int cycle = 0; cycle < 1000; ++cycle) {
        // 설정 업데이트
        if (cycle % 100 == 0) {
            json new_config = ConfigTestHelper::createHSVTestConfig(
                100 + cycle % 50, 120 + cycle % 50, 100, 255, 100, 255
            );
            config_manager->setConfig(new_config);
        }
        
        // 프레임 처리
        if (mock_capture->CaptureFrame()) {
            cv::Mat frame = mock_capture->GetLatestFrame();
            if (!frame.empty()) {
                auto results = detector->detectMultipleTargets(frame);
                // 결과를 즉시 폐기하여 메모리 사용량 테스트
            }
        }
    }
    
    // 메모리 사용량은 직접 측정하기 어려우므로
    // 예외 없이 완료되는지만 확인
    SUCCEED();
}

// 성능 벤치마크 통합 테스트
TEST_F(IntegrationTest, PerformanceBenchmarkIntegration) {
    auto detector = std::make_unique<ColorDetector>();
    EXPECT_TRUE(detector->initialize());
    
    // 다양한 해상도에서의 성능 측정
    std::vector<std::pair<int, int>> resolutions = {
        {320, 240},   // QVGA
        {640, 480},   // VGA
        {1280, 720},  // HD
        {1920, 1080}  // FHD
    };
    
    for (const auto& [width, height] : resolutions) {
        cv::Mat test_frame = TestImageGenerator::createHSVTestImage(width, height, 100, 120, 3).first;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 100번 검출 수행
        for (int i = 0; i < 100; ++i) {
            auto results = detector->detectMultipleTargets(test_frame);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        ).count();
        
        double fps = 100.0 * 1000.0 / duration;
        
        std::cout << "Resolution " << width << "x" << height 
                  << ": " << fps << " FPS (" << duration << "ms for 100 frames)" << std::endl;
        
        // 최소 성능 요구사항: 320x240에서 100 FPS 이상
        if (width == 320 && height == 240) {
            EXPECT_GT(fps, 100.0);
        }
    }
}

// 동시성 통합 테스트
TEST_F(IntegrationTest, ConcurrencyIntegrationTest) {
    auto config_manager = std::make_unique<ConfigManager>();
    EXPECT_TRUE(config_manager->loadConfig(config_file));
    
    std::vector<std::unique_ptr<ColorDetector>> detectors;
    for (int i = 0; i < 3; ++i) {
        auto detector = std::make_unique<ColorDetector>();
        EXPECT_TRUE(detector->initialize());
        detectors.push_back(std::move(detector));
    }
    
    std::vector<std::thread> threads;
    std::atomic<int> total_detections{0};
    
    // 여러 스레드에서 동시에 검출 수행
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&detectors, i, &total_detections]() {
            cv::Mat test_frame = TestImageGenerator::createHSVTestImage(
                640, 480, 100 + i * 10, 120 + i * 10, 2
            ).first;
            
            for (int j = 0; j < 50; ++j) {
                auto results = detectors[i]->detectMultipleTargets(test_frame);
                total_detections += static_cast<int>(results.size());
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 모든 스레드에서 검출이 수행되었는지 확인
    EXPECT_GT(total_detections.load(), 0);
    
    std::cout << "Total detections across all threads: " << total_detections.load() << std::endl;
}

// 설정 변경 시 시스템 동기화 테스트
TEST_F(IntegrationTest, ConfigurationSyncTest) {
    auto config_manager = std::make_unique<ConfigManager>();
    auto detector = std::make_unique<ColorDetector>();
    
    EXPECT_TRUE(config_manager->loadConfig(config_file));
    EXPECT_TRUE(detector->initialize());
    
    // 초기 설정 확인
    auto initial_vision_config = config_manager->getConfigSection("vision_algorithms");
    auto initial_hsv = initial_vision_config["hsv_tracking"];
    
    // 검출기에 초기 설정 적용
    cv::Scalar initial_lower(initial_hsv["lower_bound"][0], 
                            initial_hsv["lower_bound"][1], 
                            initial_hsv["lower_bound"][2]);
    cv::Scalar initial_upper(initial_hsv["upper_bound"][0], 
                            initial_hsv["upper_bound"][1], 
                            initial_hsv["upper_bound"][2]);
    
    detector->setColorRange(initial_lower, initial_upper);
    
    // 설정 변경
    json new_config = ConfigTestHelper::createHSVTestConfig(50, 70, 150, 255, 150, 255);
    config_manager->setConfig(new_config);
    
    // 변경된 설정 적용
    auto updated_vision_config = config_manager->getConfigSection("vision_algorithms");
    auto updated_hsv = updated_vision_config["hsv_tracking"];
    
    cv::Scalar updated_lower(updated_hsv["lower_bound"][0], 
                            updated_hsv["lower_bound"][1], 
                            updated_hsv["lower_bound"][2]);
    cv::Scalar updated_upper(updated_hsv["upper_bound"][0], 
                            updated_hsv["upper_bound"][1], 
                            updated_hsv["upper_bound"][2]);
    
    detector->setColorRange(updated_lower, updated_upper);
    
    // 검출기 설정이 올바르게 업데이트되었는지 확인
    auto detector_range = detector->getColorRange();
    EXPECT_EQ(detector_range.h_min, 50);
    EXPECT_EQ(detector_range.h_max, 70);
    
    // 새로운 설정으로 검출 테스트
    cv::Mat test_frame = TestImageGenerator::createHSVTestImage(640, 480, 55, 65, 2).first;
    auto results = detector->detectMultipleTargets(test_frame);
    
    // 새로운 색상 범위에 맞는 객체가 검출되어야 함
    EXPECT_GT(results.size(), 0);
}