// Main application entry for SmartScreenCapture Phase 2
// Supports two modes:
//  - GUI mode (default): ImGui + GLFW persistent loop
//  - Performance test mode: define RUN_PERFORMANCE_TEST (cmake -DSC_RUN_PERF_TEST=ON)

#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <exception>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>

// GUI and Monitoring includes
#include "gui/MainInterface.h"
#include "core/ConfigManager.h"
#include "capture/ScreenCaptureLiteDevice.h"

/**
 * @brief 애플리케이션 정보 출력
 */
void PrintApplicationInfo() {
    std::cout << "=== PHASE 2: 320x320 CENTER REGION CAPTURE SYSTEM ===" << std::endl;
    std::cout << "Testing Center Region Capture Optimization Implementation" << std::endl;
    std::cout << "High-Performance 320x320 Center Region Extraction" << std::endl;
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
    // Debug console for troubleshooting GUI startup issues
    #ifdef _DEBUG
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
    std::cout.clear();
    std::cerr.clear();
    std::cin.clear();
    SetConsoleTitle(L"SmartScreenCapture Debug Console");
    std::cout << "=== DEBUG CONSOLE ACTIVE ===" << std::endl;
    #endif
#else
int main() {
#endif
    PrintApplicationInfo();

#ifdef RUN_PERFORMANCE_TEST
    std::cout << "\n=== PERFORMANCE TEST MODE (10s) ===" << std::endl;
    try {
        auto device = std::make_unique<ScreenCaptureLiteDevice>();
        CaptureSettings settings; settings.target_fps = 60; settings.quality = 85;
        if(!device->Initialize(settings)) { std::cerr << "Init failed" << std::endl; return -1; }
        if(!device->StartCapture(0)) { std::cerr << "StartCapture failed" << std::endl; return -1; }
        device->SetCenterRegionMode(true);
        auto start = std::chrono::high_resolution_clock::now();
        int frame_cnt=0, center_cnt=0; const int DURATION=10;
        while(true){
            auto now=std::chrono::high_resolution_clock::now();
            if(std::chrono::duration_cast<std::chrono::seconds>(now-start).count()>=DURATION) break;
            cv::Mat f; if(device->CaptureFrame(f)){ frame_cnt++; cv::Mat c; if(device->GetCenterRegion(c)) center_cnt++; }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        device->StopCapture();
        std::cout << "Frames:"<<frame_cnt<<" Center:"<<center_cnt<<" FPS:"<<(frame_cnt/10) << std::endl;
        std::cout << device->GetPerformanceStats() << std::endl;
        std::cout << device->GetCenterRegionPerformanceStats() << std::endl;
    } catch(const std::exception& e){ std::cerr << e.what() << std::endl; return -1; }
    return 0;
#else
    // ================= GUI MODE =================
    std::unique_ptr<MainInterface> gui;
    try {
        std::cout << "\n=== Starting GUI Initialization ===" << std::endl;
        gui = std::make_unique<MainInterface>();
        if(!gui->Initialize()) {
            std::cerr << "GUI initialization failed" << std::endl;
            return -1;
        }
        std::cout << "GUI Initialized - entering main loop" << std::endl;
        int frame_count = 0; auto start_time = std::chrono::steady_clock::now();
        while(!gui->ShouldClose()) {
            gui->HandleEvents();
            gui->Render();
            frame_count++;
            if(frame_count % 300 == 0) {
                auto now = std::chrono::steady_clock::now();
                auto secs = std::chrono::duration_cast<std::chrono::seconds>(now-start_time).count();
                std::cout << "GUI running: " << frame_count << " frames, " << secs << " s" << std::endl;
            }
        }
        std::cout << "Main loop exited after " << frame_count << " frames" << std::endl;
    } catch(const std::exception& e) {
        std::cerr << "Critical GUI error: " << e.what() << std::endl; return -1;
    }
    std::cout << "Application shutdown complete" << std::endl;
    return 0;
#endif

    // Original GUI code commented out for testing
    /*
    // Initialize the GUI system with enhanced error handling
    std::unique_ptr<MainInterface> gui;
    try {
        std::cout << "\n=== Starting GUI Initialization ===" << std::endl;
        std::cout << "Step 1: Creating MainInterface object..." << std::endl;
        gui = std::make_unique<MainInterface>();
        std::cout << "Step 1: SUCCESS - MainInterface created" << std::endl;
        
        std::cout << "Step 2: Initializing GUI components..." << std::endl;
        if (!gui->Initialize()) {
            std::cerr << "ERROR: GUI initialization failed!" << std::endl;
            std::cerr << "Common causes:" << std::endl;
            std::cerr << "1. OpenGL 3.3 not supported by graphics driver" << std::endl;
            std::cerr << "2. GLFW initialization failure" << std::endl;
            std::cerr << "3. Window creation failure" << std::endl;
            std::cerr << "4. ImGui context creation failure" << std::endl;
            
            #ifdef _DEBUG
            std::cout << "Press Enter to exit..." << std::endl;
            std::cin.get();
            #endif
            return -1;
        }
        std::cout << "Step 2: SUCCESS - GUI initialized" << std::endl;
        
        std::cout << "\n=== System Initialization Complete ===" << std::endl;
        std::cout << "Windows GUI Application is now ready!" << std::endl;
        std::cout << "Features available:" << std::endl;
        std::cout << "1. Select a monitor from the dropdown" << std::endl;
        std::cout << "2. Click 'Start Capture' to begin real-time screen capture" << std::endl;
        std::cout << "3. Configure HSV color detection settings" << std::endl;
        std::cout << "4. Enable YOLO v11 object detection" << std::endl;
        std::cout << "5. Monitor real-time performance metrics" << std::endl;
        std::cout << "\nClose the GUI window to exit." << std::endl;
        
        // Run the main GUI application loop with debugging
        std::cout << "\nStep 3: Starting main application loop..." << std::endl;
        int frame_count = 0;
        auto start_time = std::chrono::steady_clock::now();
        
        while (!gui->ShouldClose()) {
            try {
                gui->HandleEvents();
                gui->Render();
                frame_count++;
                
                // Debug output every 60 frames (approximately 1 second at 60 FPS)
                if (frame_count % 60 == 0) {
                    auto current_time = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
                    std::cout << "GUI running: " << frame_count << " frames, " << elapsed << " seconds" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "ERROR in main loop: " << e.what() << std::endl;
                break;
            }
        }
        
        std::cout << "Main loop exited after " << frame_count << " frames" << std::endl;
        
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