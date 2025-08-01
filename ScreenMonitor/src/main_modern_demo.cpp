/**
 * @file main_modern_demo.cpp
 * @brief Demonstration of modernized 320x320 ROI Detection GUI System
 * 
 * This demonstrates the integration of the modernized MainInterface
 * with the 320x320 center region capture system and detection algorithms.
 */

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <vector>
#include <random>

// Core includes
#include "core/ConfigManager.h"
#include "capture/CenterRegionCapture.h"
#include "capture/ScreenCaptureLiteDevice.h"
#include "gui/MainInterface.h"
#include "monitoring/SimpleMetrics.h"

// OpenCV for image processing
#include <opencv2/opencv.hpp>

/**
 * @brief Demonstration class for 320x320 ROI Detection System
 */
class ModernROIDemo {
public:
    ModernROIDemo() 
        : m_gui(std::make_unique<MainInterface>())
        , m_centerCapture(std::make_unique<CenterRegionCapture>())
        , m_metrics(std::make_unique<ScreenMonitor::SimpleMetrics>())
        , m_screenCapture(std::make_unique<ScreenCaptureLiteDevice>())
        , m_isRunning(false)
        , m_generator(std::random_device{}())
        , m_distribution(0.0f, 1.0f)
    {
    }

    ~ModernROIDemo() = default;

    bool Initialize() {
        std::cout << "\n=== Modern 320x320 ROI Detection System Demo ===" << std::endl;
        
        // Initialize GUI
        std::cout << "Step 1: Initializing Modern GUI System..." << std::endl;
        if (!m_gui->Initialize()) {
            std::cerr << "ERROR: Failed to initialize GUI!" << std::endl;
            return false;
        }
        std::cout << "✓ Modern GUI System initialized successfully" << std::endl;

        // Initialize screen capture
        std::cout << "Step 2: Initializing Screen Capture..." << std::endl;
        if (!m_screenCapture->Initialize()) {
            std::cerr << "ERROR: Failed to initialize screen capture!" << std::endl;
            return false;
        }
        std::cout << "✓ Screen Capture initialized successfully" << std::endl;

        // Optimize center capture for expected screen size
        m_centerCapture->OptimizeMemoryAllocation(1920, 1080); // Common resolution
        std::cout << "✓ 320x320 ROI Capture system optimized" << std::endl;

        std::cout << "\n=== System Ready for Demonstration ===" << std::endl;
        return true;
    }

    void Run() {
        if (!Initialize()) {
            return;
        }

        std::cout << "\nStarting Modern 320x320 ROI Detection Demo..." << std::endl;
        std::cout << "Features demonstrated:" << std::endl;
        std::cout << "• Professional ImGui docking interface" << std::endl;
        std::cout << "• Real-time 320x320 center region capture" << std::endl;
        std::cout << "• HSV color detection visualization" << std::endl;
        std::cout << "• YOLO object detection simulation" << std::endl;
        std::cout << "• Performance monitoring dashboard" << std::endl;
        
        m_isRunning = true;
        auto last_frame_time = std::chrono::steady_clock::now();
        auto last_detection_time = std::chrono::steady_clock::now();
        int frame_count = 0;

        while (!m_gui->ShouldClose() && m_isRunning) {
            auto current_time = std::chrono::steady_clock::now();
            
            try {
                // Handle events
                m_gui->HandleEvents();

                // Update capture and detection (simulate real processing)
                if (m_gui->IsCaptureEnabled()) {
                    UpdateCaptureAndDetection(current_time);
                    frame_count++;
                }

                // Update performance metrics
                UpdatePerformanceMetrics(current_time, last_frame_time, frame_count);

                // Render GUI
                m_gui->Render();

                // Control frame rate
                std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS

                last_frame_time = current_time;

            } catch (const std::exception& e) {
                std::cerr << "ERROR in main loop: " << e.what() << std::endl;
                break;
            }
        }

        std::cout << "\nDemo completed after " << frame_count << " frames" << std::endl;
        Cleanup();
    }

private:
    void UpdateCaptureAndDetection(const std::chrono::steady_clock::time_point& current_time) {
        // Simulate screen capture and 320x320 ROI extraction
        static auto last_capture = std::chrono::steady_clock::now();
        
        if (std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_capture).count() >= 33) { // ~30 FPS capture
            
            // Generate synthetic 320x320 test image
            cv::Mat roi_frame = GenerateSyntheticROIFrame();
            
            // Calculate region info for demonstration
            CenterRegionCapture::RegionInfo region_info;
            region_info.x = 800;          // Center of 1920 width
            region_info.y = 380;          // Center of 1080 height
            region_info.width = 320;
            region_info.height = 320;
            region_info.source_width = 1920;
            region_info.source_height = 1080;
            region_info.scale_x = 1.0;
            region_info.scale_y = 1.0;

            // Update GUI with ROI frame
            m_gui->UpdateROIFrame(roi_frame, region_info);

            // Simulate HSV detection
            if (m_gui->IsHSVDetectionEnabled()) {
                UpdateHSVDetectionDemo();
            }

            // Simulate YOLO detection
            if (m_gui->IsYOLODetectionEnabled()) {
                UpdateYOLODetectionDemo();
            }

            last_capture = current_time;
        }
    }

    cv::Mat GenerateSyntheticROIFrame() {
        // Create a synthetic 320x320 test image with patterns
        cv::Mat frame(320, 320, CV_8UC3);
        
        // Create gradient background
        for (int y = 0; y < 320; ++y) {
            for (int x = 0; x < 320; ++x) {
                frame.at<cv::Vec3b>(y, x) = cv::Vec3b(
                    static_cast<uchar>((x * 255) / 320),           // Blue gradient
                    static_cast<uchar>((y * 255) / 320),           // Green gradient
                    static_cast<uchar>(((x + y) * 255) / 640)      // Red gradient
                );
            }
        }

        // Add some geometric shapes for detection
        cv::circle(frame, cv::Point(160, 160), 50, cv::Scalar(0, 255, 255), -1); // Yellow circle
        cv::rectangle(frame, cv::Point(50, 50), cv::Point(150, 100), cv::Scalar(255, 0, 255), -1); // Magenta rectangle
        cv::rectangle(frame, cv::Point(200, 200), cv::Point(280, 250), cv::Scalar(0, 255, 0), 2); // Green rectangle outline

        // Add text overlay
        cv::putText(frame, "320x320 ROI", cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
        
        return frame;
    }

    void UpdateHSVDetectionDemo() {
        // Simulate HSV color detection results
        std::vector<cv::Point> hsv_detections;
        
        // Generate random detection points within 320x320
        int num_detections = 3 + static_cast<int>(m_distribution(m_generator) * 7); // 3-10 detections
        
        for (int i = 0; i < num_detections; ++i) {
            cv::Point detection(
                static_cast<int>(m_distribution(m_generator) * 320),
                static_cast<int>(m_distribution(m_generator) * 320)
            );
            hsv_detections.push_back(detection);
        }

        m_gui->UpdateHSVDetections(hsv_detections);
    }

    void UpdateYOLODetectionDemo() {
        // Simulate YOLO object detection results
        std::vector<cv::Rect> yolo_detections;
        std::vector<float> confidence_scores;
        std::vector<std::string> class_names;

        // Generate random YOLO detections
        int num_objects = 1 + static_cast<int>(m_distribution(m_generator) * 4); // 1-5 objects

        const std::vector<std::string> demo_classes = {
            "person", "car", "bicycle", "motorcycle", "bus", "truck", 
            "traffic_light", "stop_sign", "cat", "dog", "chair", "laptop"
        };

        for (int i = 0; i < num_objects; ++i) {
            // Random bounding box within 320x320
            int x = static_cast<int>(m_distribution(m_generator) * 250); // Leave room for width
            int y = static_cast<int>(m_distribution(m_generator) * 250); // Leave room for height
            int w = 30 + static_cast<int>(m_distribution(m_generator) * 50); // 30-80 width
            int h = 30 + static_cast<int>(m_distribution(m_generator) * 50); // 30-80 height

            // Ensure bounds
            w = std::min(w, 320 - x);
            h = std::min(h, 320 - y);

            yolo_detections.emplace_back(x, y, w, h);
            confidence_scores.push_back(0.3f + m_distribution(m_generator) * 0.7f); // 0.3-1.0 confidence
            
            // Random class name
            int class_idx = static_cast<int>(m_distribution(m_generator) * demo_classes.size());
            class_names.push_back(demo_classes[class_idx]);
        }

        m_gui->UpdateYOLODetections(yolo_detections, confidence_scores, class_names);
    }

    void UpdatePerformanceMetrics(const std::chrono::steady_clock::time_point& current_time,
                                 const std::chrono::steady_clock::time_point& last_frame_time,
                                 int frame_count) {
        // Calculate FPS
        auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_frame_time);
        double fps = 1000000.0 / frame_duration.count();

        // Calculate processing time (simulate)
        double process_time = 5.0 + m_distribution(m_generator) * 10.0; // 5-15ms simulation

        // Update metrics
        m_metrics->updateFPS(fps);
        m_metrics->updateProcessTime(process_time);

        // Update GUI FPS
        m_gui->UpdateFPS(static_cast<float>(fps));

        // Debug output every 2 seconds
        static auto last_debug = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(current_time - last_debug).count() >= 2) {
            std::cout << "Demo Status - FPS: " << fps << ", Process Time: " << process_time << "ms" << std::endl;
            last_debug = current_time;
        }
    }

    void Cleanup() {
        std::cout << "\n=== Cleaning up Demo Resources ===" << std::endl;
        
        if (m_screenCapture) {
            m_screenCapture->Cleanup();
        }
        
        if (m_gui) {
            m_gui->Cleanup();
        }
        
        std::cout << "✓ Demo cleanup completed" << std::endl;
    }

private:
    std::unique_ptr<MainInterface> m_gui;
    std::unique_ptr<CenterRegionCapture> m_centerCapture;
    std::unique_ptr<ScreenMonitor::SimpleMetrics> m_metrics;
    std::unique_ptr<ScreenCaptureLiteDevice> m_screenCapture;
    
    bool m_isRunning;
    std::mt19937 m_generator;
    std::uniform_real_distribution<float> m_distribution;
};

/**
 * @brief Main entry point for Modern 320x320 ROI Detection Demo
 */
#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#else
int main() {
#endif
    try {
        std::cout << "Starting Modern 320x320 ROI Detection System Demo..." << std::endl;
        
        ModernROIDemo demo;
        demo.Run();
        
        std::cout << "\nDemo completed successfully!" << std::endl;
        std::cout << "\nKey features demonstrated:" << std::endl;
        std::cout << "✓ Professional docking interface with 4 panels" << std::endl;
        std::cout << "✓ Real-time 320x320 ROI visualization" << std::endl;
        std::cout << "✓ HSV detection coordinate tracking" << std::endl;
        std::cout << "✓ YOLO object detection with confidence scores" << std::endl;
        std::cout << "✓ Performance monitoring with FPS graphs" << std::endl;
        std::cout << "✓ Modern dark theme with professional styling" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
        std::cerr << "Please ensure:" << std::endl;
        std::cerr << "1. OpenGL 3.3+ support" << std::endl;
        std::cerr << "2. All dependencies are available" << std::endl;
        std::cerr << "3. Screen capture permissions granted" << std::endl;
        
#ifdef _DEBUG
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
#endif
        return -1;
    }
}