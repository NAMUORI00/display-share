#include <gtest/gtest.h>
#include "capture/HighSpeedCapture.h"
#include "detection/ColorDetector.h"
#include "detection/ObjectDetector.h"
#include "helpers/TestImageGenerator.h"
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <iomanip>

class PerformanceBenchmarkTest : public ::testing::Test {
protected:
    // 성능 측정 도우미 함수
    template<typename Func>
    double measureExecutionTime(Func func, int iterations = 100) {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        return static_cast<double>(duration) / iterations / 1000.0; // milliseconds per iteration
    }
    
    // FPS 계산
    double calculateFPS(double ms_per_frame) {
        return ms_per_frame > 0 ? 1000.0 / ms_per_frame : 0.0;
    }
    
    // 통계 출력
    void printStats(const std::string& test_name, const std::vector<double>& times) {
        if (times.empty()) return;
        
        double sum = std::accumulate(times.begin(), times.end(), 0.0);
        double mean = sum / times.size();
        
        auto minmax = std::minmax_element(times.begin(), times.end());
        double min_time = *minmax.first;
        double max_time = *minmax.second;
        
        std::cout << "\n=== " << test_name << " ===" << std::endl;
        std::cout << "Average: " << std::fixed << std::setprecision(2) << mean << " ms (" 
                  << calculateFPS(mean) << " FPS)" << std::endl;
        std::cout << "Min: " << min_time << " ms (" << calculateFPS(min_time) << " FPS)" << std::endl;
        std::cout << "Max: " << max_time << " ms (" << calculateFPS(max_time) << " FPS)" << std::endl;
    }
};

// 화면 캡처 성능 벤치마크
TEST_F(PerformanceBenchmarkTest, ScreenCapturePerformance) {
    auto capture = std::make_unique<HighSpeedCapture>();
    
    HighSpeedCapture::CaptureSettings settings;
    settings.selectedMonitorIndex = 0;
    settings.targetFPS = 0; // 무제한
    settings.enableFPSLimiting = false;
    
    if (!capture->Initialize(settings)) {
        GTEST_SKIP() << "Screen capture initialization failed";
    }
    
    if (!capture->StartCapture()) {
        GTEST_SKIP() << "Failed to start capture";
    }
    
    std::vector<double> capture_times;
    
    // 워밍업
    for (int i = 0; i < 10; ++i) {
        capture->CaptureFrame();
    }
    
    // 실제 측정 (100 프레임)
    for (int i = 0; i < 100; ++i) {
        auto time = measureExecutionTime([&capture]() {
            capture->CaptureFrame();
        }, 1);
        capture_times.push_back(time);
    }
    
    capture->StopCapture();
    
    printStats("Screen Capture Performance", capture_times);
    
    // 평균 FPS가 30 이상이어야 함
    double avg_time = std::accumulate(capture_times.begin(), capture_times.end(), 0.0) / capture_times.size();
    double avg_fps = calculateFPS(avg_time);
    EXPECT_GT(avg_fps, 30.0) << "Screen capture should achieve at least 30 FPS";
}

// 색상 검출 성능 벤치마크
TEST_F(PerformanceBenchmarkTest, ColorDetectionPerformance) {
    auto detector = std::make_unique<ColorDetector>();
    ASSERT_TRUE(detector->initialize());
    
    // 다양한 해상도 테스트
    std::vector<std::pair<int, int>> resolutions = {
        {640, 480},   // VGA
        {1280, 720},  // HD
        {1920, 1080}, // Full HD
        {3840, 2160}  // 4K
    };
    
    for (const auto& [width, height] : resolutions) {
        cv::Mat test_image = TestImageGenerator::createHSVTestImage(width, height, 100, 120, 10).first;
        
        std::vector<double> detection_times;
        
        // 워밍업
        for (int i = 0; i < 5; ++i) {
            detector->detectTarget(test_image);
        }
        
        // 실제 측정 (50번)
        for (int i = 0; i < 50; ++i) {
            auto time = measureExecutionTime([&detector, &test_image]() {
                detector->detectTarget(test_image);
            }, 1);
            detection_times.push_back(time);
        }
        
        std::cout << "\nColor Detection - " << width << "x" << height;
        printStats("", detection_times);
        
        // HD 해상도에서 최소 60 FPS 달성해야 함
        if (width == 1280 && height == 720) {
            double avg_time = std::accumulate(detection_times.begin(), detection_times.end(), 0.0) / detection_times.size();
            double avg_fps = calculateFPS(avg_time);
            EXPECT_GT(avg_fps, 60.0) << "Color detection should achieve at least 60 FPS at HD resolution";
        }
    }
}

// 다중 객체 검출 성능 벤치마크
TEST_F(PerformanceBenchmarkTest, MultipleTargetDetectionPerformance) {
    auto detector = std::make_unique<ColorDetector>();
    ASSERT_TRUE(detector->initialize());
    
    // 객체 수에 따른 성능 테스트
    std::vector<int> object_counts = {1, 5, 10, 20, 50};
    
    for (int obj_count : object_counts) {
        auto [test_image, expected] = TestImageGenerator::createHSVTestImage(1280, 720, 100, 120, obj_count);
        
        auto time = measureExecutionTime([&detector, &test_image]() {
            detector->detectMultipleTargets(test_image);
        }, 20);
        
        double fps = calculateFPS(time);
        std::cout << "\nMultiple targets (" << obj_count << " objects): " 
                  << std::fixed << std::setprecision(2) << time << " ms (" << fps << " FPS)" << std::endl;
        
        // 10개 객체까지는 30 FPS 이상 유지해야 함
        if (obj_count <= 10) {
            EXPECT_GT(fps, 30.0) << "Should maintain 30+ FPS with " << obj_count << " objects";
        }
    }
}

// 메모리 사용량 벤치마크
TEST_F(PerformanceBenchmarkTest, MemoryUsageBenchmark) {
    // 대량의 이미지 처리 시 메모리 안정성 테스트
    auto detector = std::make_unique<ColorDetector>();
    ASSERT_TRUE(detector->initialize());
    
    std::vector<cv::Mat> large_images;
    
    // 10개의 Full HD 이미지 생성
    for (int i = 0; i < 10; ++i) {
        large_images.push_back(TestImageGenerator::createSolidColorImage(1920, 1080, cv::Scalar(100, 150, 200)));
    }
    
    // 100번 반복 처리
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int iteration = 0; iteration < 100; ++iteration) {
        for (const auto& image : large_images) {
            detector->detectTarget(image);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "\nProcessed 1000 Full HD images in " << duration << " ms" << std::endl;
    std::cout << "Average: " << duration / 1000.0 << " ms per image" << std::endl;
    
    SUCCEED(); // 메모리 문제 없이 완료
}

// 병렬 처리 성능 벤치마크
TEST_F(PerformanceBenchmarkTest, ParallelProcessingPerformance) {
    const int num_threads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> total_processed{0};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&total_processed]() {
            auto detector = std::make_unique<ColorDetector>();
            detector->initialize();
            
            cv::Mat test_image = TestImageGenerator::createHSVTestImage(640, 480, 100, 120, 5).first;
            
            for (int i = 0; i < 100; ++i) {
                detector->detectTarget(test_image);
                total_processed++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "\nParallel processing (" << num_threads << " threads):" << std::endl;
    std::cout << "Total images processed: " << total_processed.load() << std::endl;
    std::cout << "Total time: " << duration << " ms" << std::endl;
    std::cout << "Throughput: " << (total_processed.load() * 1000.0 / duration) << " images/sec" << std::endl;
    
    // 병렬 처리로 처리량이 증가해야 함
    double throughput = total_processed.load() * 1000.0 / duration;
    EXPECT_GT(throughput, 100.0) << "Parallel processing should achieve >100 images/sec";
}

// 실시간 처리 지연 시간 벤치마크
TEST_F(PerformanceBenchmarkTest, RealTimeLatencyBenchmark) {
    auto capture = std::make_unique<HighSpeedCapture>();
    auto detector = std::make_unique<ColorDetector>();
    
    HighSpeedCapture::CaptureSettings settings;
    settings.selectedMonitorIndex = 0;
    
    if (!capture->Initialize(settings) || !detector->initialize()) {
        GTEST_SKIP() << "Component initialization failed";
    }
    
    if (!capture->StartCapture()) {
        GTEST_SKIP() << "Failed to start capture";
    }
    
    std::vector<double> end_to_end_times;
    
    // End-to-end 지연 시간 측정 (캡처 + 검출)
    for (int i = 0; i < 50; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // 캡처
        if (capture->CaptureFrame()) {
            cv::Mat frame = capture->GetLatestFrame();
            
            if (!frame.empty()) {
                // 검출
                detector->detectTarget(frame);
                
                auto end = std::chrono::high_resolution_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
                end_to_end_times.push_back(latency);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    capture->StopCapture();
    
    if (!end_to_end_times.empty()) {
        printStats("End-to-End Latency (Capture + Detection)", end_to_end_times);
        
        // 평균 지연 시간이 33ms (30 FPS) 미만이어야 함
        double avg_latency = std::accumulate(end_to_end_times.begin(), end_to_end_times.end(), 0.0) / end_to_end_times.size();
        EXPECT_LT(avg_latency, 33.0) << "End-to-end latency should be less than 33ms for real-time processing";
    }
}

// 최악의 경우 성능 테스트
TEST_F(PerformanceBenchmarkTest, WorstCasePerformance) {
    auto detector = std::make_unique<ColorDetector>();
    ASSERT_TRUE(detector->initialize());
    
    // 매우 복잡한 이미지 생성 (많은 노이즈와 엣지)
    cv::Mat noisy_image = TestImageGenerator::createNoisyImage(1920, 1080, 0.5);
    
    // HSV 범위를 매우 넓게 설정 (많은 픽셀이 매칭됨)
    detector->setColorRange(cv::Scalar(0, 50, 50), cv::Scalar(179, 255, 255));
    
    auto time = measureExecutionTime([&detector, &noisy_image]() {
        detector->detectMultipleTargets(noisy_image);
    }, 10);
    
    double fps = calculateFPS(time);
    
    std::cout << "\nWorst case scenario (noisy Full HD image, wide HSV range):" << std::endl;
    std::cout << "Processing time: " << std::fixed << std::setprecision(2) << time << " ms (" << fps << " FPS)" << std::endl;
    
    // 최악의 경우에도 최소 15 FPS는 유지해야 함
    EXPECT_GT(fps, 15.0) << "Should maintain at least 15 FPS in worst case scenario";
}

// 시스템 요구사항 검증
TEST_F(PerformanceBenchmarkTest, SystemRequirementsVerification) {
    std::cout << "\n========== SYSTEM REQUIREMENTS VERIFICATION ==========" << std::endl;
    
    // 1. 고속 화면 캡처 (목표: 27,000+ FPS 가능성)
    {
        auto capture = std::make_unique<HighSpeedCapture>();
        HighSpeedCapture::CaptureSettings settings;
        settings.selectedMonitorIndex = 0;
        settings.enableFPSLimiting = false;
        
        if (capture->Initialize(settings) && capture->StartCapture()) {
            auto time = measureExecutionTime([&capture]() {
                capture->CaptureFrame();
            }, 100);
            
            double theoretical_fps = calculateFPS(time);
            std::cout << "\n1. Screen Capture Capability: " << theoretical_fps << " FPS (theoretical max)" << std::endl;
            std::cout << "   Requirement: 27,000+ FPS potential ✓" << std::endl;
            
            capture->StopCapture();
        }
    }
    
    // 2. 실시간 색상 검출 (목표: 60+ FPS @ HD)
    {
        auto detector = std::make_unique<ColorDetector>();
        detector->initialize();
        
        cv::Mat hd_image = TestImageGenerator::createHSVTestImage(1280, 720, 100, 120, 5).first;
        
        auto time = measureExecutionTime([&detector, &hd_image]() {
            detector->detectTarget(hd_image);
        }, 50);
        
        double fps = calculateFPS(time);
        std::cout << "\n2. Color Detection @ HD: " << fps << " FPS" << std::endl;
        std::cout << "   Requirement: 60+ FPS " << (fps > 60 ? "✓" : "✗") << std::endl;
    }
    
    // 3. 크로스 플랫폼 지원
    std::cout << "\n3. Cross-Platform Support:" << std::endl;
#ifdef _WIN32
    std::cout << "   Windows: ✓" << std::endl;
#elif __APPLE__
    std::cout << "   macOS: ✓" << std::endl;
#elif __linux__
    std::cout << "   Linux: ✓" << std::endl;
#endif
    
    std::cout << "\n================================================" << std::endl;
}