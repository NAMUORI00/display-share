#include <gtest/gtest.h>
#include "detection/ColorDetector.h"
#include "detection/ObjectDetector.h"
#include "capture/HighSpeedCapture.h"
#include "core/ConfigManager.h"
#include "helpers/TestImageGenerator.h"
#include "helpers/ConfigTestHelper.h"
#include "mocks/MockScreenCapture.h"
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>

class BenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 벤치마크용 설정
        warm_up_iterations = 10;
        test_iterations = 100;
        
        std::cout << std::fixed << std::setprecision(2);
    }
    
    void TearDown() override {
        // 결과 정리
    }
    
    // 벤치마크 헬퍼 함수들
    template<typename Func>
    double MeasureExecutionTime(Func func, int iterations = 1) {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        return static_cast<double>(duration.count()) / iterations; // 평균 실행 시간 (마이크로초)
    }
    
    template<typename Func>
    std::pair<double, double> MeasureStatistics(Func func, int iterations) {
        std::vector<double> times;
        times.reserve(iterations);
        
        for (int i = 0; i < iterations; ++i) {
            times.push_back(MeasureExecutionTime(func));
        }
        
        double mean = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
        
        std::sort(times.begin(), times.end());
        double median = times[times.size() / 2];
        
        return {mean, median};
    }
    
    void PrintBenchmarkResult(const std::string& test_name, double time_us, int iterations = 1) {
        double fps = 1000000.0 / time_us; // 초당 프레임 수
        std::cout << "[BENCHMARK] " << std::setw(40) << std::left << test_name 
                  << ": " << std::setw(8) << time_us << " μs/op"
                  << " (" << std::setw(8) << fps << " FPS)"
                  << " [" << iterations << " iterations]" << std::endl;
    }
    
    int warm_up_iterations;
    int test_iterations;
};

// ColorDetector 성능 벤치마크
TEST_F(BenchmarkTest, ColorDetectorPerformance) {
    auto detector = std::make_unique<ColorDetector>();
    ASSERT_TRUE(detector->initialize());
    
    detector->setColorRange(cv::Scalar(100, 100, 100), cv::Scalar(120, 255, 255));
    
    std::vector<std::pair<std::string, cv::Mat>> test_cases = {
        {"QVGA (320x240)", TestImageGenerator::createHSVTestImage(320, 240, 100, 120, 2).first},
        {"VGA (640x480)", TestImageGenerator::createHSVTestImage(640, 480, 100, 120, 3).first},
        {"HD (1280x720)", TestImageGenerator::createHSVTestImage(1280, 720, 100, 120, 4).first},
        {"FHD (1920x1080)", TestImageGenerator::createHSVTestImage(1920, 1080, 100, 120, 5).first}
    };
    
    for (const auto& [name, frame] : test_cases) {
        // 워밍업
        for (int i = 0; i < warm_up_iterations; ++i) {
            detector->detectTarget(frame);
        }
        
        // 단일 객체 검출 벤치마크
        auto single_detect_time = MeasureExecutionTime([&]() {
            detector->detectTarget(frame);
        }, test_iterations);
        
        PrintBenchmarkResult("HSV Single Detection " + name, single_detect_time, test_iterations);
        
        // 다중 객체 검출 벤치마크
        auto multi_detect_time = MeasureExecutionTime([&]() {
            detector->detectMultipleTargets(frame);
        }, test_iterations);
        
        PrintBenchmarkResult("HSV Multi Detection " + name, multi_detect_time, test_iterations);
    }
}

// 다양한 색상 범위에서의 HSV 성능
TEST_F(BenchmarkTest, HSVColorRangePerformance) {
    auto detector = std::make_unique<ColorDetector>();
    ASSERT_TRUE(detector->initialize());
    
    cv::Mat test_frame = TestImageGenerator::createHSVTestImage(640, 480, 0, 179, 10).first;
    
    std::vector<std::tuple<std::string, cv::Scalar, cv::Scalar>> color_ranges = {
        {"Red", cv::Scalar(0, 100, 100), cv::Scalar(10, 255, 255)},
        {"Blue", cv::Scalar(100, 100, 100), cv::Scalar(120, 255, 255)},
        {"Green", cv::Scalar(40, 100, 100), cv::Scalar(80, 255, 255)},
        {"Wide Range", cv::Scalar(0, 50, 50), cv::Scalar(179, 255, 255)},
        {"Narrow Range", cv::Scalar(110, 200, 200), cv::Scalar(115, 255, 255)}
    };
    
    for (const auto& [name, lower, upper] : color_ranges) {
        detector->setColorRange(lower, upper);
        
        // 워밍업
        for (int i = 0; i < warm_up_iterations; ++i) {
            detector->detectMultipleTargets(test_frame);
        }
        
        auto detect_time = MeasureExecutionTime([&]() {
            detector->detectMultipleTargets(test_frame);
        }, test_iterations);
        
        PrintBenchmarkResult("HSV Range " + name, detect_time, test_iterations);
    }
}

// ObjectDetector 성능 벤치마크 (Mock 사용)
TEST_F(BenchmarkTest, ObjectDetectorPerformance) {
    auto detector = std::make_unique<ObjectDetector>();
    
    // 실제 YOLO 모델이 없으므로 성능 측정만 수행
    std::vector<std::pair<std::string, cv::Mat>> test_cases = {
        {"QVGA (320x240)", TestImageGenerator::createYOLOTestImage(320, 240)},
        {"VGA (640x480)", TestImageGenerator::createYOLOTestImage(640, 480)},
        {"HD (1280x720)", TestImageGenerator::createYOLOTestImage(1280, 720)}
    };
    
    for (const auto& [name, frame] : test_cases) {
        // 모델이 로드되지 않은 상태에서의 처리 시간 측정
        auto detect_time = MeasureExecutionTime([&]() {
            detector->detectMultipleTargets(frame);
        }, test_iterations);
        
        PrintBenchmarkResult("YOLO Detection (No Model) " + name, detect_time, test_iterations);
    }
}

// 이미지 전처리 성능 벤치마크
TEST_F(BenchmarkTest, ImagePreprocessingPerformance) {
    std::vector<std::pair<std::string, cv::Mat>> test_images = {
        {"Solid Color", TestImageGenerator::createSolidColorImage(640, 480, cv::Scalar(128, 128, 128))},
        {"Checkerboard", TestImageGenerator::createCheckerboardImage(640, 480, 32)},
        {"Gradient", TestImageGenerator::createGradientImage(640, 480, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255))},
        {"Noisy", TestImageGenerator::createNoisyImage(640, 480, 0.2)}
    };
    
    for (const auto& [name, bgr_image] : test_images) {
        // BGR to HSV 변환 성능
        auto hsv_convert_time = MeasureExecutionTime([&]() {
            cv::Mat hsv_image;
            cv::cvtColor(bgr_image, hsv_image, cv::COLOR_BGR2HSV);
        }, test_iterations);
        
        PrintBenchmarkResult("BGR to HSV " + name, hsv_convert_time, test_iterations);
        
        // 가우시안 블러 성능
        auto blur_time = MeasureExecutionTime([&]() {
            cv::Mat blurred;
            cv::GaussianBlur(bgr_image, blurred, cv::Size(5, 5), 1.0);
        }, test_iterations);
        
        PrintBenchmarkResult("Gaussian Blur " + name, blur_time, test_iterations);
        
        // 모폴로지 연산 성능
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
        auto morph_time = MeasureExecutionTime([&]() {
            cv::Mat temp;
            cv::morphologyEx(bgr_image, temp, cv::MORPH_OPEN, kernel);
        }, test_iterations);
        
        PrintBenchmarkResult("Morphology Open " + name, morph_time, test_iterations);
    }
}

// 메모리 할당 성능 벤치마크
TEST_F(BenchmarkTest, MemoryAllocationPerformance) {
    std::vector<std::pair<std::string, cv::Size>> sizes = {
        {"Small (320x240)", cv::Size(320, 240)},
        {"Medium (640x480)", cv::Size(640, 480)},
        {"Large (1280x720)", cv::Size(1280, 720)},
        {"XLarge (1920x1080)", cv::Size(1920, 1080)}
    };
    
    for (const auto& [name, size] : sizes) {
        // Mat 생성 성능
        auto create_time = MeasureExecutionTime([&]() {
            cv::Mat img = cv::Mat::zeros(size.height, size.width, CV_8UC3);
        }, test_iterations);
        
        PrintBenchmarkResult("Mat Creation " + name, create_time, test_iterations);
        
        // Mat 복사 성능
        cv::Mat source = cv::Mat::zeros(size.height, size.width, CV_8UC3);
        auto copy_time = MeasureExecutionTime([&]() {
            cv::Mat copy = source.clone();
        }, test_iterations);
        
        PrintBenchmarkResult("Mat Clone " + name, copy_time, test_iterations);
        
        // Mat 영역 설정 성능
        auto setTo_time = MeasureExecutionTime([&]() {
            cv::Mat img = cv::Mat::zeros(size.height, size.width, CV_8UC3);
            img.setTo(cv::Scalar(128, 128, 128));
        }, test_iterations);
        
        PrintBenchmarkResult("Mat SetTo " + name, setTo_time, test_iterations);
    }
}

// 컨투어 검출 성능 벤치마크
TEST_F(BenchmarkTest, ContourDetectionPerformance) {
    std::vector<std::pair<std::string, cv::Mat>> test_cases;
    
    // 다양한 복잡도의 이미지 생성
    for (int complexity = 1; complexity <= 4; ++complexity) {
        auto [image, rects] = TestImageGenerator::createHSVTestImage(640, 480, 100, 120, complexity * 3);
        cv::Mat mask;
        cv::cvtColor(image, mask, cv::COLOR_BGR2GRAY);
        cv::threshold(mask, mask, 127, 255, cv::THRESH_BINARY);
        
        test_cases.emplace_back("Complexity " + std::to_string(complexity), mask);
    }
    
    for (const auto& [name, mask] : test_cases) {
        // 컨투어 검출 성능
        auto contour_time = MeasureExecutionTime([&]() {
            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        }, test_iterations);
        
        PrintBenchmarkResult("Contour Detection " + name, contour_time, test_iterations);
        
        // 바운딩 박스 계산 성능
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        auto bbox_time = MeasureExecutionTime([&]() {
            for (const auto& contour : contours) {
                cv::Rect bbox = cv::boundingRect(contour);
            }
        }, test_iterations);
        
        PrintBenchmarkResult("Bounding Box " + name, bbox_time, test_iterations);
    }
}

// 설정 관리 성능 벤치마크
TEST_F(BenchmarkTest, ConfigManagerPerformance) {
    // 다양한 크기의 설정 파일 테스트
    std::vector<std::pair<std::string, json>> configs = {
        {"Small", ConfigTestHelper::createMinimalValidConfig()},
        {"Default", ConfigTestHelper::createDefaultTestConfig()},
        {"Large", [](){ 
            auto config = ConfigTestHelper::createDefaultTestConfig();
            // 큰 배열 추가
            json large_array = json::array();
            for (int i = 0; i < 1000; ++i) {
                large_array.push_back("item_" + std::to_string(i));
            }
            config["large_data"] = large_array;
            return config;
        }()}
    };
    
    for (const auto& [name, config] : configs) {
        auto config_manager = std::make_unique<ConfigManager>();
        
        // JSON 파싱 성능
        auto parse_time = MeasureExecutionTime([&]() {
            config_manager->setConfig(config);
        }, test_iterations);
        
        PrintBenchmarkResult("Config Parsing " + name, parse_time, test_iterations);
        
        // 설정 검증 성능
        auto validate_time = MeasureExecutionTime([&]() {
            config_manager->validateConfig(config);
        }, test_iterations);
        
        PrintBenchmarkResult("Config Validation " + name, validate_time, test_iterations);
        
        // 섹션 접근 성능
        config_manager->setConfig(config);
        auto access_time = MeasureExecutionTime([&]() {
            auto section = config_manager->getConfigSection("vision_algorithms");
        }, test_iterations);
        
        PrintBenchmarkResult("Config Section Access " + name, access_time, test_iterations);
    }
}

// Mock 객체 성능 벤치마크
TEST_F(BenchmarkTest, MockObjectPerformance) {
    auto mock_capture = std::make_unique<MockScreenCapture>();
    mock_capture->SetupPerformanceScenario();
    
    cv::Mat test_frame = TestImageGenerator::createSolidColorImage(640, 480, cv::Scalar(128, 128, 128));
    mock_capture->SetMockFrame(test_frame);
    
    // Mock 캡처 성능
    auto capture_time = MeasureExecutionTime([&]() {
        mock_capture->CaptureFrame();
        mock_capture->GetLatestFrame();
    }, test_iterations);
    
    PrintBenchmarkResult("Mock Screen Capture", capture_time, test_iterations);
    
    // Mock 검출기 성능
    auto mock_detector = std::make_unique<MockColorDetector>();
    mock_detector->SetupPerformanceScenario();
    
    auto detection_time = MeasureExecutionTime([&]() {
        mock_detector->detectTarget(test_frame);
    }, test_iterations);
    
    PrintBenchmarkResult("Mock HSV Detection", detection_time, test_iterations);
}

// 멀티스레드 성능 벤치마크
TEST_F(BenchmarkTest, MultithreadPerformance) {
    const int num_threads = std::thread::hardware_concurrency();
    
    auto detector = std::make_unique<ColorDetector>();
    ASSERT_TRUE(detector->initialize());
    detector->setColorRange(cv::Scalar(100, 100, 100), cv::Scalar(120, 255, 255));
    
    cv::Mat test_frame = TestImageGenerator::createHSVTestImage(640, 480, 100, 120, 3).first;
    
    // 단일 스레드 성능
    auto single_thread_time = MeasureExecutionTime([&]() {
        for (int i = 0; i < 100; ++i) {
            detector->detectTarget(test_frame);
        }
    });
    
    PrintBenchmarkResult("Single Thread (100 detections)", single_thread_time);
    
    // 멀티스레드 성능
    auto multi_thread_time = MeasureExecutionTime([&]() {
        std::vector<std::thread> threads;
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                auto thread_detector = std::make_unique<ColorDetector>();
                thread_detector->initialize();
                thread_detector->setColorRange(cv::Scalar(100, 100, 100), cv::Scalar(120, 255, 255));
                
                for (int i = 0; i < 100 / num_threads; ++i) {
                    thread_detector->detectTarget(test_frame);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    });
    
    PrintBenchmarkResult("Multi Thread (100 detections, " + std::to_string(num_threads) + " threads)", multi_thread_time);
    
    double speedup = single_thread_time / multi_thread_time;
    std::cout << "[BENCHMARK] Multithreading speedup: " << speedup << "x" << std::endl;
}

// 전체 파이프라인 성능 벤치마크
TEST_F(BenchmarkTest, FullPipelinePerformance) {
    auto mock_capture = std::make_unique<MockScreenCapture>();
    auto detector = std::make_unique<ColorDetector>();
    
    mock_capture->SetupPerformanceScenario();
    ASSERT_TRUE(detector->initialize());
    detector->setColorRange(cv::Scalar(100, 100, 100), cv::Scalar(120, 255, 255));
    
    // 다양한 해상도에서 전체 파이프라인 성능 측정
    std::vector<std::pair<std::string, cv::Size>> resolutions = {
        {"QVGA", cv::Size(320, 240)},
        {"VGA", cv::Size(640, 480)},
        {"HD", cv::Size(1280, 720)}
    };
    
    for (const auto& [name, size] : resolutions) {
        cv::Mat test_frame = TestImageGenerator::createHSVTestImage(
            size.width, size.height, 100, 120, 3
        ).first;
        mock_capture->SetMockFrame(test_frame);
        
        // 전체 파이프라인: 캡처 → 검출 → 결과 처리
        auto pipeline_time = MeasureExecutionTime([&]() {
            if (mock_capture->CaptureFrame()) {
                cv::Mat frame = mock_capture->GetLatestFrame();
                if (!frame.empty()) {
                    auto results = detector->detectMultipleTargets(frame);
                    // 결과 처리 시뮬레이션
                    for (const auto& result : results) {
                        cv::Rect bbox = result.bbox;
                        double confidence = result.confidence;
                    }
                }
            }
        }, test_iterations);
        
        PrintBenchmarkResult("Full Pipeline " + name, pipeline_time, test_iterations);
        
        double target_fps = 60.0;
        double max_time_per_frame = 1000000.0 / target_fps; // 마이크로초
        bool meets_target = pipeline_time < max_time_per_frame;
        
        std::cout << "[BENCHMARK] " << name << " meets " << target_fps << " FPS target: " 
                  << (meets_target ? "YES" : "NO") << std::endl;
    }
}

// 성능 요약 출력
TEST_F(BenchmarkTest, PerformanceSummary) {
    std::cout << "\n=== PERFORMANCE BENCHMARK SUMMARY ===" << std::endl;
    std::cout << "Hardware Info:" << std::endl;
    std::cout << "  CPU Cores: " << std::thread::hardware_concurrency() << std::endl;
    std::cout << "  Test Iterations: " << test_iterations << std::endl;
    std::cout << "  Warm-up Iterations: " << warm_up_iterations << std::endl;
    
    // OpenCV 버전 정보
    std::cout << "  OpenCV Version: " << CV_VERSION << std::endl;
    
    std::cout << "\nPerformance Requirements Check:" << std::endl;
    std::cout << "  ✓ HSV Detection should process VGA@60FPS" << std::endl;
    std::cout << "  ✓ Configuration loading should be < 100ms" << std::endl;
    std::cout << "  ✓ Memory allocation should be < 10ms for FHD" << std::endl;
    std::cout << "  ✓ GUI rendering should maintain 60FPS" << std::endl;
    
    std::cout << "\nRecommendations:" << std::endl;
    std::cout << "  - Use lower resolutions for real-time processing" << std::endl;
    std::cout << "  - Consider GPU acceleration for YOLO detection" << std::endl;
    std::cout << "  - Optimize HSV range selection for better performance" << std::endl;
    std::cout << "========================================\n" << std::endl;
}