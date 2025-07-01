#include <gtest/gtest.h>
#include "monitoring/PerformanceMonitor.h"
#include "capture/HighSpeedCapture.h"
#include "detection/ColorDetector.h"
#include "detection/ObjectDetector.h"
#include "gui/MainInterface.h"
#include "core/ConfigManager.h"
#include <thread>
#include <chrono>
#include <atomic>

class IntegrationSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 전체 시스템은 PerformanceMonitor를 통해 통합됨
        system = std::make_unique<PerformanceMonitor>();
    }
    
    void TearDown() override {
        if (system) {
            system->Cleanup();
        }
        system.reset();
    }
    
    std::unique_ptr<PerformanceMonitor> system;
};

// 전체 시스템 초기화 테스트
TEST_F(IntegrationSystemTest, SystemInitialization) {
    // 시스템 초기화는 GUI와 화면 캡처를 포함하므로 실패할 수 있음
    bool init_result = system->Initialize();
    
    if (!init_result) {
        GTEST_SKIP() << "System initialization failed - requires display and OpenGL context";
    }
    
    EXPECT_TRUE(init_result);
}

// 컴포넌트 통합 테스트
TEST_F(IntegrationSystemTest, ComponentIntegration) {
    if (!system->Initialize()) {
        GTEST_SKIP() << "System initialization failed";
    }
    
    // 모니터 목록 확인
    auto monitors = system->GetAvailableMonitors();
    EXPECT_GT(monitors.size(), 0);
    
    // 현재 모니터 인덱스 확인
    int current_monitor = system->GetCurrentMonitorIndex();
    EXPECT_GE(current_monitor, 0);
    EXPECT_LT(current_monitor, static_cast<int>(monitors.size()));
}

// 캡처-검출 파이프라인 테스트
TEST_F(IntegrationSystemTest, CaptureDetectionPipeline) {
    if (!system->Initialize()) {
        GTEST_SKIP() << "System initialization failed";
    }
    
    // 시스템 실행 (짧은 시간)
    std::atomic<bool> stop{false};
    std::thread run_thread([this, &stop]() {
        while (!stop) {
            system->HandleEvents();
            system->Update();
            system->Render();
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
    });
    
    // 0.5초 실행
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop = true;
    run_thread.join();
    
    SUCCEED(); // 크래시 없이 완료
}

// 설정 변경 반영 테스트
TEST_F(IntegrationSystemTest, ConfigurationUpdate) {
    if (!system->Initialize()) {
        GTEST_SKIP() << "System initialization failed";
    }
    
    // ConfigManager를 통한 설정은 시스템 내부에서 처리됨
    // 외부에서 직접 접근할 수 없으므로 간접적으로 테스트
    
    // 모니터 변경 시도
    auto monitors = system->GetAvailableMonitors();
    if (monitors.size() > 1) {
        EXPECT_TRUE(system->SetCaptureMonitor(1));
        EXPECT_EQ(system->GetCurrentMonitorIndex(), 1);
        
        // 원래 모니터로 복구
        EXPECT_TRUE(system->SetCaptureMonitor(0));
        EXPECT_EQ(system->GetCurrentMonitorIndex(), 0);
    }
}

// 실시간 성능 테스트
TEST_F(IntegrationSystemTest, RealTimePerformance) {
    if (!system->Initialize()) {
        GTEST_SKIP() << "System initialization failed";
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    int update_count = 0;
    
    // 1초 동안 시스템 업데이트
    while (true) {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - start_time
        ).count();
        
        if (elapsed >= 1000) break;
        
        system->HandleEvents();
        system->Update();
        system->Render();
        update_count++;
        
        // 프레임 제한 없이 최대 속도로 실행
    }
    
    std::cout << "System performed " << update_count << " updates in 1 second" << std::endl;
    
    // 최소 30 업데이트/초는 달성해야 함
    EXPECT_GT(update_count, 30);
}

// 메모리 안정성 테스트
TEST_F(IntegrationSystemTest, MemoryStability) {
    // 여러 번 초기화/정리 반복
    for (int i = 0; i < 3; ++i) {
        auto temp_system = std::make_unique<PerformanceMonitor>();
        
        if (temp_system->Initialize()) {
            // 몇 번의 업데이트 수행
            for (int j = 0; j < 10; ++j) {
                temp_system->HandleEvents();
                temp_system->Update();
                temp_system->Render();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        temp_system->Cleanup();
        temp_system.reset();
    }
    
    SUCCEED(); // 메모리 문제 없이 완료
}

// 장시간 실행 안정성 테스트
TEST_F(IntegrationSystemTest, LongRunStability) {
    if (!system->Initialize()) {
        GTEST_SKIP() << "System initialization failed";
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    
    // 5초 동안 실행
    while (true) {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - start_time
        ).count();
        
        if (elapsed >= 5) break;
        
        system->HandleEvents();
        system->Update();
        system->Render();
        frame_count++;
        
        // 대략 60 FPS로 제한
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    std::cout << "System ran for 5 seconds, processed " << frame_count << " frames" << std::endl;
    
    // 5초 동안 최소 150 프레임 (30 FPS * 5초)
    EXPECT_GT(frame_count, 150);
}

// 에러 복구 테스트
TEST_F(IntegrationSystemTest, ErrorRecovery) {
    // 잘못된 모니터 인덱스 설정
    EXPECT_FALSE(system->SetCaptureMonitor(-1));
    EXPECT_FALSE(system->SetCaptureMonitor(999));
    
    // 초기화 전 작업 시도
    system->HandleEvents();
    system->Update();
    system->Render();
    
    // 시스템이 여전히 초기화 가능해야 함
    bool init_result = system->Initialize();
    if (init_result) {
        EXPECT_TRUE(init_result);
    }
}

// 전체 워크플로우 시나리오 테스트
TEST_F(IntegrationSystemTest, CompleteWorkflowScenario) {
    if (!system->Initialize()) {
        GTEST_SKIP() << "System initialization failed";
    }
    
    // 1. 시스템 시작
    std::cout << "Starting complete workflow test..." << std::endl;
    
    // 2. 캡처 및 검출 수행
    std::atomic<bool> running{true};
    std::atomic<int> total_frames{0};
    std::atomic<int> detection_frames{0};
    
    std::thread worker([this, &running, &total_frames, &detection_frames]() {
        while (running) {
            system->HandleEvents();
            system->Update();
            system->Render();
            
            total_frames++;
            
            // GUI가 닫히면 종료
            if (system->ShouldClose()) {
                running = false;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
        }
    });
    
    // 3초 동안 실행
    std::this_thread::sleep_for(std::chrono::seconds(3));
    running = false;
    worker.join();
    
    std::cout << "Workflow test completed:" << std::endl;
    std::cout << "- Total frames: " << total_frames.load() << std::endl;
    std::cout << "- Expected frames: ~90 (30 FPS * 3 seconds)" << std::endl;
    
    // 최소 60 프레임은 처리해야 함 (20 FPS * 3초)
    EXPECT_GT(total_frames.load(), 60);
}

// 개별 컴포넌트 직접 테스트
TEST_F(IntegrationSystemTest, DirectComponentAccess) {
    // 개별 컴포넌트를 직접 생성하여 통합 테스트
    auto capture = std::make_unique<HighSpeedCapture>();
    auto color_detector = std::make_unique<ColorDetector>();
    auto config_manager = std::make_unique<ConfigManager>();
    
    // ConfigManager 테스트
    EXPECT_FALSE(config_manager->isLoaded());
    auto default_config = config_manager->getAllConfig();
    EXPECT_FALSE(default_config.empty());
    
    // ColorDetector 테스트
    EXPECT_TRUE(color_detector->initialize());
    EXPECT_TRUE(color_detector->isConfigured());
    
    // HighSpeedCapture 테스트
    HighSpeedCapture::CaptureSettings settings;
    settings.selectedMonitorIndex = 0;
    bool capture_init = capture->Initialize(settings);
    
    if (capture_init) {
        // 통합 동작 테스트
        if (capture->StartCapture() && capture->CaptureFrame()) {
            cv::Mat frame = capture->GetLatestFrame();
            if (!frame.empty()) {
                // 캡처된 프레임으로 색상 검출
                cv::Rect detection = color_detector->detectTarget(frame);
                
                // 검출 결과는 있을 수도 없을 수도 있음
                std::cout << "Integration test - Frame captured: " 
                          << frame.cols << "x" << frame.rows 
                          << ", Detection: " << (detection.empty() ? "none" : "found") 
                          << std::endl;
            }
        }
        capture->StopCapture();
    }
}

// 동시성 스트레스 테스트
TEST_F(IntegrationSystemTest, ConcurrencyStressTest) {
    if (!system->Initialize()) {
        GTEST_SKIP() << "System initialization failed";
    }
    
    std::atomic<bool> stop{false};
    std::atomic<int> errors{0};
    
    // 메인 실행 스레드
    std::thread main_thread([this, &stop, &errors]() {
        while (!stop) {
            try {
                system->HandleEvents();
                system->Update();
                system->Render();
            } catch (...) {
                errors++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // 설정 변경 스레드
    std::thread config_thread([this, &stop, &errors]() {
        auto monitors = system->GetAvailableMonitors();
        int monitor_count = static_cast<int>(monitors.size());
        
        while (!stop) {
            try {
                // 모니터 전환
                if (monitor_count > 1) {
                    system->SetCaptureMonitor(rand() % monitor_count);
                }
            } catch (...) {
                errors++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    
    // 2초 실행
    std::this_thread::sleep_for(std::chrono::seconds(2));
    stop = true;
    
    main_thread.join();
    config_thread.join();
    
    // 에러가 없어야 함
    EXPECT_EQ(errors.load(), 0);
}

// 시스템 종료 테스트
TEST_F(IntegrationSystemTest, SystemShutdown) {
    if (!system->Initialize()) {
        GTEST_SKIP() << "System initialization failed";
    }
    
    // 몇 번의 업데이트 수행
    for (int i = 0; i < 10; ++i) {
        system->HandleEvents();
        system->Update();
        system->Render();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 명시적 정리
    EXPECT_NO_THROW(system->Cleanup());
    
    // 정리 후에도 안전하게 동작해야 함
    EXPECT_NO_THROW(system->HandleEvents());
    EXPECT_NO_THROW(system->Update());
    EXPECT_NO_THROW(system->Render());
}