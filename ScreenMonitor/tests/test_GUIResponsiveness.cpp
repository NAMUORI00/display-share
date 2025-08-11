/**
 * @file test_GUIResponsiveness.cpp
 * @brief Phase 5 QA: Modern 4-Panel GUI Responsiveness Testing
 * 
 * Validation of the modern GUI interface responsiveness, 4-panel layout
 * performance, and real-time visualization capabilities.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <opencv2/opencv.hpp>

// Windows headers for memory measurement
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

#include "gui/MainInterface.h"
#include "core/ConfigManager.h"
#include "helpers/TestImageGenerator.h"
#include "helpers/GLTestContext.h"

class GUIResponsivenessTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_manager_ = std::make_unique<ConfigManager>();
        gl_context_ = std::make_unique<GLTestContext>();
        
        target_gui_fps_ = 60.0f;    // Real-time GUI target
        target_update_latency_ms_ = 16.7f;  // 60 FPS = 16.7ms per frame
        
        if (gl_context_->IsInitialized()) {
            gui_ = std::make_unique<MainInterface>(*config_manager_);
            gui_available_ = true;
        } else {
            gui_available_ = false;
            std::cout << "GUI tests skipped - OpenGL context not available" << std::endl;
        }
    }

    cv::Mat CreateGUITestFrame() {
        return TestImageGenerator::createCheckerboardImage(320, 320, 20);
    }

    std::unique_ptr<ConfigManager> config_manager_;
    std::unique_ptr<GLTestContext> gl_context_;
    std::unique_ptr<MainInterface> gui_;
    bool gui_available_;
    float target_gui_fps_;
    float target_update_latency_ms_;
};

/**
 * @brief TEST: GUI Update Performance
 */
TEST_F(GUIResponsivenessTest, GUIUpdatePerformance) {
    if (!gui_available_) {
        GTEST_SKIP() << "GUI not available in test environment";
    }

    cv::Mat test_frame = CreateGUITestFrame();
    const int iterations = 100;
    
    std::vector<float> update_latencies;
    update_latencies.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        
        gui_->UpdateFrame(test_frame);
        
        auto end = std::chrono::high_resolution_clock::now();
        float latency_ms = std::chrono::duration<float, std::milli>(end - start).count();
        update_latencies.push_back(latency_ms);
    }
    
    float avg_latency = std::accumulate(update_latencies.begin(), update_latencies.end(), 0.0f) / iterations;
    float max_latency = *std::max_element(update_latencies.begin(), update_latencies.end());
    float avg_fps = 1000.0f / avg_latency;
    
    std::cout << "GUI Update Performance:" << std::endl;
    std::cout << "  Average Latency: " << avg_latency << " ms" << std::endl;
    std::cout << "  Maximum Latency: " << max_latency << " ms" << std::endl;
    std::cout << "  Average FPS: " << avg_fps << std::endl;
    
    EXPECT_LT(avg_latency, target_update_latency_ms_) 
        << "GUI update latency exceeds 60 FPS target";
    
    EXPECT_GT(avg_fps, target_gui_fps_ * 0.8f)
        << "GUI FPS below 80% of target";
}

/**
 * @brief TEST: 4-Panel Layout Responsiveness
 */
TEST_F(GUIResponsivenessTest, FourPanelLayoutResponsiveness) {
    if (!gui_available_) {
        GTEST_SKIP() << "GUI not available in test environment";
    }

    cv::Mat test_frame = CreateGUITestFrame();
    
    // Simulate typical detection results (HSV uses Points, not Rects)
    std::vector<cv::Point> hsv_detections = {
        cv::Point(70, 70),   // Center of detected region
        cv::Point(165, 115), // Center of detected region
        cv::Point(267, 167)  // Center of detected region
    };
    
    std::vector<cv::Rect> yolo_detections = {
        cv::Rect(75, 75, 60, 60),
        cv::Rect(180, 120, 50, 50)
    };
    
    std::vector<float> confidence_scores = {0.85f, 0.92f};
    std::vector<std::string> class_names = {"object1", "object2"};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Update all GUI panels
    gui_->UpdateFrame(test_frame);
    gui_->UpdateHSVDetections(hsv_detections);
    gui_->UpdateYOLODetections(yolo_detections, confidence_scores, class_names);
    gui_->UpdateFPS(60.0f);
    
    auto end = std::chrono::high_resolution_clock::now();
    
    float full_update_latency = std::chrono::duration<float, std::milli>(end - start).count();
    
    std::cout << "4-Panel Full Update Latency: " << full_update_latency << " ms" << std::endl;
    
    EXPECT_LT(full_update_latency, target_update_latency_ms_) 
        << "Full 4-panel update too slow for real-time display";
}

/**
 * @brief TEST: GUI Memory Stability
 */
TEST_F(GUIResponsivenessTest, GUIMemoryStability) {
    if (!gui_available_) {
        GTEST_SKIP() << "GUI not available in test environment";
    }

    cv::Mat test_frame = CreateGUITestFrame();
    const int stress_iterations = 500;
    
    // Measure initial memory usage
    #ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX initial_pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&initial_pmc, sizeof(initial_pmc));
    float initial_memory_mb = static_cast<float>(initial_pmc.WorkingSetSize) / (1024.0f * 1024.0f);
    #endif
    
    // Stress test GUI updates
    for (int i = 0; i < stress_iterations; ++i) {
        gui_->UpdateFrame(test_frame);
        
        // Vary detection results to test different update paths
        std::vector<cv::Point> detections;
        if (i % 3 == 0) {
            detections.push_back(cv::Point(i % 200, (i * 2) % 200));
        }
        gui_->UpdateHSVDetections(detections);
        gui_->UpdateFPS(static_cast<float>(60 + (i % 20)));
    }
    
    #ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX final_pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&final_pmc, sizeof(final_pmc));
    float final_memory_mb = static_cast<float>(final_pmc.WorkingSetSize) / (1024.0f * 1024.0f);
    float memory_increase_mb = final_memory_mb - initial_memory_mb;
    
    std::cout << "GUI Memory Stability Test:" << std::endl;
    std::cout << "  Iterations: " << stress_iterations << std::endl;
    std::cout << "  Initial Memory: " << initial_memory_mb << " MB" << std::endl;
    std::cout << "  Final Memory: " << final_memory_mb << " MB" << std::endl;
    std::cout << "  Memory Increase: " << memory_increase_mb << " MB" << std::endl;
    
    EXPECT_LT(memory_increase_mb, 50.0f) << "Excessive memory increase suggests GUI memory leak";
    #endif
}

/**
 * @brief TEST: Real-time Rendering Performance
 */
TEST_F(GUIResponsivenessTest, RealTimeRenderingPerformance) {
    if (!gui_available_) {
        GTEST_SKIP() << "GUI not available in test environment";
    }

    const int render_test_duration_ms = 5000;  // 5 second test
    const auto test_start = std::chrono::high_resolution_clock::now();
    
    int frame_count = 0;
    std::vector<float> frame_times;
    
    cv::Mat test_frame = CreateGUITestFrame();
    
    while (true) {
        auto frame_start = std::chrono::high_resolution_clock::now();
        
        // Simulate realistic GUI update cycle
        gui_->UpdateFrame(test_frame);
        
        // Simulate varying detection loads
        std::vector<cv::Point> detections;
        int detection_count = (frame_count % 10) + 1;  // 1-10 detections
        for (int i = 0; i < detection_count; ++i) {
            detections.push_back(cv::Point(
                (i * 30) % 290, (i * 40) % 280
            ));
        }
        gui_->UpdateHSVDetections(detections);
        gui_->UpdateFPS(static_cast<float>(frame_count % 100));
        
        auto frame_end = std::chrono::high_resolution_clock::now();
        auto elapsed_total = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - test_start);
        
        if (elapsed_total.count() >= render_test_duration_ms) {
            break;
        }
        
        float frame_time_ms = std::chrono::duration<float, std::milli>(frame_end - frame_start).count();
        frame_times.push_back(frame_time_ms);
        frame_count++;
    }
    
    auto test_end = std::chrono::high_resolution_clock::now();
    float total_test_time_ms = std::chrono::duration<float, std::milli>(test_end - test_start).count();
    
    float average_fps = (frame_count * 1000.0f) / total_test_time_ms;
    float avg_frame_time = std::accumulate(frame_times.begin(), frame_times.end(), 0.0f) / frame_times.size();
    float max_frame_time = *std::max_element(frame_times.begin(), frame_times.end());
    
    std::cout << "Real-time Rendering Performance:" << std::endl;
    std::cout << "  Test Duration: " << total_test_time_ms << " ms" << std::endl;
    std::cout << "  Total Frames: " << frame_count << std::endl;
    std::cout << "  Average FPS: " << average_fps << std::endl;
    std::cout << "  Average Frame Time: " << avg_frame_time << " ms" << std::endl;
    std::cout << "  Maximum Frame Time: " << max_frame_time << " ms" << std::endl;
    
    EXPECT_GT(average_fps, target_gui_fps_ * 0.9f) 
        << "Real-time rendering FPS below 90% of target";
    
    EXPECT_LT(max_frame_time, target_update_latency_ms_ * 2.0f)
        << "Maximum frame time indicates potential GUI freezing";
}