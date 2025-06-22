#pragma once

#include <ScreenCapture.h>
#include <memory>
#include <vector>
#include <mutex>
#include <chrono>
#include <string>

/**
 * @brief 크로스 플랫폼 화면 캡처 래퍼
 * 
 * screen_capture_lite 라이브러리를 래핑하여 크로스 플랫폼 지원
 */
class CrossPlatformCapture {
public:
    /**
     * @brief 모니터 정보 구조체
     */
    struct MonitorInfo {
        int index = 0;
        std::string name;
        int width = 0;
        int height = 0;
        int offsetX = 0;
        int offsetY = 0;
    };

    /**
     * @brief 성능 통계 구조체
     */
    struct PerformanceStats {
        double currentFPS = 0.0;
        double averageFPS = 0.0;
        int totalFrames = 0;
        double totalCaptureTime = 0.0;
        std::chrono::high_resolution_clock::time_point startTime;
        std::chrono::high_resolution_clock::time_point lastUpdateTime;
    };

    /**
     * @brief 캡처 설정 구조체
     */
    struct CaptureSettings {
        bool enableFPSLimiting = false;
        int targetFPS = 60;
        int selectedMonitorIndex = 0;
        int frameChangeInterval = 16; // ms
    };

private:
    std::shared_ptr<SL::Screen_Capture::IScreenCaptureManager> m_captureManager;
    std::vector<uint8_t> m_latestFrameData;
    std::vector<MonitorInfo> m_availableMonitors;
    
    mutable std::mutex m_frameMutex;
    mutable std::mutex m_statsMutex;
    
    PerformanceStats m_stats;
    CaptureSettings m_settings;
    
    int m_currentWidth = 0;
    int m_currentHeight = 0;
    bool m_isCapturing = false;
    bool m_initialized = false;
    
    // 성능 측정
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    int m_frameCount = 0;

public:
    /**
     * @brief CrossPlatformCapture 생성자
     */
    CrossPlatformCapture();
    
    /**
     * @brief 소멸자
     */
    ~CrossPlatformCapture();

    /**
     * @brief 캡처 시스템 초기화
     * @param settings 캡처 설정
     * @return 초기화 성공 여부
     */
    bool Initialize(const CaptureSettings& settings);
    
    /**
     * @brief 캡처 시작
     * @return 시작 성공 여부
     */
    bool StartCapture();
    
    /**
     * @brief 캡처 중지
     */
    void StopCapture();
    
    /**
     * @brief 캡처 시스템 정리
     */
    void Cleanup();

    /**
     * @brief 최신 프레임 데이터 획득
     * @param data 출력될 프레임 데이터
     * @param width 출력될 이미지 너비
     * @param height 출력될 이미지 높이
     * @param monitorIndex 모니터 인덱스 (기본값: 현재 선택된 모니터)
     * @return 데이터 획득 성공 여부
     */
    bool GetLatestFrameData(std::vector<uint8_t>& data, int& width, int& height, int monitorIndex = -1);
    
    /**
     * @brief 특정 모니터로 전환
     * @param monitorIndex 모니터 인덱스
     * @return 전환 성공 여부
     */
    bool SetMonitor(int monitorIndex);
    
    /**
     * @brief 설정 업데이트
     * @param settings 새로운 설정
     */
    void UpdateSettings(const CaptureSettings& settings);

    /**
     * @brief 사용 가능한 모니터 목록 반환
     * @return 모니터 정보 목록
     */
    static std::vector<MonitorInfo> GetAvailableMonitors();
    
    /**
     * @brief 성능 통계 반환
     * @return 성능 통계
     */
    PerformanceStats GetPerformanceStats() const;
    
    /**
     * @brief 현재 캡처 중인 모니터 인덱스 반환
     * @return 모니터 인덱스
     */
    int GetCurrentMonitorIndex() const;
    
    /**
     * @brief 초기화 상태 확인
     * @return 초기화 완료 여부
     */
    bool IsInitialized() const { return m_initialized; }
    
    /**
     * @brief 캡처 진행 상태 확인
     * @return 캡처 중 여부
     */
    bool IsCapturing() const { return m_isCapturing; }

private:
    /**
     * @brief 프레임 콜백 함수
     * @param img 캡처된 이미지
     * @param monitor 모니터 정보
     */
    void OnNewFrame(const SL::Screen_Capture::Image& img, const SL::Screen_Capture::Monitor& monitor);
    
    /**
     * @brief 마우스 변경 콜백 함수
     * @param img 마우스 이미지
     * @param point 마우스 위치
     */
    void OnMouseChanged(const SL::Screen_Capture::Image* img, const SL::Screen_Capture::Point& point);
    
    /**
     * @brief 성능 통계 업데이트
     */
    void UpdatePerformanceStats();
    
    /**
     * @brief screen_capture_lite 모니터 정보를 내부 구조체로 변환
     * @param monitor screen_capture_lite 모니터 정보
     * @param index 모니터 인덱스
     * @return 변환된 모니터 정보
     */
    static MonitorInfo ConvertMonitor(const SL::Screen_Capture::Monitor& monitor, int index);
};