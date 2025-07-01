#pragma once

#include <memory>
#include <chrono>
#include <string>
#include <vector>
#include "../capture/HighSpeedCapture.h"

// Forward declarations  
class MainInterface;
class ConfigManager;
class ColorDetector;
class ObjectDetector;

class PerformanceMonitor {
public:
    PerformanceMonitor();
    ~PerformanceMonitor();

    bool Initialize();
    void Run();
    void Cleanup();
    
    // Monitor management methods
    bool SetCaptureMonitor(int monitorIndex);
    int GetCurrentMonitorIndex() const;
    std::vector<HighSpeedCapture::MonitorInfo> GetAvailableMonitors() const;

private:
    // Core update methods
    void HandleEvents();
    void Update();
    void Render();
    
    // Component-specific updates
    void UpdateDetection();
    void UpdateSettings();
    
    // FPS and timing
    void UpdateFPS();
    double GetApplicationTimeInSeconds() const;
    
    // Core components
    std::unique_ptr<MainInterface> m_mainInterface;
    std::unique_ptr<HighSpeedCapture> m_screenCapture;
    std::unique_ptr<ConfigManager> m_configManager;
    
    // Detection processing
    std::unique_ptr<ColorDetector> m_colorDetector;
    std::unique_ptr<ObjectDetector> m_objectDetector;
    
    // Application state
    bool m_running = true;
    bool m_initialized = false;
    
    // Detection flags
    bool m_hsvDetectionEnabled = true;
    bool m_yoloDetectionEnabled = false;
    
    // Performance metrics
    float m_targetFPS = 60.0f;
    double m_lastFrameTime = 0.0;
    double m_frameTimeAccumulator = 0.0;
    int m_frameCount = 0;
    float m_currentFPS = 0.0f;
    
    // Timing control
    std::chrono::high_resolution_clock::time_point m_lastUpdateTime;
    const double m_fixedTimeStep = 1.0 / 60.0; // 60 FPS target
};
