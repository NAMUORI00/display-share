#pragma once

#include "../interfaces/ICaptureDevice.h"
#include "../interfaces/IPerformanceObserver.h"
#include "CenterRegionCapture.h"
#include <memory>
#include <chrono>
#include <atomic>
#include <vector>

// Forward declaration for screen_capture_lite
namespace SL {
    namespace Screen_Capture {
        class IScreenCaptureManager;
        struct Monitor;
        struct Image;
    }
}

/**
 * @brief screen_capture_lite 기반 캡처 디바이스 구현
 * 
 * 고성능 크로스 플랫폼 화면 캡처를 제공합니다.
 */
class ScreenCaptureLiteDevice : public ICaptureDevice {
public:
    ScreenCaptureLiteDevice();
    virtual ~ScreenCaptureLiteDevice();

    // ICaptureDevice 인터페이스 구현
    bool Initialize(const CaptureSettings& settings) override;
    bool StartCapture(int monitor_index) override;
    void StopCapture() override;
    bool CaptureFrame(cv::Mat& output_frame) override;
    std::vector<ICaptureDevice::MonitorInfo> GetAvailableMonitors() const override;
    bool IsCapturing() const override;
    int GetCurrentMonitorIndex() const override;
    bool UpdateSettings(const CaptureSettings& settings) override;
    std::string GetPerformanceStats() const override;
    void Cleanup() override;

    /**
     * @brief 성능 관찰자 등록
     * @param observer 성능 관찰자
     */
    void RegisterPerformanceObserver(std::shared_ptr<IPerformanceObserver> observer);

    /**
     * @brief 성능 관찰자 해제
     * @param observer 성능 관찰자
     */
    void UnregisterPerformanceObserver(std::shared_ptr<IPerformanceObserver> observer);

    /**
     * @brief 320x320 중심 영역 캡처 모드 활성화/비활성화
     * @param enabled 활성화 여부
     */
    void SetCenterRegionMode(bool enabled);

    /**
     * @brief 320x320 중심 영역 캡처 모드 상태 조회
     * @return 활성화 여부
     */
    bool IsCenterRegionModeEnabled() const;

    /**
     * @brief 최신 320x320 중심 영역 프레임 획득
     * @param output_region 320x320 중심 영역 (출력)
     * @return 프레임 획득 성공 여부
     */
    bool GetCenterRegion(cv::Mat& output_region);

    /**
     * @brief 320x320 좌표를 전체 화면 좌표로 변환
     * @param region_x 320x320 영역 내 X 좌표
     * @param region_y 320x320 영역 내 Y 좌표
     * @param screen_x 전체 화면 X 좌표 (출력)
     * @param screen_y 전체 화면 Y 좌표 (출력)
     * @return 변환 성공 여부
     */
    bool TransformCenterRegionToScreen(int region_x, int region_y, int& screen_x, int& screen_y);

    /**
     * @brief 중심 영역 캡처 성능 메트릭 조회
     * @return 성능 메트릭 문자열
     */
    std::string GetCenterRegionPerformanceStats() const;

private:
    // screen_capture_lite 관련
    std::shared_ptr<SL::Screen_Capture::IScreenCaptureManager> capture_manager_;
    // Note: monitors_ vector will be handled in implementation file to avoid incomplete type issues
    
    // 캡처 설정 및 상태
    CaptureSettings current_settings_;
    std::atomic<bool> is_capturing_;
    std::atomic<int> current_monitor_index_;
    std::atomic<bool> is_initialized_;
    
    // 프레임 데이터
    cv::Mat latest_frame_;
    std::mutex frame_mutex_;
    std::atomic<bool> new_frame_available_;
    
    // 성능 메트릭
    mutable std::chrono::high_resolution_clock::time_point last_capture_time_;
    mutable std::chrono::high_resolution_clock::time_point start_capture_time_;
    mutable std::atomic<uint64_t> total_frames_captured_;
    mutable std::atomic<uint64_t> dropped_frames_;
    mutable std::atomic<double> average_fps_;
    mutable std::atomic<double> average_frame_time_ms_;
    
    // 성능 관찰자들
    std::vector<std::weak_ptr<IPerformanceObserver>> performance_observers_;
    mutable std::mutex observers_mutex_;

    // 320x320 중심 영역 캡처 최적화
    std::unique_ptr<CenterRegionCapture> center_region_capture_;
    cv::Mat latest_center_region_;          ///< 최신 320x320 중심 영역
    std::mutex center_region_mutex_;        ///< 중심 영역 접근 동기화
    std::atomic<bool> center_region_enabled_;  ///< 중심 영역 모드 활성화 여부

    /**
     * @brief screen_capture_lite 모니터를 MonitorInfo로 변환
     * @param sl_monitor screen_capture_lite 모니터
     * @param index 모니터 인덱스
     * @return 변환된 MonitorInfo
     */
    ICaptureDevice::MonitorInfo ConvertMonitorInfo(const SL::Screen_Capture::Monitor& sl_monitor, int index) const;

    /**
     * @brief 프레임 캡처 콜백
     * @param image 캡처된 이미지
     * @param monitor 모니터 정보
     */
    void OnFrameChanged(const SL::Screen_Capture::Image& image, const SL::Screen_Capture::Monitor& monitor);

    /**
     * @brief 성능 메트릭 업데이트
     */
    void UpdatePerformanceMetrics() const;

    /**
     * @brief 모든 성능 관찰자에게 통지
     * @param metrics 성능 메트릭
     */
    void NotifyPerformanceObservers(const IPerformanceObserver::PerformanceMetrics& metrics);

    /**
     * @brief screen_capture_lite Image를 OpenCV Mat으로 변환
     * @param image screen_capture_lite 이미지
     * @return OpenCV Mat
     */
    cv::Mat ConvertToMat(const SL::Screen_Capture::Image& image) const;

    /**
     * @brief 캡처 매니저 초기화
     * @return 초기화 성공 여부
     */
    bool InitializeCaptureManager();

    /**
     * @brief 모니터 목록 새로고침
     */
    void RefreshMonitorList();

    /**
     * @brief 320x320 중심 영역 처리
     * @param full_frame 전체 화면 프레임
     */
    void ProcessCenterRegion(const cv::Mat& full_frame);
};