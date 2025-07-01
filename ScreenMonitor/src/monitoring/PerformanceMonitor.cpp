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
    std::cout << "GUI window opened. Close the window to exit." << std::endl;

    // 모니터 목록 GUI에 전달
    if (m_screenCapture && m_mainInterface) {
        auto monitors = m_screenCapture->GetAvailableMonitors();
        std::vector<std::string> monitorNames;
        for (const auto& monitor : monitors) {
            monitorNames.push_back(monitor.name);
        }
        m_mainInterface->SetMonitorList(monitorNames);
    }

    while (m_running && !m_mainInterface->ShouldClose()) {
        HandleEvents();
        Update();
        Render();
        
        // Control frame rate - GUI에서 VSync 처리
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    m_running = false;
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
    if (!m_screenCapture || !m_mainInterface) {
        return;
    }

    // GUI 상태 확인
    bool captureEnabled = m_mainInterface->IsCaptureEnabled();
    if (!captureEnabled) {
        return;
    }

    // 모니터 변경 확인
    int selectedMonitor = m_mainInterface->GetSelectedMonitorIndex();
    if (selectedMonitor != m_screenCapture->GetCurrentMonitorIndex()) {
        m_screenCapture->SetTargetMonitor(selectedMonitor);
    }

    // Capture frame for detection analysis
    if (m_screenCapture->CaptureFrame()) {
        cv::Mat frame = m_screenCapture->GetLatestFrame();
        std::vector<cv::Rect> allDetections;
        
        // HSV Color Detection
        if (m_mainInterface->IsHSVDetectionEnabled() && m_colorDetector) {
            // GUI에서 HSV 범위 가져오기
            cv::Scalar lower = m_mainInterface->GetHSVLowerBound();
            cv::Scalar upper = m_mainInterface->GetHSVUpperBound();
            m_colorDetector->setColorRange(lower, upper);
            
            cv::Rect detectionResult = m_colorDetector->detectTarget(frame);
            if (!detectionResult.empty()) {
                allDetections.push_back(detectionResult);
            }
        }
        
        // YOLO Object Detection (레거시)
        if (m_mainInterface->IsYOLODetectionEnabled() && m_objectDetector) {
            auto objectResults = m_objectDetector->detectMultipleTargets(frame);
            for (const auto& obj : objectResults) {
                allDetections.push_back(obj.bbox);
            }
        }
        
        // YOLO v11 Object Detection (권장)
        if (m_mainInterface->IsYOLOv11DetectionEnabled() && m_objectDetector) {
            // YOLO v11 모델 로드 (설정이 변경된 경우)
            std::string modelPath = m_mainInterface->GetYOLOv11ModelPath();
            std::string classNamesPath = m_mainInterface->GetYOLOv11ClassNamesPath();
            float confidence = m_mainInterface->GetYOLOv11Confidence();
            float nmsThreshold = m_mainInterface->GetYOLOv11NMSThreshold();
            int backendIndex = m_mainInterface->GetYOLOv11Backend();
            
            static std::string lastModelPath;
            static float lastConfidence = -1.0f;
            static float lastNMSThreshold = -1.0f;
            static int lastBackend = -1;
            
            // 설정이 변경되었거나 모델이 로드되지 않은 경우 재로드
            bool needReload = !m_objectDetector->isModelLoaded() || 
                            modelPath != lastModelPath ||
                            confidence != lastConfidence ||
                            nmsThreshold != lastNMSThreshold ||
                            backendIndex != lastBackend;
            
            if (needReload) {
#ifdef ONNX_GPU_ONLY
                // GPU 전용 모드: 항상 ONNX Runtime GPU 사용
                YOLOBackend backend = YOLOBackend::ONNX_RUNTIME_GPU;
#else
                // 일반 모드: 사용자 선택에 따라
                YOLOBackend backend = (backendIndex == 1) ? YOLOBackend::ONNX_RUNTIME_GPU : YOLOBackend::OPENCV_DNN;
#endif
                
                if (m_objectDetector->loadYOLOv11Model(modelPath, backend)) {
                    // 클래스 이름 로드 (선택사항)
                    if (!classNamesPath.empty()) {
                        m_objectDetector->loadClassNames(classNamesPath);
                    }
                    
                    // 임계값 설정
                    m_objectDetector->setConfidenceThreshold(confidence);
                    m_objectDetector->setNMSThreshold(nmsThreshold);
                    
                    // 설정 저장
                    lastModelPath = modelPath;
                    lastConfidence = confidence;
                    lastNMSThreshold = nmsThreshold;
                    lastBackend = backendIndex;
                }
            }
            
            // YOLO v11 검출 수행
            if (m_objectDetector->isModelLoaded()) {
                auto yolov11Results = m_objectDetector->detectMultipleTargets(frame);
                for (const auto& obj : yolov11Results) {
                    allDetections.push_back(obj.bbox);
                }
            }
        }
        
        // Update main interface with new frame and detections
        m_mainInterface->UpdateFrame(frame);
        m_mainInterface->UpdateDetections(allDetections);
    }
}

void PerformanceMonitor::UpdateSettings() {
    // Update system settings from GUI
    if (m_configManager && m_mainInterface && m_screenCapture) {
        // FPS 타겟 업데이트
        float targetFPS = m_mainInterface->GetTargetFPS();
        HighSpeedCapture::CaptureSettings settings;
        settings.targetFPS = targetFPS;
        settings.enableFPSLimiting = (targetFPS < 240.0f);
        m_screenCapture->UpdateSettings(settings);
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
        
        // GUI에 FPS 업데이트
        if (m_mainInterface) {
            m_mainInterface->UpdateFPS(m_currentFPS);
        }
    }
}

double PerformanceMonitor::GetApplicationTimeInSeconds() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double>(duration).count();
}