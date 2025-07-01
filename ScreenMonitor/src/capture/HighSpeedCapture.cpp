#include "capture/HighSpeedCapture.h"
#include <iostream>
#include <algorithm>
#include <cstring>

HighSpeedCapture::HighSpeedCapture() 
    : m_screenCapture(std::make_unique<CrossPlatformCapture>()) {
    m_stats.startTime = std::chrono::high_resolution_clock::now();
    m_stats.lastUpdateTime = m_stats.startTime;
    m_lastCaptureTime = m_stats.startTime;
}

HighSpeedCapture::~HighSpeedCapture() {
    Cleanup();
}

bool HighSpeedCapture::Initialize() {
    CaptureSettings defaultSettings;
    return Initialize(defaultSettings);
}

bool HighSpeedCapture::Initialize(const CaptureSettings& settings) {
    std::lock_guard<std::mutex> lock(m_captureMutex);
    
    if (m_initialized) {
        Cleanup();
    }
    
    m_settings = settings;
    
    // Convert settings to CrossPlatformCapture format
    CrossPlatformCapture::CaptureSettings cpcSettings;
    cpcSettings.enableFPSLimiting = settings.enableFPSLimiting;
    cpcSettings.targetFPS = static_cast<int>(settings.targetFPS);
    cpcSettings.selectedMonitorIndex = settings.selectedMonitorIndex;
    cpcSettings.frameChangeInterval = static_cast<int>(settings.timeout);
    
    std::cout << "DEBUG: Initializing CrossPlatformCapture..." << std::endl;
    
    if (!m_screenCapture->Initialize(cpcSettings)) {
        throw CaptureException("Failed to initialize CrossPlatformCapture");
    }
    
    // Start capturing
    if (!m_screenCapture->StartCapture()) {
        throw CaptureException("Failed to start capture");
    }
    
    // Update monitor information
    m_availableMonitors.clear();
    auto monitors = CrossPlatformCapture::GetAvailableMonitors();
    for (size_t i = 0; i < monitors.size(); ++i) {
        m_availableMonitors.push_back(ConvertMonitorInfo(monitors[i], static_cast<int>(i)));
    }
    
    if (settings.selectedMonitorIndex < m_availableMonitors.size()) {
        m_currentMonitor = m_availableMonitors[settings.selectedMonitorIndex];
        m_width = m_currentMonitor.width;
        m_height = m_currentMonitor.height;
    }
    
    m_initialized = true;
    ResetPerformanceStats();
    
    std::cout << "DEBUG: HighSpeedCapture initialized successfully" << std::endl;
    
    return true;
}

void HighSpeedCapture::Cleanup() {
    std::lock_guard<std::mutex> lock(m_captureMutex);
    
    if (m_screenCapture) {
        m_screenCapture->StopCapture();
        m_screenCapture->Cleanup();
    }
    
    m_initialized = false;
}

bool HighSpeedCapture::CaptureScreen() {
    return CaptureScreenOptimized();
}

bool HighSpeedCapture::CaptureFrame() {
    return CaptureScreenOptimized();
}

bool HighSpeedCapture::CaptureScreenOptimized() {
    return CaptureScreenOptimized(m_settings.selectedMonitorIndex);
}

bool HighSpeedCapture::CaptureScreenOptimized(int monitorIndex) {
    if (!m_initialized) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_captureMutex);
    
    // Apply FPS limiting if enabled
    if (ShouldLimitFPS()) {
        return false;
    }
    
    try {
        // Get the latest frame from CrossPlatformCapture
        int frameWidth, frameHeight;
        if (!m_screenCapture->GetLatestFrameData(m_latestFrameData, frameWidth, frameHeight, monitorIndex)) {
            return false;
        }
        
        // Update dimensions
        m_width = frameWidth;
        m_height = frameHeight;
        
        // Copy to image data buffer (already in BGRA format)
        m_imageData = m_latestFrameData;
        
        // Convert to OpenCV Mat
        if (!m_latestFrameData.empty() && m_width > 0 && m_height > 0) {
            // Create Mat from BGRA data
            cv::Mat bgraMat(m_height, m_width, CV_8UC4, m_latestFrameData.data());
            
            // Convert BGRA to BGR for OpenCV
            cv::cvtColor(bgraMat, m_latestFrame, cv::COLOR_BGRA2BGR);
        }
        
        UpdatePerformanceStats();
        m_lastCaptureTime = std::chrono::high_resolution_clock::now();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Capture error: " << e.what() << std::endl;
        m_stats.retryCount++;
        return false;
    }
}

const std::vector<uint8_t>& HighSpeedCapture::GetImageData() const {
    return m_imageData;
}

cv::Mat HighSpeedCapture::GetLatestFrame() const {
    std::lock_guard<std::mutex> lock(m_captureMutex);
    return m_latestFrame.clone();
}

int HighSpeedCapture::GetWidth() const {
    return m_width;
}

int HighSpeedCapture::GetHeight() const {
    return m_height;
}

unsigned int HighSpeedCapture::GetTextureID() const {
    return m_textureID;
}

bool HighSpeedCapture::IsInitialized() const {
    return m_initialized;
}

const HighSpeedCapture::PerformanceStats& HighSpeedCapture::GetPerformanceStats() const {
    return m_stats;
}

void HighSpeedCapture::ResetPerformanceStats() {
    auto cpcStats = m_screenCapture->GetPerformanceStats();
    m_stats.currentFPS = cpcStats.currentFPS;
    m_stats.averageFPS = cpcStats.averageFPS;
    m_stats.totalFrames = cpcStats.totalFrames;
    m_stats.totalCaptureTime = cpcStats.totalCaptureTime;
    m_stats.retryCount = 0;
    m_stats.errorRecoveryCount = 0;
}

void HighSpeedCapture::UpdateSettings(const CaptureSettings& settings) {
    m_settings = settings;
    
    if (m_screenCapture) {
        CrossPlatformCapture::CaptureSettings cpcSettings;
        cpcSettings.enableFPSLimiting = settings.enableFPSLimiting;
        cpcSettings.targetFPS = static_cast<int>(settings.targetFPS);
        cpcSettings.selectedMonitorIndex = settings.selectedMonitorIndex;
        cpcSettings.frameChangeInterval = static_cast<int>(settings.timeout);
        
        m_screenCapture->UpdateSettings(cpcSettings);
    }
}

const HighSpeedCapture::CaptureSettings& HighSpeedCapture::GetSettings() const {
    return m_settings;
}

std::vector<HighSpeedCapture::MonitorInfo> HighSpeedCapture::GetAvailableMonitors() {
    std::vector<MonitorInfo> result;
    auto monitors = CrossPlatformCapture::GetAvailableMonitors();
    
    for (size_t i = 0; i < monitors.size(); ++i) {
        result.push_back(ConvertMonitorInfo(monitors[i], static_cast<int>(i)));
    }
    
    return result;
}

bool HighSpeedCapture::SetMonitor(int monitorIndex) {
    return SetTargetMonitor(monitorIndex);
}

bool HighSpeedCapture::SetTargetMonitor(int monitorIndex) {
    if (!m_screenCapture) {
        return false;
    }
    
    if (m_screenCapture->SetMonitor(monitorIndex)) {
        m_settings.selectedMonitorIndex = monitorIndex;
        if (monitorIndex < m_availableMonitors.size()) {
            m_currentMonitor = m_availableMonitors[monitorIndex];
            m_width = m_currentMonitor.width;
            m_height = m_currentMonitor.height;
        }
        return true;
    }
    
    return false;
}

int HighSpeedCapture::GetCurrentMonitorIndex() const {
    return m_settings.selectedMonitorIndex;
}

int HighSpeedCapture::GetCurrentMonitor() const {
    return m_settings.selectedMonitorIndex;
}

const HighSpeedCapture::MonitorInfo& HighSpeedCapture::GetCurrentMonitorInfo() const {
    return m_currentMonitor;
}

HighSpeedCapture::MonitorInfo HighSpeedCapture::ConvertMonitorInfo(
    const CrossPlatformCapture::MonitorInfo& info, int index) {
    MonitorInfo result;
    result.index = index;
    result.name = info.name;
    result.deviceName = info.name; // Use same name for device name
    result.width = info.width;
    result.height = info.height;
    result.left = info.offsetX;
    result.top = info.offsetY;
    result.isPrimary = (index == 0); // Assume first monitor is primary
    result.hMonitor = nullptr;
    result.adapter = nullptr;
    result.output = nullptr;
    return result;
}

void HighSpeedCapture::UpdatePerformanceStats() {
    auto cpcStats = m_screenCapture->GetPerformanceStats();
    m_stats.currentFPS = cpcStats.currentFPS;
    m_stats.averageFPS = cpcStats.averageFPS;
    m_stats.totalFrames = cpcStats.totalFrames;
    m_stats.totalCaptureTime = cpcStats.totalCaptureTime;
    m_stats.lastUpdateTime = std::chrono::high_resolution_clock::now();
}

bool HighSpeedCapture::ShouldLimitFPS() {
    if (!m_settings.enableFPSLimiting) {
        return false;
    }
    
    auto now = std::chrono::high_resolution_clock::now();
    auto timeSinceLastCapture = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastCaptureTime).count();
    
    double targetFrameTime = 1000.0 / m_settings.targetFPS;
    return timeSinceLastCapture < targetFrameTime;
}