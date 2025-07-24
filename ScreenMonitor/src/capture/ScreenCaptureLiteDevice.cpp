// ScreenCaptureLiteDevice implementation
#include "capture/ScreenCaptureLiteDevice.h"
#include <ScreenCapture.h>
#include <nlohmann/json.hpp>

ScreenCaptureLiteDevice::ScreenCaptureLiteDevice() 
    : is_capturing_(false), current_monitor_index_(-1), is_initialized_(false),
      new_frame_available_(false), total_frames_captured_(0), dropped_frames_(0),
      average_fps_(0.0), average_frame_time_ms_(0.0) {}

ScreenCaptureLiteDevice::~ScreenCaptureLiteDevice() {
    Cleanup();
}

bool ScreenCaptureLiteDevice::Initialize(const CaptureSettings& settings) { 
    current_settings_ = settings;
    is_initialized_ = InitializeCaptureManager();
    return is_initialized_;
}

bool ScreenCaptureLiteDevice::StartCapture(int monitor_index) {
    if (!is_initialized_ || is_capturing_) {
        return false;
    }
    
    current_monitor_index_ = monitor_index;
    is_capturing_ = true;
    start_capture_time_ = std::chrono::high_resolution_clock::now();
    
    // TODO: Implement actual capture start using screen_capture_lite
    return true;
}

void ScreenCaptureLiteDevice::StopCapture() {
    is_capturing_ = false;
    // TODO: Implement capture stop
}

bool ScreenCaptureLiteDevice::CaptureFrame(cv::Mat& output_frame) {
    if (!is_capturing_ || !new_frame_available_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (!latest_frame_.empty()) {
        latest_frame_.copyTo(output_frame);
        new_frame_available_ = false;
        return true;
    }
    return false;
}

std::vector<ICaptureDevice::MonitorInfo> ScreenCaptureLiteDevice::GetAvailableMonitors() const {
    std::vector<ICaptureDevice::MonitorInfo> monitors;
    
    // TODO: Implement monitor enumeration using screen_capture_lite
    // For now, return a dummy monitor
    ICaptureDevice::MonitorInfo dummy_monitor;
    dummy_monitor.index = 0;
    dummy_monitor.name = "Primary Monitor";
    dummy_monitor.width = 1920;
    dummy_monitor.height = 1080;
    dummy_monitor.x = 0;
    dummy_monitor.y = 0;
    dummy_monitor.is_primary = true;
    monitors.push_back(dummy_monitor);
    
    return monitors;
}

bool ScreenCaptureLiteDevice::IsCapturing() const {
    return is_capturing_;
}

int ScreenCaptureLiteDevice::GetCurrentMonitorIndex() const {
    return current_monitor_index_;
}

bool ScreenCaptureLiteDevice::UpdateSettings(const CaptureSettings& settings) {
    current_settings_ = settings;
    // TODO: Apply settings to capture manager
    return true;
}

std::string ScreenCaptureLiteDevice::GetPerformanceStats() const {
    UpdatePerformanceMetrics();
    
    nlohmann::json stats;
    stats["fps"] = average_fps_.load();
    stats["frame_time_ms"] = average_frame_time_ms_.load();
    stats["total_frames"] = total_frames_captured_.load();
    stats["dropped_frames"] = dropped_frames_.load();
    stats["is_capturing"] = is_capturing_.load();
    
    return stats.dump(2);
}

void ScreenCaptureLiteDevice::Cleanup() {
    StopCapture();
    capture_manager_.reset();
    is_initialized_ = false;
}

void ScreenCaptureLiteDevice::RegisterPerformanceObserver(std::shared_ptr<IPerformanceObserver> observer) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    performance_observers_.push_back(observer);
}

void ScreenCaptureLiteDevice::UnregisterPerformanceObserver(std::shared_ptr<IPerformanceObserver> observer) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    performance_observers_.erase(
        std::remove_if(performance_observers_.begin(), performance_observers_.end(),
            [&observer](const std::weak_ptr<IPerformanceObserver>& weak_obs) {
                return weak_obs.expired() || weak_obs.lock() == observer;
            }),
        performance_observers_.end());
}

ICaptureDevice::MonitorInfo ScreenCaptureLiteDevice::ConvertMonitorInfo(const SL::Screen_Capture::Monitor& sl_monitor, int index) const {
    ICaptureDevice::MonitorInfo info;
    info.index = index;
    info.name = "Monitor " + std::to_string(index);
    // TODO: Get actual monitor properties from sl_monitor
    info.width = 1920;
    info.height = 1080;
    info.x = 0;
    info.y = 0;
    info.is_primary = (index == 0);
    return info;
}

void ScreenCaptureLiteDevice::OnFrameChanged(const SL::Screen_Capture::Image& image, const SL::Screen_Capture::Monitor& monitor) {
    // TODO: Convert image to OpenCV Mat and update latest_frame_
    total_frames_captured_++;
    new_frame_available_ = true;
    UpdatePerformanceMetrics();
}

void ScreenCaptureLiteDevice::UpdatePerformanceMetrics() const {
    auto current_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_capture_time_);
    
    if (elapsed.count() > 0) {
        double seconds = elapsed.count() / 1000.0;
        average_fps_ = total_frames_captured_ / seconds;
        
        if (total_frames_captured_ > 0) {
            average_frame_time_ms_ = seconds * 1000.0 / total_frames_captured_;
        }
    }
    
    last_capture_time_ = current_time;
}

void ScreenCaptureLiteDevice::NotifyPerformanceObservers(const IPerformanceObserver::PerformanceMetrics& metrics) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    for (auto it = performance_observers_.begin(); it != performance_observers_.end();) {
        if (auto observer = it->lock()) {
            observer->OnPerformanceUpdated(metrics);
            ++it;
        } else {
            it = performance_observers_.erase(it);
        }
    }
}

cv::Mat ScreenCaptureLiteDevice::ConvertToMat(const SL::Screen_Capture::Image& image) const {
    // TODO: Implement image conversion from screen_capture_lite to OpenCV
    return cv::Mat();
}

bool ScreenCaptureLiteDevice::InitializeCaptureManager() {
    try {
        // TODO: Initialize screen_capture_lite capture manager
        RefreshMonitorList();
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

void ScreenCaptureLiteDevice::RefreshMonitorList() {
    // TODO: Get monitor list from screen_capture_lite and store in implementation-specific way
}