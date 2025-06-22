#include "monitoring/PerformanceMonitor.h"
#include "gui/MainInterface.h"
#include "capture/HighSpeedCapture.h"
#include "core/ConfigManager.h"
#include "detection/ColorDetector.h"
#include "detection/ObjectDetector.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <limits>

PerformanceMonitor::PerformanceMonitor() 
    : m_lastUpdateTime(std::chrono::high_resolution_clock::now()) {
}

PerformanceMonitor::~PerformanceMonitor() {
    Cleanup();
}

bool PerformanceMonitor::Initialize() {
    std::cout << "Initializing Smart Screen Capture System..." << std::endl;

    try {
        // Initialize ConfigManager first
        std::cout << "Creating ConfigManager..." << std::endl;
        m_configManager = std::make_unique<ConfigManager>();
        if (!m_configManager->loadConfig("config/config.json")) {
            std::cerr << "Warning: Could not load config.json, using defaults" << std::endl;
        }
        std::cout << "ConfigManager created successfully" << std::endl;

        // Initialize High-Speed Capture System
        std::cout << "Creating HighSpeedCapture..." << std::endl;
        m_screenCapture = std::make_unique<HighSpeedCapture>();
        
        HighSpeedCapture::CaptureSettings settings;
        settings.selectedMonitorIndex = 0; // Primary monitor
        settings.targetFPS = 60.0; // 60 FPS
        settings.enableFPSLimiting = false; // Maximum performance
        
        if (!m_screenCapture->Initialize(settings)) {
            std::cerr << "Failed to initialize screen capture system" << std::endl;
            throw std::runtime_error("Critical: Screen capture initialization failed - screen_capture_lite required");
        }
        std::cout << "HighSpeedCapture created and initialized successfully" << std::endl;

        // Initialize Color Detector (HSV)
        std::cout << "Creating ColorDetector..." << std::endl;
        m_colorDetector = std::make_unique<ColorDetector>();
        std::cout << "ColorDetector created successfully" << std::endl;

        // Initialize Object Detector (YOLO) - Optional
        std::cout << "Creating ObjectDetector..." << std::endl;
        m_objectDetector = std::make_unique<ObjectDetector>();
        std::cout << "ObjectDetector created successfully" << std::endl;

        // Initialize Main GUI Interface
        std::cout << "Creating MainInterface..." << std::endl;
        m_mainInterface = std::make_unique<MainInterface>();
        if (!m_mainInterface->Initialize()) {
            std::cerr << "Failed to initialize Main Interface" << std::endl;
            return false;
        }
        std::cout << "MainInterface created successfully" << std::endl;

        m_initialized = true;
        std::cout << "Smart Screen Capture System initialization completed successfully!" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception during initialization: " << e.what() << std::endl;
        return false;
    }
}

void PerformanceMonitor::Run() {
    if (!m_initialized) {
        std::cerr << "Application not initialized!" << std::endl;
        return;
    }

    std::cout << "Starting Smart Screen Capture System..." << std::endl;
    std::cout << "Press Ctrl+C or close the window to exit." << std::endl;

    while (m_running) {
        HandleEvents();
        Update();
        Render();
        
        // Control frame rate
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
}

void PerformanceMonitor::Cleanup() {
    std::cout << "Cleaning up Smart Screen Capture System..." << std::endl;
    
    m_mainInterface.reset();
    m_objectDetector.reset();
    m_colorDetector.reset();
    m_screenCapture.reset();
    m_configManager.reset();
    
    std::cout << "Smart Screen Capture System cleanup completed" << std::endl;
}

bool PerformanceMonitor::SetCaptureMonitor(int monitorIndex) {
    if (!m_screenCapture) {
        return false;
    }
    
    auto monitors = m_screenCapture->GetAvailableMonitors();
    if (monitorIndex < 0 || monitorIndex >= static_cast<int>(monitors.size())) {
        return false;
    }
    
    return m_screenCapture->SetTargetMonitor(monitorIndex);
}

int PerformanceMonitor::GetCurrentMonitorIndex() const {
    return m_screenCapture ? m_screenCapture->GetCurrentMonitorIndex() : 0;
}

std::vector<HighSpeedCapture::MonitorInfo> PerformanceMonitor::GetAvailableMonitors() const {
    return m_screenCapture ? m_screenCapture->GetAvailableMonitors() : std::vector<HighSpeedCapture::MonitorInfo>();
}

void PerformanceMonitor::HandleEvents() {
    // Handle main interface events
    if (m_mainInterface) {
        m_mainInterface->HandleEvents();
    }
}

void PerformanceMonitor::Update() {
    UpdateFPS();
    UpdateDetection();
    UpdateSettings();
}

void PerformanceMonitor::Render() {
    // Render main interface
    if (m_mainInterface) {
        m_mainInterface->Render();
    }
}

void PerformanceMonitor::UpdateDetection() {
    if (!m_screenCapture) {
        return;
    }

    // Capture frame for detection analysis
    if (m_screenCapture->CaptureFrame()) {
        cv::Mat frame = m_screenCapture->GetLatestFrame();
        
        // HSV Color Detection
        if (m_hsvDetectionEnabled && m_colorDetector) {
            auto detectionResult = m_colorDetector->detectTarget(frame);
            // Process detection result
        }
        
        // YOLO Object Detection
        if (m_yoloDetectionEnabled && m_objectDetector) {
            auto objectResults = m_objectDetector->detectMultipleTargets(frame);
            // Process object detection results
        }
        
        // Update main interface with new frame
        if (m_mainInterface) {
            // Update interface with frame and detection results
        }
    }
}

void PerformanceMonitor::UpdateSettings() {
    // Update system settings if needed
    if (m_configManager) {
        // Sync configuration settings
        // Update detection flags based on config
    }
}

void PerformanceMonitor::UpdateFPS() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto deltaTime = std::chrono::duration<double>(currentTime - m_lastUpdateTime).count();
    m_lastUpdateTime = currentTime;
    
    m_frameTimeAccumulator += deltaTime;
    m_frameCount++;
    
    if (m_frameTimeAccumulator >= 1.0) {
        m_currentFPS = static_cast<float>(m_frameCount) / static_cast<float>(m_frameTimeAccumulator);
        m_frameTimeAccumulator = 0.0;
        m_frameCount = 0;
    }
}

double PerformanceMonitor::GetApplicationTimeInSeconds() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double>(duration).count();
}