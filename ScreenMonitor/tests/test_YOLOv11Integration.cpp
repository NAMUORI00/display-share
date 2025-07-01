#include <gtest/gtest.h>
#include "monitoring/PerformanceMonitor.h"
#include "detection/ObjectDetector.h"
#include "gui/MainInterface.h"
#include "helpers/TestImageGenerator.h"
#include <filesystem>
#include <thread>
#include <chrono>

class YOLOv11IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 모델 파일 확인
        model_exists = std::filesystem::exists("models/yolo11n.onnx");
        class_file_exists = std::filesystem::exists("models/coco_classes.txt");
        
        if (!model_exists) {
            std::cout << "YOLO v11 model not found. Integration tests will be skipped." << std::endl;
            std::cout << "Please follow models/README.md to download YOLO v11 model." << std::endl;
        }
    }
    
    bool model_exists;
    bool class_file_exists;
};

// YOLO v11과 전체 시스템 통합 테스트
TEST_F(YOLOv11IntegrationTest, FullSystemIntegration) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not available";
    }
    
    auto system = std::make_unique<PerformanceMonitor>();
    
    // 시스템 초기화
    bool init_result = system->Initialize();
    if (!init_result) {
        GTEST_SKIP() << "System initialization failed - requires display and OpenGL context";
    }
    
    // YOLO v11을 사용한 짧은 테스트 실행
    std::atomic<bool> running{true};
    std::atomic<int> processed_frames{0};
    
    std::thread worker([&system, &running, &processed_frames]() {
        while (running) {
            system->HandleEvents();
            system->Update();
            system->Render();
            processed_frames++;
            
            if (system->ShouldClose()) {
                running = false;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 FPS
        }
    });
    
    // 2초 실행
    std::this_thread::sleep_for(std::chrono::seconds(2));
    running = false;
    worker.join();
    
    system->Cleanup();
    
    std::cout << "Full system integration test completed. Processed " 
              << processed_frames.load() << " frames." << std::endl;
    
    // 최소 20 프레임은 처리되어야 함
    EXPECT_GT(processed_frames.load(), 20);
}

// YOLO v11 독립 성능 테스트
TEST_F(YOLOv11IntegrationTest, StandaloneYOLOv11Performance) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not available";
    }
    
    auto detector = std::make_unique<ObjectDetector>();
    
    // OpenCV DNN 백엔드로 모델 로드
    bool load_success = detector->loadYOLOv11Model("models/yolo11n.onnx", YOLOBackend::OPENCV_DNN);
    if (!load_success) {
        GTEST_SKIP() << "YOLO v11 model loading failed";
    }
    
    // 클래스 이름 로드
    if (class_file_exists) {
        detector->loadClassNames("models/coco_classes.txt");
    }
    
    // 다양한 테스트 이미지로 성능 측정
    std::vector<cv::Mat> test_images;
    test_images.push_back(TestImageGenerator::createYOLOTestImage(640, 640));
    test_images.push_back(TestImageGenerator::createSolidColorImage(640, 640, cv::Scalar(128, 128, 128)));
    test_images.push_back(TestImageGenerator::createNoisyImage(640, 640, 0.3));
    
    auto start_time = std::chrono::high_resolution_clock::now();
    int total_detections = 0;
    
    // 각 이미지를 5번씩 처리
    for (int iteration = 0; iteration < 5; ++iteration) {
        for (const auto& image : test_images) {
            auto results = detector->detectMultipleTargets(image);
            total_detections += static_cast<int>(results.size());
            
            // 결과 출력 (첫 번째 반복에서만)
            if (iteration == 0) {
                std::cout << "Image " << (&image - &test_images[0]) << " detections: " 
                          << results.size() << std::endl;
                for (const auto& result : results) {
                    std::cout << "  - " << result.label 
                              << " (conf: " << result.confidence << ")" << std::endl;
                }
            }
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    int total_images = test_images.size() * 5;
    double avg_time_per_image = static_cast<double>(duration.count()) / total_images;
    double fps = 1000.0 / avg_time_per_image;
    
    std::cout << "YOLO v11 Performance Summary:" << std::endl;
    std::cout << "  Total images processed: " << total_images << std::endl;
    std::cout << "  Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "  Average time per image: " << avg_time_per_image << " ms" << std::endl;
    std::cout << "  Estimated FPS: " << fps << std::endl;
    std::cout << "  Total detections: " << total_detections << std::endl;
    
    // 성능 기준: 이미지당 처리 시간이 5초를 넘지 않아야 함
    EXPECT_LT(avg_time_per_image, 5000.0);
}

// YOLO v11 백엔드 비교 테스트
TEST_F(YOLOv11IntegrationTest, BackendComparisonTest) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not available";
    }
    
    cv::Mat test_image = TestImageGenerator::createYOLOTestImage(640, 640);
    
    // OpenCV DNN 백엔드 테스트
    {
        auto detector = std::make_unique<ObjectDetector>();
        bool load_success = detector->loadYOLOv11Model("models/yolo11n.onnx", YOLOBackend::OPENCV_DNN);
        
        if (load_success) {
            auto start = std::chrono::high_resolution_clock::now();
            auto results = detector->detectMultipleTargets(test_image);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            std::cout << "OpenCV DNN Backend:" << std::endl;
            std::cout << "  Processing time: " << duration.count() << " ms" << std::endl;
            std::cout << "  Detections: " << results.size() << std::endl;
        } else {
            std::cout << "OpenCV DNN Backend: Model loading failed" << std::endl;
        }
    }
    
#ifdef HAVE_ONNXRUNTIME
    // ONNX Runtime 백엔드 테스트
    {
        auto detector = std::make_unique<ObjectDetector>();
        bool load_success = detector->loadYOLOv11Model("models/yolo11n.onnx", YOLOBackend::ONNX_RUNTIME);
        
        if (load_success) {
            auto start = std::chrono::high_resolution_clock::now();
            auto results = detector->detectMultipleTargets(test_image);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            std::cout << "ONNX Runtime Backend:" << std::endl;
            std::cout << "  Processing time: " << duration.count() << " ms" << std::endl;
            std::cout << "  Detections: " << results.size() << std::endl;
        } else {
            std::cout << "ONNX Runtime Backend: Model loading failed" << std::endl;
        }
    }
#else
    std::cout << "ONNX Runtime Backend: Not available (not compiled with ONNX Runtime)" << std::endl;
#endif
    
    SUCCEED();
}

// 메모리 사용량 및 누수 테스트
TEST_F(YOLOv11IntegrationTest, MemoryUsageTest) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not available";
    }
    
    // 여러 검출기 인스턴스를 순차적으로 생성/삭제하여 메모리 누수 확인
    for (int i = 0; i < 10; ++i) {
        auto detector = std::make_unique<ObjectDetector>();
        
        bool load_success = detector->loadYOLOv11Model("models/yolo11n.onnx");
        if (load_success) {
            cv::Mat test_image = TestImageGenerator::createYOLOTestImage(640, 640);
            detector->detectMultipleTargets(test_image);
        }
        
        detector.reset();  // 명시적 해제
    }
    
    std::cout << "Memory usage test completed - created and destroyed 10 detector instances" << std::endl;
    SUCCEED();
}

// GPU 가속 통합 테스트
TEST_F(YOLOv11IntegrationTest, GPUAccelerationIntegration) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not available";
    }
    
    auto detector = std::make_unique<ObjectDetector>();
    
    // GPU 사용 설정
    detector->setUseGPU(true);
    
    bool load_success = detector->loadYOLOv11Model("models/yolo11n.onnx");
    if (!load_success) {
        GTEST_SKIP() << "GPU model loading failed (CUDA not available or other GPU issues)";
    }
    
    cv::Mat test_image = TestImageGenerator::createYOLOTestImage(640, 640);
    
    // GPU 검출 성능 측정
    auto start = std::chrono::high_resolution_clock::now();
    auto results = detector->detectMultipleTargets(test_image);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "GPU Acceleration Test:" << std::endl;
    std::cout << "  GPU enabled: " << (detector->isUsingGPU() ? "Yes" : "No") << std::endl;
    std::cout << "  Processing time: " << duration.count() << " ms" << std::endl;
    std::cout << "  Detections: " << results.size() << std::endl;
    
    SUCCEED();
}

// 실시간 처리 시뮬레이션 테스트
TEST_F(YOLOv11IntegrationTest, RealTimeProcessingSimulation) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not available";
    }
    
    auto detector = std::make_unique<ObjectDetector>();
    bool load_success = detector->loadYOLOv11Model("models/yolo11n.onnx");
    if (!load_success) {
        GTEST_SKIP() << "Model loading failed";
    }
    
    // 실시간 처리 시뮬레이션 (30 FPS 목표)
    const int target_fps = 30;
    const int duration_seconds = 3;
    const int total_frames = target_fps * duration_seconds;
    
    std::vector<double> processing_times;
    int successful_frames = 0;
    
    auto simulation_start = std::chrono::high_resolution_clock::now();
    
    for (int frame = 0; frame < total_frames; ++frame) {
        // 다양한 이미지 생성
        cv::Mat test_image;
        switch (frame % 3) {
            case 0:
                test_image = TestImageGenerator::createYOLOTestImage(640, 640);
                break;
            case 1:
                test_image = TestImageGenerator::createSolidColorImage(640, 640, cv::Scalar(100, 150, 200));
                break;
            default:
                test_image = TestImageGenerator::createNoisyImage(640, 640, 0.2);
                break;
        }
        
        auto frame_start = std::chrono::high_resolution_clock::now();
        auto results = detector->detectMultipleTargets(test_image);
        auto frame_end = std::chrono::high_resolution_clock::now();
        
        auto frame_time = std::chrono::duration_cast<std::chrono::microseconds>(
            frame_end - frame_start).count() / 1000.0;  // ms
        
        processing_times.push_back(frame_time);
        successful_frames++;
        
        // 목표 FPS를 위한 대기 시간 계산
        double target_frame_time = 1000.0 / target_fps;  // ms
        if (frame_time < target_frame_time) {
            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<int>(target_frame_time - frame_time)));
        }
    }
    
    auto simulation_end = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        simulation_end - simulation_start).count();
    
    // 통계 계산
    double avg_processing_time = 0;
    double max_processing_time = 0;
    for (double time : processing_times) {
        avg_processing_time += time;
        max_processing_time = std::max(max_processing_time, time);
    }
    avg_processing_time /= processing_times.size();
    
    double actual_fps = (successful_frames * 1000.0) / total_time;
    
    std::cout << "Real-time Processing Simulation Results:" << std::endl;
    std::cout << "  Target FPS: " << target_fps << std::endl;
    std::cout << "  Actual FPS: " << actual_fps << std::endl;
    std::cout << "  Successful frames: " << successful_frames << "/" << total_frames << std::endl;
    std::cout << "  Average processing time: " << avg_processing_time << " ms" << std::endl;
    std::cout << "  Maximum processing time: " << max_processing_time << " ms" << std::endl;
    std::cout << "  Total simulation time: " << total_time << " ms" << std::endl;
    
    // 성능 기준: 평균 처리 시간이 목표 프레임 시간의 80% 이내
    double target_frame_time = 1000.0 / target_fps;
    EXPECT_LT(avg_processing_time, target_frame_time * 0.8);
    
    // 실시간 처리 가능성: 실제 FPS가 목표 FPS의 90% 이상
    EXPECT_GT(actual_fps, target_fps * 0.9);
}

// 시스템 안정성 스트레스 테스트
TEST_F(YOLOv11IntegrationTest, SystemStabilityStressTest) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not available";
    }
    
    auto detector = std::make_unique<ObjectDetector>();
    bool load_success = detector->loadYOLOv11Model("models/yolo11n.onnx");
    if (!load_success) {
        GTEST_SKIP() << "Model loading failed";
    }
    
    std::atomic<bool> stop_test{false};
    std::atomic<int> total_processed{0};
    std::atomic<int> errors{0};
    
    // 여러 스레드에서 동시에 검출 수행 (스트레스 테스트)
    std::vector<std::thread> workers;
    const int num_threads = 3;
    
    for (int t = 0; t < num_threads; ++t) {
        workers.emplace_back([&detector, &stop_test, &total_processed, &errors]() {
            while (!stop_test) {
                try {
                    cv::Mat test_image = TestImageGenerator::createYOLOTestImage(416, 416);  // 더 작은 크기
                    auto results = detector->detectMultipleTargets(test_image);
                    total_processed++;
                } catch (...) {
                    errors++;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    // 5초간 스트레스 테스트 실행
    std::this_thread::sleep_for(std::chrono::seconds(5));
    stop_test = true;
    
    for (auto& worker : workers) {
        worker.join();
    }
    
    std::cout << "System Stability Stress Test Results:" << std::endl;
    std::cout << "  Total processed frames: " << total_processed.load() << std::endl;
    std::cout << "  Errors encountered: " << errors.load() << std::endl;
    std::cout << "  Success rate: " << (100.0 * total_processed.load() / (total_processed.load() + errors.load())) << "%" << std::endl;
    
    // 에러율이 5% 미만이어야 함
    double error_rate = static_cast<double>(errors.load()) / (total_processed.load() + errors.load());
    EXPECT_LT(error_rate, 0.05);
    
    // 최소한의 처리량이 있어야 함
    EXPECT_GT(total_processed.load(), 50);
}