// ScreenCaptureLiteDevice implementation
#include "capture/ScreenCaptureLiteDevice.h"
#include <ScreenCapture.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

ScreenCaptureLiteDevice::ScreenCaptureLiteDevice() 
    : is_capturing_(false), current_monitor_index_(-1), is_initialized_(false),
      new_frame_available_(false), total_frames_captured_(0), dropped_frames_(0),
      average_fps_(0.0), average_frame_time_ms_(0.0) {}

ScreenCaptureLiteDevice::~ScreenCaptureLiteDevice() {
    Cleanup();
}

bool ScreenCaptureLiteDevice::Initialize(const CaptureSettings& settings) { 
    std::cout << "[ScreenCapture] 캡처 장치 초기화 시작..." << std::endl;
    current_settings_ = settings;
    is_initialized_ = InitializeCaptureManager();
    
    if (is_initialized_) {
        std::cout << "[ScreenCapture] 캡처 장치 초기화 성공" << std::endl;
    } else {
        std::cerr << "[ScreenCapture] 캡처 장치 초기화 실패" << std::endl;
    }
    
    return is_initialized_;
}

bool ScreenCaptureLiteDevice::StartCapture(int monitor_index) {
    if (!is_initialized_ || is_capturing_) {
        std::cerr << "[ScreenCapture] 캡처 시작 실패: " 
                  << "초기화됨=" << is_initialized_ 
                  << ", 캡처중=" << is_capturing_.load() << std::endl;
        return false;
    }
    
    // 유효한 모니터 인덱스 확인
    auto monitors = GetAvailableMonitors();
    if (monitor_index < 0 || monitor_index >= static_cast<int>(monitors.size())) {
        std::cerr << "[ScreenCapture] 유효하지 않은 모니터 인덱스: " << monitor_index << std::endl;
        return false;
    }
    
    current_monitor_index_ = monitor_index;
    start_capture_time_ = std::chrono::high_resolution_clock::now();
    
    try {
        // screen_capture_lite 캡처 시작
        if (capture_manager_) {
            capture_manager_->setFrameChangeInterval(std::chrono::milliseconds(16)); // ~60 FPS
            capture_manager_->setMouseChangeInterval(std::chrono::milliseconds(100));
            is_capturing_ = true;
            
            std::cout << "[ScreenCapture] 모니터 " << monitor_index 
                      << "에서 캡처 시작 (60 FPS 목표)" << std::endl;
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ScreenCapture] 캡처 시작 중 오류: " << e.what() << std::endl;
    }
    
    return false;
}

void ScreenCaptureLiteDevice::StopCapture() {
    if (!is_capturing_) {
        return;
    }
    
    is_capturing_ = false;
    
    // 캡처 매니저 정지
    if (capture_manager_) {
        capture_manager_.reset();
        std::cout << "[ScreenCapture] Capture stopped" << std::endl;
    }
    
    // 프레임 버퍼 정리
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        latest_frame_.release();
        new_frame_available_ = false;
    }
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
    
    try {
        // screen_capture_lite를 사용한 실제 모니터 열거
        auto sl_monitors = SL::Screen_Capture::GetMonitors();
        
        for (size_t i = 0; i < sl_monitors.size(); ++i) {
            ICaptureDevice::MonitorInfo monitor_info;
            monitor_info.index = static_cast<int>(i);
            monitor_info.name = "Monitor " + std::to_string(i + 1);
            monitor_info.width = static_cast<int>(SL::Screen_Capture::Width(sl_monitors[i]));
            monitor_info.height = static_cast<int>(SL::Screen_Capture::Height(sl_monitors[i]));
            monitor_info.x = static_cast<int>(SL::Screen_Capture::OffsetX(sl_monitors[i]));
            monitor_info.y = static_cast<int>(SL::Screen_Capture::OffsetY(sl_monitors[i]));
            monitor_info.is_primary = (i == 0); // 첫 번째 모니터를 주 모니터로 간주
            
            monitors.push_back(monitor_info);
        }
        
        std::cout << "[ScreenCapture] " << monitors.size() << "개의 모니터 발견" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[ScreenCapture] 모니터 열거 중 오류: " << e.what() << std::endl;
        
        // 폴백: 기본 모니터 정보 제공
        ICaptureDevice::MonitorInfo fallback_monitor;
        fallback_monitor.index = 0;
        fallback_monitor.name = "Primary Monitor (Fallback)";
        fallback_monitor.width = 1920;
        fallback_monitor.height = 1080;
        fallback_monitor.x = 0;
        fallback_monitor.y = 0;
        fallback_monitor.is_primary = true;
        monitors.push_back(fallback_monitor);
    }
    
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
    info.name = "Monitor " + std::to_string(index + 1);
    info.width = static_cast<int>(SL::Screen_Capture::Width(sl_monitor));
    info.height = static_cast<int>(SL::Screen_Capture::Height(sl_monitor));
    info.x = static_cast<int>(SL::Screen_Capture::OffsetX(sl_monitor));
    info.y = static_cast<int>(SL::Screen_Capture::OffsetY(sl_monitor));
    info.is_primary = (index == 0);
    return info;
}

void ScreenCaptureLiteDevice::OnFrameChanged(const SL::Screen_Capture::Image& image, const SL::Screen_Capture::Monitor& monitor) {
    if (!is_capturing_) {
        return;
    }
    
    try {
        // screen_capture_lite 이미지를 OpenCV Mat으로 변환
        cv::Mat converted_frame = ConvertToMat(image);
        
        if (!converted_frame.empty()) {
            std::lock_guard<std::mutex> lock(frame_mutex_);
            converted_frame.copyTo(latest_frame_);
            new_frame_available_ = true;
            
            // 프레임 카운터 증가 및 성능 메트릭 업데이트
            total_frames_captured_++;
            UpdatePerformanceMetrics();
            
            // 성능 관찰자들에게 통지
            IPerformanceObserver::PerformanceMetrics metrics;
            metrics.fps = average_fps_.load();
            metrics.frame_time_ms = average_frame_time_ms_.load();
            metrics.total_frames = total_frames_captured_.load();
            metrics.dropped_frames = dropped_frames_.load();
            
            NotifyPerformanceObservers(metrics);
        } else {
            dropped_frames_++;
            std::cerr << "[ScreenCapture] 프레임 변환 실패, 드롭된 프레임: " << dropped_frames_.load() << std::endl;
        }
        
    } catch (const std::exception& e) {
        dropped_frames_++;
        std::cerr << "[ScreenCapture] 프레임 처리 중 오류: " << e.what() << std::endl;
    }
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
    try {
        // screen_capture_lite 이미지 속성 가져오기
        int width = static_cast<int>(SL::Screen_Capture::Width(image));
        int height = static_cast<int>(SL::Screen_Capture::Height(image));
        int row_stride = image.RowStrideInBytes;
        
        // 이미지 데이터 검증
        if (width <= 0 || height <= 0 || row_stride <= 0) {
            std::cerr << "[ScreenCapture] 유효하지 않은 이미지 크기: " << width << "x" << height << ", stride: " << row_stride << std::endl;
            return cv::Mat();
        }
        
        // screen_capture_lite는 BGRA 형식 사용 (4바이트 per pixel)
        const SL::Screen_Capture::ImageBGRA* bgra_data = SL::Screen_Capture::StartSrc(image);
        if (!bgra_data) {
            std::cerr << "[ScreenCapture] Image data is null" << std::endl;
            return cv::Mat();
        }
        
        // OpenCV Mat 생성 (BGRA → BGR 변환)
        cv::Mat bgra_image(height, width, CV_8UC4, const_cast<SL::Screen_Capture::ImageBGRA*>(bgra_data), row_stride);
        cv::Mat bgr_image;
        
        // BGRA에서 BGR로 변환 (알파 채널 제거)
        cv::cvtColor(bgra_image, bgr_image, cv::COLOR_BGRA2BGR);
        
        // 메모리 복사본 생성 (원본 데이터 수명 보장)
        cv::Mat result;
        bgr_image.copyTo(result);
        
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "[ScreenCapture] 이미지 변환 중 오류: " << e.what() << std::endl;
        return cv::Mat();
    }
}

bool ScreenCaptureLiteDevice::InitializeCaptureManager() {
    try {
        std::cout << "[ScreenCapture] 캡처 매니저 초기화 중..." << std::endl;
        
        // 사용 가능한 모니터 확인
        RefreshMonitorList();
        auto monitors = GetAvailableMonitors();
        
        if (monitors.empty()) {
            std::cerr << "[ScreenCapture] 사용 가능한 모니터가 없습니다" << std::endl;
            return false;
        }
        
        // screen_capture_lite 캡처 매니저 생성
        auto config = SL::Screen_Capture::CreateCaptureConfiguration([]() {
            return SL::Screen_Capture::GetMonitors();
        });
        
        // 프레임 변경 콜백 설정
        config->onFrameChanged([this](const SL::Screen_Capture::Image& image, 
                                     const SL::Screen_Capture::Monitor& monitor) {
            this->OnFrameChanged(image, monitor);
        });
        
        // 오류 콜백 설정
        config->onNewFrame([this](const SL::Screen_Capture::Image& image, 
                                 const SL::Screen_Capture::Monitor& monitor) {
            // 새 프레임이 도착했을 때의 추가 처리
            // 현재는 onFrameChanged와 동일한 처리
            this->OnFrameChanged(image, monitor);
        });
        
        // 캡처 시작
        capture_manager_ = config->start_capturing();
        
        if (capture_manager_) {
            std::cout << "[ScreenCapture] Capture manager initialized. " << monitors.size() << " monitors available" << std::endl;
            return true;
        } else {
            std::cerr << "[ScreenCapture] 캡처 매니저 생성 실패" << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[ScreenCapture] 캡처 매니저 초기화 중 오류: " << e.what() << std::endl;
        return false;
    }
}

void ScreenCaptureLiteDevice::RefreshMonitorList() {
    try {
        // screen_capture_lite에서 모니터 목록 새로고침
        auto sl_monitors = SL::Screen_Capture::GetMonitors();
        
        std::cout << "[ScreenCapture] 모니터 목록 새로고침: " << sl_monitors.size() << "개 모니터 발견" << std::endl;
                  
        // 각 모니터 정보 로그 출력
        for (size_t i = 0; i < sl_monitors.size(); ++i) {
            int width = static_cast<int>(SL::Screen_Capture::Width(sl_monitors[i]));
            int height = static_cast<int>(SL::Screen_Capture::Height(sl_monitors[i]));
            int x = static_cast<int>(SL::Screen_Capture::OffsetX(sl_monitors[i]));
            int y = static_cast<int>(SL::Screen_Capture::OffsetY(sl_monitors[i]));
            
            std::cout << "[ScreenCapture] 모니터 " << (i + 1) << ": " << width << "x" << height << " at (" << x << ", " << y << ")" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[ScreenCapture] 모니터 목록 새로고침 중 오류: " << e.what() << std::endl;
    }
}