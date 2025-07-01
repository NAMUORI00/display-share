#include "capture/CrossPlatformCapture.h"
#include <iostream>
#include <algorithm>
#include <cstring>

CrossPlatformCapture::CrossPlatformCapture() {
    m_stats.startTime = std::chrono::high_resolution_clock::now();
    m_stats.lastUpdateTime = m_stats.startTime;
    m_lastFrameTime = m_stats.startTime;
}

CrossPlatformCapture::~CrossPlatformCapture() {
    Cleanup();
}

bool CrossPlatformCapture::Initialize(const CaptureSettings& settings) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    
    if (m_initialized) {
        Cleanup();
    }
    
    m_settings = settings;
    
    // Get available monitors
    auto monitors = SL::Screen_Capture::GetMonitors();
    if (monitors.empty()) {
        std::cerr << "No monitors found!" << std::endl;
        return false;
    }
    
    // Convert monitor information
    m_availableMonitors.clear();
    for (size_t i = 0; i < monitors.size(); ++i) {
        m_availableMonitors.push_back(ConvertMonitor(monitors[i], static_cast<int>(i)));
    }
    
    // Validate monitor index
    if (m_settings.selectedMonitorIndex >= static_cast<int>(monitors.size())) {
        std::cerr << "Invalid monitor index: " << m_settings.selectedMonitorIndex << std::endl;
        m_settings.selectedMonitorIndex = 0;
    }
    
    const auto& monitor = monitors[m_settings.selectedMonitorIndex];
    m_currentWidth = monitor.Width;
    m_currentHeight = monitor.Height;
    
    // Create screen capture manager
    auto captureConfig = SL::Screen_Capture::CreateCaptureConfiguration([]() {
        return SL::Screen_Capture::GetMonitors();
    })
    ->onNewFrame([this](const SL::Screen_Capture::Image& img, const SL::Screen_Capture::Monitor& monitor) {
        OnNewFrame(img, monitor);
    })
    ->onMouseChanged([this](const SL::Screen_Capture::Image* img, const SL::Screen_Capture::MousePoint& mousepoint) {
        OnMouseChanged(img, mousepoint.Position);
    });
    
    m_captureManager = captureConfig->start_capturing();
    
    m_initialized = true;
    m_frameCount = 0;
    
    std::cout << "CrossPlatformCapture initialized successfully" << std::endl;
    return true;
}

bool CrossPlatformCapture::StartCapture() {
    if (!m_initialized || !m_captureManager) {
        return false;
    }
    
    m_isCapturing = true;
    return true;
}

void CrossPlatformCapture::StopCapture() {
    m_isCapturing = false;
}

void CrossPlatformCapture::Cleanup() {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    
    StopCapture();
    
    if (m_captureManager) {
        m_captureManager.reset();
    }
    
    m_initialized = false;
    m_latestFrameData.clear();
    m_availableMonitors.clear();
}

bool CrossPlatformCapture::GetLatestFrameData(std::vector<uint8_t>& data, int& width, int& height, int monitorIndex) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    
    if (!m_isCapturing || m_latestFrameData.empty()) {
        return false;
    }
    
    data = m_latestFrameData;
    width = m_currentWidth;
    height = m_currentHeight;
    
    return true;
}

bool CrossPlatformCapture::SetMonitor(int monitorIndex) {
    if (monitorIndex < 0 || monitorIndex >= static_cast<int>(m_availableMonitors.size())) {
        return false;
    }
    
    if (monitorIndex == m_settings.selectedMonitorIndex) {
        return true; // Already on this monitor
    }
    
    m_settings.selectedMonitorIndex = monitorIndex;
    
    // Reinitialize with new monitor
    bool wasCapturing = m_isCapturing;
    
    if (Initialize(m_settings)) {
        if (wasCapturing) {
            StartCapture();
        }
        return true;
    }
    
    return false;
}

void CrossPlatformCapture::UpdateSettings(const CaptureSettings& settings) {
    m_settings = settings;
}

std::vector<CrossPlatformCapture::MonitorInfo> CrossPlatformCapture::GetAvailableMonitors() {
    std::vector<MonitorInfo> result;
    auto monitors = SL::Screen_Capture::GetMonitors();
    
    for (size_t i = 0; i < monitors.size(); ++i) {
        result.push_back(ConvertMonitor(monitors[i], static_cast<int>(i)));
    }
    
    return result;
}

CrossPlatformCapture::PerformanceStats CrossPlatformCapture::GetPerformanceStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

int CrossPlatformCapture::GetCurrentMonitorIndex() const {
    return m_settings.selectedMonitorIndex;
}

void CrossPlatformCapture::OnNewFrame(const SL::Screen_Capture::Image& img, const SL::Screen_Capture::Monitor& monitor) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    
    if (!m_isCapturing) {
        return;
    }
    
    // Update dimensions
    m_currentWidth = monitor.Width;
    m_currentHeight = monitor.Height;
    
    // Calculate frame data size (BGRA format)
    size_t dataSize = m_currentWidth * m_currentHeight * 4;
    
    // Resize buffer if needed
    if (m_latestFrameData.size() != dataSize) {
        m_latestFrameData.resize(dataSize);
    }
    
    // Copy frame data
    const auto* src = SL::Screen_Capture::StartSrc(img);
    if (src) {
        std::memcpy(m_latestFrameData.data(), src, dataSize);
    }
    
    // Update performance stats
    UpdatePerformanceStats();
}

void CrossPlatformCapture::OnMouseChanged(const SL::Screen_Capture::Image* img, const SL::Screen_Capture::Point& point) {
    // Optional: Handle mouse cursor changes
    // This can be used for more advanced capture features
}

void CrossPlatformCapture::UpdatePerformanceStats() {
    auto current_time = std::chrono::high_resolution_clock::now();
    
    m_frameCount++;
    
    // Calculate time difference
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        current_time - m_lastFrameTime);
    double time_diff_sec = duration.count() / 1000000.0;
    
    if (time_diff_sec > 0) {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        
        // Calculate current FPS
        m_stats.currentFPS = 1.0 / time_diff_sec;
        
        // Calculate average FPS
        auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            current_time - m_stats.startTime);
        m_stats.totalCaptureTime = total_duration.count() / 1000000.0;
        
        if (m_stats.totalCaptureTime > 0) {
            m_stats.averageFPS = m_frameCount / m_stats.totalCaptureTime;
        }
        
        m_stats.totalFrames = m_frameCount;
        m_stats.lastUpdateTime = current_time;
    }
    
    m_lastFrameTime = current_time;
}

CrossPlatformCapture::MonitorInfo CrossPlatformCapture::ConvertMonitor(
    const SL::Screen_Capture::Monitor& monitor, int index) {
    MonitorInfo info;
    info.index = index;
    info.name = monitor.Name;
    info.width = monitor.Width;
    info.height = monitor.Height;
    info.offsetX = monitor.OffsetX;
    info.offsetY = monitor.OffsetY;
    return info;
}