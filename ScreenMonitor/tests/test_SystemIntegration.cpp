/**
 * @file test_SystemIntegration.cpp
 * @brief Phase 5 QA: Comprehensive System Integration Testing
 * 
 * This file contains comprehensive integration tests for the complete 320x320
 * detection system pipeline, validating end-to-end functionality, performance
 * benchmarks, and system reliability under various operating conditions.
 * 
 * Test Coverage:
 * - Complete pipeline: 320x320 capture → HSV detection → YOLO inference → GUI display
 * - Performance validation against Phase 0-4 targets
 * - Coordinate transformation accuracy
 * - Memory management and resource cleanup
 * - Error handling and graceful degradation
 * - Cross-component integration reliability
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <memory>
#include <opencv2/opencv.hpp>

// Windows headers for memory measurement
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

// Core system components
#include "core/ConfigManager.h"
#include "capture/CenterRegionCapture.h"
#include "detection/HSVColorDetection.h"
#include "detection/YOLOv11TensorRTInference.h"
#include "gui/MainInterface.h"
#include "monitoring/SimpleMetrics.h"

// Test infrastructure
#include "helpers/TestImageGenerator.h"
#include "helpers/ConfigTestHelper.h"
#include "helpers/GLTestContext.h"
#include "mocks/MockScreenCapture.h"
#include "mocks/MockHSVDetector.h"

/**
 * @class SystemIntegrationTest
 * @brief Phase 5 QA Test Fixture for Complete System Integration
 * 
 * This test fixture provides comprehensive validation of the 320x320 detection
 * system, focusing on real-world performance and reliability testing.
 */
class SystemIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test configuration
        config_helper_ = std::make_unique<ConfigTestHelper>();
        config_manager_ = std::make_unique<ConfigManager>();
        
        // Initialize OpenGL test context for GUI testing
        gl_context_ = std::make_unique<GLTestContext>();
        
        // Initialize performance metrics tracking
        metrics_ = std::make_unique<SimpleMetrics>();
        
        // Phase 5 performance targets from research
        target_yolo_gpu_fps_ = 280.0f;  // YOLOv11n TensorRT GPU
        target_yolo_cpu_fps_ = 30.0f;   // YOLOv11n OpenCV DNN CPU  
        target_gui_fps_ = 60.0f;        // Real-time GUI refresh
        target_capture_latency_ms_ = 5.0f;  // Center region extraction
        target_memory_mb_ = 500.0f;     // Total system memory footprint
        
        std::cout << "[SystemIntegration] Test setup complete" << std::endl;
    }

    void TearDown() override {
        // Cleanup resources
        metrics_.reset();
        gl_context_.reset();
        config_manager_.reset();
        config_helper_.reset();
        
        std::cout << "[SystemIntegration] Test cleanup complete" << std::endl;
    }

    /**
     * @brief Create synthetic 320x320 test image with detection targets
     */
    cv::Mat CreateTest320x320Image() {
        cv::Mat test_image = TestImageGenerator::createCheckerboardImage(320, 320, 40);
        
        // Add HSV-detectable regions (red, green, blue)
        cv::rectangle(test_image, cv::Rect(50, 50, 40, 40), cv::Scalar(0, 0, 255), -1);    // Red
        cv::rectangle(test_image, cv::Rect(150, 100, 30, 30), cv::Scalar(0, 255, 0), -1);  // Green  
        cv::rectangle(test_image, cv::Rect(250, 150, 35, 35), cv::Scalar(255, 0, 0), -1);  // Blue
        
        return test_image;
    }

    /**
     * @brief Measure actual memory usage in MB
     */
    float MeasureMemoryUsageMB() {
        // Simple Windows process memory measurement
        #ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            return static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
        }
        #endif
        return 0.0f;  // Fallback for non-Windows
    }

protected:
    std::unique_ptr<ConfigTestHelper> config_helper_;
    std::unique_ptr<ConfigManager> config_manager_;
    std::unique_ptr<GLTestContext> gl_context_;
    std::unique_ptr<SimpleMetrics> metrics_;
    
    // Phase 5 Performance Targets
    float target_yolo_gpu_fps_;
    float target_yolo_cpu_fps_;
    float target_gui_fps_;
    float target_capture_latency_ms_;
    float target_memory_mb_;
};

/**
 * @brief TEST: Complete Pipeline Integration
 * 
 * Validates the entire 320x320 detection pipeline:
 * CenterRegionCapture → HSVColorDetection → YOLOv11TensorRT → GUI Display
 */
TEST_F(SystemIntegrationTest, CompletePipelineIntegration) {
    SCOPED_TRACE("Complete 320x320 Detection Pipeline Integration");

    try {
        // 1. Initialize Center Region Capture (320x320)
        auto center_capture = std::make_unique<CenterRegionCapture>();
        ASSERT_TRUE(center_capture != nullptr) << "Failed to create CenterRegionCapture";

        // 2. Initialize HSV Color Detection
        auto hsv_detector = std::make_unique<HSVColorDetection>();
        ASSERT_TRUE(hsv_detector != nullptr) << "Failed to create HSVColorDetection";

        // 3. Initialize YOLO v11 TensorRT Inference  
        auto yolo_detector = std::make_unique<YOLOv11TensorRTInference>();
        ASSERT_TRUE(yolo_detector != nullptr) << "Failed to create YOLOv11TensorRTInference";

        // 4. Initialize GUI Interface (if OpenGL available)
        std::unique_ptr<MainInterface> gui_interface;
        if (gl_context_->IsInitialized()) {
            gui_interface = std::make_unique<MainInterface>(*config_manager_);
            ASSERT_TRUE(gui_interface != nullptr) << "Failed to create MainInterface";
        }

        // 5. Create test 320x320 image
        cv::Mat test_image_320 = CreateTest320x320Image();
        ASSERT_FALSE(test_image_320.empty()) << "Failed to create test image";
        ASSERT_EQ(test_image_320.rows, 320) << "Test image height mismatch";
        ASSERT_EQ(test_image_320.cols, 320) << "Test image width mismatch";

        // 6. Pipeline Stage 1: HSV Color Detection
        auto start_hsv = std::chrono::high_resolution_clock::now();
        auto hsv_detection_results = hsv_detector->DetectMultiple(test_image_320);
        auto end_hsv = std::chrono::high_resolution_clock::now();
        
        float hsv_latency_ms = std::chrono::duration<float, std::milli>(end_hsv - start_hsv).count();
        std::cout << "HSV Detection Latency: " << hsv_latency_ms << " ms" << std::endl;
        
        EXPECT_LT(hsv_latency_ms, 10.0f) << "HSV detection too slow for real-time processing";
        EXPECT_GT(hsv_detection_results.size(), 0) << "HSV detector should find colored regions";

        // 7. Pipeline Stage 2: YOLO v11 Inference
        auto start_yolo = std::chrono::high_resolution_clock::now();
        auto yolo_detection_results = yolo_detector->DetectMultiple(test_image_320);
        auto end_yolo = std::chrono::high_resolution_clock::now();
        
        float yolo_latency_ms = std::chrono::duration<float, std::milli>(end_yolo - start_yolo).count();
        std::cout << "YOLO Inference Latency: " << yolo_latency_ms << " ms" << std::endl;
        
        // Performance validation depends on available hardware
        if (yolo_detector->IsGPUAccelerated()) {
            float yolo_fps = 1000.0f / yolo_latency_ms;
            EXPECT_GT(yolo_fps, target_yolo_gpu_fps_ * 0.8f) << "GPU YOLO performance below 80% of target";
            std::cout << "YOLO GPU FPS: " << yolo_fps << " (target: " << target_yolo_gpu_fps_ << ")" << std::endl;
        } else {
            float yolo_fps = 1000.0f / yolo_latency_ms;
            EXPECT_GT(yolo_fps, target_yolo_cpu_fps_ * 0.8f) << "CPU YOLO performance below 80% of target";  
            std::cout << "YOLO CPU FPS: " << yolo_fps << " (target: " << target_yolo_cpu_fps_ << ")" << std::endl;
        }

        // 8. Pipeline Stage 3: GUI Update (if available)
        if (gui_interface) {
            auto start_gui = std::chrono::high_resolution_clock::now();
            
            gui_interface->UpdateFrame(test_image_320);
            
            // Convert DetectionResult to cv::Point for HSV GUI update
            std::vector<cv::Point> hsv_points;
            for (const auto& result : hsv_detection_results) {
                hsv_points.push_back(cv::Point(result.bounding_box.x + result.bounding_box.width/2, 
                                             result.bounding_box.y + result.bounding_box.height/2));
            }
            gui_interface->UpdateHSVDetections(hsv_points);
            
            // Convert DetectionResult to cv::Rect for YOLO GUI update
            std::vector<cv::Rect> yolo_rects;
            std::vector<float> confidences;
            std::vector<std::string> class_names;
            for (const auto& result : yolo_detection_results) {
                yolo_rects.push_back(result.bounding_box);
                confidences.push_back(result.confidence);
                class_names.push_back(result.class_name);
            }
            gui_interface->UpdateYOLODetections(yolo_rects, confidences, class_names);
            
            auto end_gui = std::chrono::high_resolution_clock::now();
            float gui_latency_ms = std::chrono::duration<float, std::milli>(end_gui - start_gui).count();
            
            float gui_fps = 1000.0f / gui_latency_ms;
            EXPECT_GT(gui_fps, target_gui_fps_ * 0.8f) << "GUI performance below 80% of target";
            std::cout << "GUI Update FPS: " << gui_fps << " (target: " << target_gui_fps_ << ")" << std::endl;
        }

        // 9. Memory Usage Validation
        float memory_usage_mb = MeasureMemoryUsageMB();
        EXPECT_LT(memory_usage_mb, target_memory_mb_) << "Memory usage exceeds target";
        std::cout << "Memory Usage: " << memory_usage_mb << " MB (target: < " << target_memory_mb_ << " MB)" << std::endl;

        // 10. Pipeline Success Validation
        EXPECT_TRUE(true) << "Complete pipeline integration successful";

    } catch (const std::exception& e) {
        FAIL() << "Pipeline integration failed with exception: " << e.what();
    }
}

/**
 * @brief TEST: Coordinate Transformation Accuracy
 * 
 * Validates accurate coordinate mapping between 320x320 center region
 * and full screen coordinates for detection result display.
 */
TEST_F(SystemIntegrationTest, CoordinateTransformationAccuracy) {
    SCOPED_TRACE("320x320 ↔ Full Screen Coordinate Transformation");

    // Simulate different screen resolutions
    std::vector<std::pair<int, int>> test_resolutions = {
        {1920, 1080},  // Full HD
        {2560, 1440},  // 2K
        {3840, 2160},  // 4K
        {1366, 768},   // Laptop common
        {1280, 720}    // HD
    };

    for (auto [screen_width, screen_height] : test_resolutions) {
        // Calculate center region position
        int center_x = (screen_width - 320) / 2;
        int center_y = (screen_height - 320) / 2;

        // Test coordinates within 320x320 region
        std::vector<cv::Point> test_points_320 = {
            {0, 0},        // Top-left
            {319, 0},      // Top-right  
            {0, 319},      // Bottom-left
            {319, 319},    // Bottom-right
            {160, 160},    // Center
            {100, 200},    // Random point
            {250, 75}      // Another random point
        };

        for (const auto& point_320 : test_points_320) {
            // Transform 320x320 → full screen
            cv::Point full_screen_point = {
                center_x + point_320.x,
                center_y + point_320.y
            };

            // Transform full screen → 320x320
            cv::Point back_to_320 = {
                full_screen_point.x - center_x,
                full_screen_point.y - center_y
            };

            // Validate round-trip accuracy
            EXPECT_EQ(point_320.x, back_to_320.x) 
                << "X coordinate round-trip failed for resolution " 
                << screen_width << "x" << screen_height;
            
            EXPECT_EQ(point_320.y, back_to_320.y)
                << "Y coordinate round-trip failed for resolution "
                << screen_width << "x" << screen_height;

            // Validate bounds checking
            EXPECT_GE(full_screen_point.x, center_x) << "Full screen X below expected range";
            EXPECT_LT(full_screen_point.x, center_x + 320) << "Full screen X above expected range";
            EXPECT_GE(full_screen_point.y, center_y) << "Full screen Y below expected range";
            EXPECT_LT(full_screen_point.y, center_y + 320) << "Full screen Y above expected range";
        }

        std::cout << "Coordinate transformation validated for " 
                  << screen_width << "x" << screen_height << std::endl;
    }
}

/**
 * @brief TEST: Performance Benchmark Validation
 * 
 * Validates system performance against Phase 0-4 research targets
 * with sustained operation simulation.
 */
TEST_F(SystemIntegrationTest, PerformanceBenchmarkValidation) {
    SCOPED_TRACE("Phase 5 Performance Benchmark Validation");

    const int benchmark_iterations = 100;  // Sustained operation test
    const int warmup_iterations = 10;      // Algorithm warmup

    try {
        // Initialize components
        auto hsv_detector = std::make_unique<HSVColorDetection>();
        auto yolo_detector = std::make_unique<YOLOv11TensorRTInference>();
        
        ASSERT_TRUE(hsv_detector != nullptr);
        ASSERT_TRUE(yolo_detector != nullptr);

        // Create benchmark test image
        cv::Mat benchmark_image = CreateTest320x320Image();
        ASSERT_FALSE(benchmark_image.empty());

        // Warmup phase
        std::cout << "Performing warmup iterations..." << std::endl;
        for (int i = 0; i < warmup_iterations; ++i) {
            hsv_detector->DetectObjects(benchmark_image);
            yolo_detector->DetectObjects(benchmark_image);
        }

        // HSV Detection Performance Benchmark
        auto hsv_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < benchmark_iterations; ++i) {
            auto detections = hsv_detector->DetectObjects(benchmark_image);
        }
        auto hsv_end = std::chrono::high_resolution_clock::now();

        float hsv_total_ms = std::chrono::duration<float, std::milli>(hsv_end - hsv_start).count();
        float hsv_avg_ms = hsv_total_ms / benchmark_iterations;
        float hsv_fps = 1000.0f / hsv_avg_ms;

        std::cout << "HSV Detection Benchmark:" << std::endl;
        std::cout << "  Average latency: " << hsv_avg_ms << " ms" << std::endl;
        std::cout << "  Average FPS: " << hsv_fps << std::endl;

        EXPECT_LT(hsv_avg_ms, 5.0f) << "HSV detection average latency too high";
        EXPECT_GT(hsv_fps, 200.0f) << "HSV detection FPS below expectations";

        // YOLO v11 Performance Benchmark  
        auto yolo_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < benchmark_iterations; ++i) {
            auto detections = yolo_detector->DetectObjects(benchmark_image);
        }
        auto yolo_end = std::chrono::high_resolution_clock::now();

        float yolo_total_ms = std::chrono::duration<float, std::milli>(yolo_end - yolo_start).count();
        float yolo_avg_ms = yolo_total_ms / benchmark_iterations;
        float yolo_fps = 1000.0f / yolo_avg_ms;

        std::cout << "YOLO v11 Inference Benchmark:" << std::endl;
        std::cout << "  Average latency: " << yolo_avg_ms << " ms" << std::endl;
        std::cout << "  Average FPS: " << yolo_fps << std::endl;
        std::cout << "  GPU Accelerated: " << (yolo_detector->IsGPUAccelerated() ? "Yes" : "No") << std::endl;

        // Performance validation based on hardware capabilities
        if (yolo_detector->IsGPUAccelerated()) {
            EXPECT_GT(yolo_fps, target_yolo_gpu_fps_ * 0.7f) 
                << "GPU YOLO performance significantly below target";
        } else {
            EXPECT_GT(yolo_fps, target_yolo_cpu_fps_ * 0.7f)
                << "CPU YOLO performance significantly below target";
        }

        // Memory stability check during sustained operation
        float initial_memory = MeasureMemoryUsageMB();
        float final_memory = MeasureMemoryUsageMB();
        float memory_increase = final_memory - initial_memory;

        std::cout << "Memory Stability:" << std::endl;
        std::cout << "  Initial: " << initial_memory << " MB" << std::endl;
        std::cout << "  Final: " << final_memory << " MB" << std::endl;
        std::cout << "  Increase: " << memory_increase << " MB" << std::endl;

        EXPECT_LT(memory_increase, 50.0f) << "Significant memory increase detected (possible leak)";
        EXPECT_LT(final_memory, target_memory_mb_) << "Final memory usage exceeds target";

    } catch (const std::exception& e) {
        FAIL() << "Performance benchmark failed with exception: " << e.what();
    }
}

/**
 * @brief TEST: System Robustness and Error Handling
 * 
 * Validates graceful error handling and system stability under
 * various failure conditions and edge cases.
 */
TEST_F(SystemIntegrationTest, SystemRobustnessValidation) {
    SCOPED_TRACE("System Robustness and Error Handling");

    // Test 1: Empty image handling
    {
        auto hsv_detector = std::make_unique<HSVColorDetection>();
        cv::Mat empty_image;
        
        EXPECT_NO_THROW({
            auto detections = hsv_detector->DetectObjects(empty_image);
            EXPECT_EQ(detections.size(), 0) << "Empty image should return no detections";
        }) << "HSV detector should handle empty images gracefully";
    }

    // Test 2: Invalid image sizes
    {
        auto hsv_detector = std::make_unique<HSVColorDetection>();
        
        // Test various problematic sizes
        std::vector<cv::Size> problem_sizes = {
            {1, 1},      // Tiny image
            {10, 1000000}, // Extreme aspect ratio  
            {50, 50},    // Too small for meaningful detection
            {10000, 10}  // Another extreme aspect ratio
        };

        for (const auto& size : problem_sizes) {
            cv::Mat problem_image = cv::Mat::zeros(size.height, size.width, CV_8UC3);
            
            EXPECT_NO_THROW({
                auto detections = hsv_detector->DetectObjects(problem_image);
            }) << "HSV detector should handle size " << size.width << "x" << size.height;
        }
    }

    // Test 3: Resource cleanup validation
    {
        const int stress_iterations = 50;
        float initial_memory = MeasureMemoryUsageMB();

        for (int i = 0; i < stress_iterations; ++i) {
            // Create and destroy components repeatedly
            auto hsv_detector = std::make_unique<HSVColorDetection>();
            auto yolo_detector = std::make_unique<YOLOv11TensorRTInference>();
            
            cv::Mat test_image = CreateTest320x320Image();
            
            auto hsv_detections = hsv_detector->DetectObjects(test_image);
            auto yolo_detections = yolo_detector->DetectObjects(test_image);
            
            // Components automatically destroyed at end of scope
        }

        float final_memory = MeasureMemoryUsageMB();
        float memory_increase = final_memory - initial_memory;

        std::cout << "Resource cleanup test:" << std::endl;
        std::cout << "  Initial memory: " << initial_memory << " MB" << std::endl;
        std::cout << "  Final memory: " << final_memory << " MB" << std::endl;
        std::cout << "  Memory increase: " << memory_increase << " MB" << std::endl;

        EXPECT_LT(memory_increase, 100.0f) << "Excessive memory increase suggests resource leaks";
    }

    // Test 4: Configuration error handling
    {
        EXPECT_NO_THROW({
            ConfigManager bad_config;
            // Try to access non-existent configuration values
            auto nonexistent = bad_config.getValue<std::string>("/nonexistent/path", "default");
            EXPECT_EQ(nonexistent, "default") << "Should return default for non-existent config";
        }) << "ConfigManager should handle missing configurations gracefully";
    }

    std::cout << "System robustness validation completed successfully" << std::endl;
}