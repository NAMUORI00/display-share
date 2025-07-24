#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>

// GUI and Monitoring includes
#include "gui/MainInterface.h"
#include "core/ConfigManager.h"

/**
 * @brief 애플리케이션 정보 출력
 */
void PrintApplicationInfo() {
    std::cout << "=== Professional Screen Capture & Computer Vision System v1.0 ===" << std::endl;
    std::cout << "Enterprise-Grade High-Performance Screen Capture & Real-time Computer Vision" << std::endl;
    std::cout << "Windows GUI Application with GLFW + ImGui + OpenGL3" << std::endl;
    std::cout << "============================================" << std::endl;
    
    // 시스템 정보 출력
    nlohmann::json status_info = {
        {"name", "SmartScreenCapture"},
        {"version", "1.0"},
        {"gui_status", "Fully Enabled"},
        {"build_type", "Static Linking"},
        {"dependencies", {
            {"imgui", "Latest with GLFW Backend"},
            {"glfw", "3.x"},
            {"opengl", "3.3+"},
            {"opencv", "4.11+ (core modules)"},
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
}

// For Windows GUI applications, use WinMain instead of main
#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Pure GUI application - no console window
#else
int main() {
#endif
    PrintApplicationInfo();
    
    // Initialize the GUI system
    std::unique_ptr<MainInterface> gui;
    try {
        std::cout << "\nInitializing Windows GUI Application..." << std::endl;
        gui = std::make_unique<MainInterface>();
        
        if (!gui->Initialize()) {
            std::cerr << "Failed to initialize GUI system" << std::endl;
            return -1;
        }
        
        std::cout << "\n=== System Initialization Complete ===" << std::endl;
        std::cout << "Windows GUI Application is now ready!" << std::endl;
        std::cout << "You can now:" << std::endl;
        std::cout << "1. Select a monitor from the dropdown" << std::endl;
        std::cout << "2. Click 'Start Capture' to begin real-time screen capture" << std::endl;
        std::cout << "3. Configure HSV color detection settings" << std::endl;
        std::cout << "4. Enable YOLO v11 object detection" << std::endl;
        std::cout << "5. Monitor real-time performance metrics" << std::endl;
        std::cout << "\nClose the GUI window to exit." << std::endl;
        
        // Run the main GUI application loop
        while (!gui->ShouldClose()) {
            gui->HandleEvents();
            gui->Render();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Critical error: " << e.what() << std::endl;
        std::cerr << "Please check that:" << std::endl;
        std::cerr << "1. Your system supports OpenGL 3.3" << std::endl;
        std::cerr << "2. Screen capture permissions are granted" << std::endl;
        std::cerr << "3. All dependencies are properly linked" << std::endl;
        return -1;
    }
    
    std::cout << "\n=== Application Shutdown ===" << std::endl;
    std::cout << "Smart Screen Capture system has been shut down successfully." << std::endl;
    std::cout << "Thank you for using the Professional Screen Capture & Computer Vision System!" << std::endl;
    
#ifdef _WIN32
    // Pure GUI application - no console cleanup needed
#endif
    
    return 0;
}