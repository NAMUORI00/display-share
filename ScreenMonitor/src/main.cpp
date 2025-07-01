#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>

// GUI and Monitoring includes
#include "gui/MainInterface.h"
#include "monitoring/PerformanceMonitor.h"
#include "core/ConfigManager.h"

int main() {
    std::cout << "=== SmartScreenCapture v1.0 ===" << std::endl;
    std::cout << "High-Performance Screen Capture & Real-time Computer Vision" << std::endl;
    std::cout << "============================================" << std::endl;
    
    // Display system information
    nlohmann::json status_info = {
        {"name", "SmartScreenCapture"},
        {"version", "1.0"},
        {"gui_status", "Fully Enabled"},
        {"build_type", "Static Linking"},
        {"dependencies", {
            {"imgui", "Latest"},
            {"glfw", "3.x"},
            {"opencv", "4.11+"},
            {"nlohmann_json", "3.11+"},
            {"screen_capture_lite", "Latest"}
        }}
    };
    
    std::cout << "Application Status: " << status_info.dump(2) << std::endl;
    std::cout << "\nFeatures enabled:" << std::endl;
    std::cout << "- ImGui GUI with GLFW + OpenGL3" << std::endl;
    std::cout << "- Real-time screen capture (screen_capture_lite)" << std::endl;
    std::cout << "- OpenCV computer vision (static linked)" << std::endl;
    std::cout << "- HSV color detection & YOLO v11 object detection" << std::endl;
    std::cout << "- Real-time performance monitoring" << std::endl;
    
    // Initialize the complete PerformanceMonitor system
    std::unique_ptr<PerformanceMonitor> monitor;
    try {
        std::cout << "\nInitializing Smart Screen Capture System..." << std::endl;
        monitor = std::make_unique<PerformanceMonitor>();
        
        if (!monitor->Initialize()) {
            std::cerr << "Failed to initialize PerformanceMonitor system" << std::endl;
            return -1;
        }
        
        std::cout << "\n=== System Initialization Complete ===" << std::endl;
        std::cout << "You can now:" << std::endl;
        std::cout << "1. Select a monitor from the dropdown" << std::endl;
        std::cout << "2. Click 'Start Capture' to begin real-time screen capture" << std::endl;
        std::cout << "3. Configure HSV color detection settings" << std::endl;
        std::cout << "4. Enable YOLO v11 object detection" << std::endl;
        std::cout << "5. Monitor real-time performance metrics" << std::endl;
        std::cout << "\nClose the GUI window to exit." << std::endl;
        
        // Run the main application loop
        monitor->Run();
        
    } catch (const std::exception& e) {
        std::cerr << "Critical error: " << e.what() << std::endl;
        std::cerr << "Please check that:" << std::endl;
        std::cerr << "1. Your system supports OpenGL 3.3" << std::endl;
        std::cerr << "2. Screen capture permissions are granted" << std::endl;
        std::cerr << "3. All submodules are properly initialized" << std::endl;
        return -1;
    }
    
    std::cout << "\n=== Application Shutdown ===" << std::endl;
    std::cout << "Smart Screen Capture system has been shut down successfully." << std::endl;
    std::cout << "Thank you for using the Educational Computer Vision Framework!" << std::endl;
    
    return 0;
}