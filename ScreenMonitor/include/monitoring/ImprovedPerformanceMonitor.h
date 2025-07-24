#pragma once

#include "../interfaces/ICaptureDevice.h"
#include "../interfaces/IDetectionAlgorithm.h"
#include "../interfaces/IPerformanceObserver.h"
#include "../core/Constants.h"
#include <memory>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>

// Conditional GUI support
#ifndef DISABLE_GUI
    #define GUI_ENABLED 1
#else
    #define GUI_ENABLED 0
#endif

// Forward declarations
#if GUI_ENABLED
class MainInterface;
#endif
class ConfigManager;

/**
 * @brief 개선된 성능 모니터링 클래스
 * 
 * SOLID 원칙을 준수하고 의존성 주입을 통해 구현된 메인 애플리케이션 클래스입니다.
 * 기존의 God Object 패턴을 해체하고 책임을 분리했습니다.
 */
class ImprovedPerformanceMonitor : public IPerformanceObserver {
public:
    /**
     * @brief 모니터 설정 구조체
     */
    struct MonitorSettings {
        float target_fps = Constants::Performance::DEFAULT_TARGET_FPS;
        bool enable_hsv_detection = true;
        bool enable_yolo_detection = false;
        bool enable_gui = true;
        int monitor_index = Constants::Capture::DEFAULT_MONITOR_INDEX;
        std::string capture_device_type = "screen_capture_lite";
        std::string detection_algorithm_type = "hsv_color_detection";
        
        MonitorSettings() = default;
    };

    /**
     * @brief 애플리케이션 상태 열거형
     */
    enum class ApplicationState {
        UNINITIALIZED,
        INITIALIZING,
        READY,
        RUNNING,
        PAUSED,
        STOPPING,
        ERROR
    };

    ImprovedPerformanceMonitor();
    virtual ~ImprovedPerformanceMonitor();

    /**
     * @brief 시스템 초기화
     * @param settings 모니터 설정
     * @return 초기화 성공 여부
     */
    bool Initialize(const MonitorSettings& settings = MonitorSettings{});

    /**
     * @brief 메인 애플리케이션 루프 실행
     */
    void Run();

    /**
     * @brief 애플리케이션 정리
     */
    void Cleanup();

    /**
     * @brief 캡처 시작
     * @param monitor_index 모니터 인덱스
     * @return 시작 성공 여부
     */
    bool StartCapture(int monitor_index);

    /**
     * @brief 캡처 중지
     */
    void StopCapture();

    /**
     * @brief 현재 애플리케이션 상태
     * @return 애플리케이션 상태
     */
    ApplicationState GetState() const;

    /**
     * @brief 설정 업데이트
     * @param settings 새로운 설정
     * @return 업데이트 성공 여부
     */
    bool UpdateSettings(const MonitorSettings& settings);

    /**
     * @brief 사용 가능한 모니터 목록 조회
     * @return 모니터 정보 목록
     */
    std::vector<ICaptureDevice::MonitorInfo> GetAvailableMonitors() const;

    /**
     * @brief 현재 모니터 인덱스 조회
     * @return 모니터 인덱스
     */
    int GetCurrentMonitorIndex() const;

    /**
     * @brief 성능 통계 조회
     * @return 성능 정보 문자열
     */
    std::string GetPerformanceStats() const;

    // IPerformanceObserver 인터페이스 구현
    void OnFPSUpdated(float fps) override;
    void OnFrameTimeUpdated(double frame_time_ms) override;
    void OnMemoryUsageUpdated(size_t memory_usage_mb) override;
    void OnFrameDropped(int dropped_count) override;
    void OnPerformanceUpdated(const PerformanceMetrics& metrics) override;
    void OnError(const std::string& error_message) override;

    // 복사 생성자 및 대입 연산자 삭제
    ImprovedPerformanceMonitor(const ImprovedPerformanceMonitor&) = delete;
    ImprovedPerformanceMonitor& operator=(const ImprovedPerformanceMonitor&) = delete;

private:
    // 핵심 컴포넌트들 (의존성 주입)
    std::unique_ptr<ICaptureDevice> capture_device_;
    std::unique_ptr<IDetectionAlgorithm> detection_algorithm_;
    std::unique_ptr<ConfigManager> config_manager_;

#if GUI_ENABLED
    std::unique_ptr<MainInterface> main_interface_;
#endif

    // 애플리케이션 상태
    std::atomic<ApplicationState> current_state_;
    MonitorSettings current_settings_;
    mutable std::mutex settings_mutex_;

    // 실행 제어
    std::atomic<bool> should_run_;
    std::atomic<bool> is_capturing_;
    std::atomic<bool> is_initialized_;

    // 성능 메트릭
    std::atomic<float> current_fps_;
    std::atomic<double> current_frame_time_ms_;
    std::atomic<size_t> current_memory_usage_mb_;
    std::atomic<int> total_dropped_frames_;
    
    // 타이밍 관련
    std::chrono::high_resolution_clock::time_point last_update_time_;
    std::chrono::high_resolution_clock::time_point last_render_time_;
    std::chrono::high_resolution_clock::time_point application_start_time_;
    
    // FPS 제어
    double frame_accumulator_;
    int frame_count_;
    std::chrono::high_resolution_clock::time_point last_fps_update_;

    // 스레드 안전성
    mutable std::mutex performance_mutex_;
    mutable std::mutex frame_mutex_;

    /**
     * @brief 컴포넌트 팩토리를 통한 의존성 주입
     * @return 주입 성공 여부
     */
    bool InjectDependencies();

    /**
     * @brief 설정 로드 및 검증
     * @return 로드 성공 여부
     */
    bool LoadConfiguration();

    /**
     * @brief GUI 초기화 (조건부 컴파일)
     * @return 초기화 성공 여부
     */
    bool InitializeGUI();

    /**
     * @brief 메인 업데이트 루프
     */
    void UpdateLoop();

    /**
     * @brief 이벤트 처리
     */
    void HandleEvents();

    /**
     * @brief 렌더링 (GUI 포함)
     */
    void Render();

    /**
     * @brief 감지 알고리즘 처리
     */
    void ProcessDetection();

    /**
     * @brief FPS 제어 및 타이밍 업데이트
     */
    void UpdateTiming();

    /**
     * @brief 메모리 사용량 모니터링
     */
    void MonitorMemoryUsage();

    /**
     * @brief 시스템 상태 변경
     * @param new_state 새로운 상태
     */
    void ChangeState(ApplicationState new_state);

    /**
     * @brief 애플리케이션 시작 시간 반환
     * @return 시작 시간 (초)
     */
    double GetApplicationTimeInSeconds() const;

    /**
     * @brief 오류 처리
     * @param error_message 오류 메시지
     * @param error_code 오류 코드
     */
    void HandleError(const std::string& error_message, int error_code = Constants::Errors::GENERIC_ERROR);

    /**
     * @brief 상태별 업데이트 처리
     */
    void UpdateBasedOnState();

    /**
     * @brief 리소스 정리
     */
    void CleanupResources();

    /**
     * @brief 설정 유효성 검사
     * @param settings 검사할 설정
     * @return 유효성 여부
     */
    bool ValidateSettings(const MonitorSettings& settings) const;
};