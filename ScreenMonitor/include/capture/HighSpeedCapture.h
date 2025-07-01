#pragma once

#include "capture/CrossPlatformCapture.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <memory>
#include <vector>
#include <chrono>
#include <mutex>
#include <string>

/**
 * @brief 고성능 화면 캡처 엔진
 * 
 * screen_capture_lite 기반 크로스 플랫폼 고속 화면 캡처 시스템
 */
class HighSpeedCapture {
public:
    /**
     * @brief 모니터 정보 구조체
     */
    struct MonitorInfo {
        int index = 0;
        std::string name;
        std::string deviceName;
        int width = 0;
        int height = 0;
        int left = 0;
        int top = 0;
        bool isPrimary = false;
        void* hMonitor = nullptr;
        void* adapter = nullptr;
        void* output = nullptr;
    };

    /**
     * @brief 성능 통계 구조체
     */
    struct PerformanceStats {
        double currentFPS = 0.0;
        double averageFPS = 0.0;
        int totalFrames = 0;
        double totalCaptureTime = 0.0;
        double lastFrameTime = 0.0;
        std::chrono::high_resolution_clock::time_point startTime;
        std::chrono::high_resolution_clock::time_point lastUpdateTime;
        int retryCount = 0;
        int errorRecoveryCount = 0;
    };

    /**
     * @brief 캡처 설정 구조체
     */
    struct CaptureSettings {
        bool enableFPSLimiting = false;
        double targetFPS = 60.0;
        bool enableGPUOptimization = true;
        bool enableMemoryPooling = true;
        int memoryPoolSize = 5;
        int selectedMonitorIndex = 0;
        bool enableDirtyRectOptimization = true;
        unsigned int timeout = 16; // ms for frame capture
    };

    /**
     * @brief 캡처 예외 클래스
     */
    class CaptureException : public std::exception {
    private:
        std::string message_;
    public:
        explicit CaptureException(const std::string& msg) : message_(msg) {}
        const char* what() const noexcept override { return message_.c_str(); }
    };

private:
    std::unique_ptr<CrossPlatformCapture> m_screenCapture;
    cv::Mat m_latestFrame;
    std::vector<uint8_t> m_latestFrameData;
    std::vector<uint8_t> m_imageData;
    
    mutable std::mutex m_captureMutex;
    
    CaptureSettings m_settings;
    PerformanceStats m_stats;
    std::vector<MonitorInfo> m_availableMonitors;
    MonitorInfo m_currentMonitor;
    
    int m_width = 0;
    int m_height = 0;
    unsigned int m_textureID = 0;
    bool m_initialized = false;
    
    std::chrono::high_resolution_clock::time_point m_lastCaptureTime;

public:
    /**
     * @brief HighSpeedCapture 생성자
     */
    HighSpeedCapture();
    
    /**
     * @brief 소멸자
     */
    ~HighSpeedCapture();

    /**
     * @brief 기본 설정으로 초기화
     * @return 초기화 성공 여부
     */
    bool Initialize();
    
    /**
     * @brief 사용자 설정으로 초기화
     * @param settings 캡처 설정
     * @return 초기화 성공 여부
     */
    bool Initialize(const CaptureSettings& settings);
    
    /**
     * @brief 캡처 시스템 정리
     */
    void Cleanup();

    /**
     * @brief 화면 캡처 실행
     * @return 캡처 성공 여부
     */
    bool CaptureScreen();
    
    /**
     * @brief 프레임 캡처 (CaptureScreen 별칭)
     * @return 캡처 성공 여부
     */
    bool CaptureFrame();
    
    /**
     * @brief 최적화된 화면 캡처
     * @return 캡처 성공 여부
     */
    bool CaptureScreenOptimized();
    
    /**
     * @brief 특정 모니터 캡처
     * @param monitorIndex 모니터 인덱스
     * @return 캡처 성공 여부
     */
    bool CaptureScreenOptimized(int monitorIndex);

    /**
     * @brief 최신 캡처된 프레임 반환 (OpenCV Mat)
     * @return 캡처된 이미지 (BGR 형식)
     */
    cv::Mat GetLatestFrame() const;
    
    /**
     * @brief 원시 이미지 데이터 반환
     * @return BGRA 형식의 이미지 데이터
     */
    const std::vector<uint8_t>& GetImageData() const;
    
    /**
     * @brief 캡처된 이미지 너비
     * @return 이미지 너비 (픽셀)
     */
    int GetWidth() const;
    
    /**
     * @brief 캡처된 이미지 높이
     * @return 이미지 높이 (픽셀)
     */
    int GetHeight() const;
    
    /**
     * @brief OpenGL 텍스처 ID 반환
     * @return 텍스처 ID
     */
    unsigned int GetTextureID() const;
    
    /**
     * @brief 초기화 상태 확인
     * @return 초기화 완료 여부
     */
    bool IsInitialized() const;

    /**
     * @brief 성능 통계 반환
     * @return 성능 통계 구조체
     */
    const PerformanceStats& GetPerformanceStats() const;
    
    /**
     * @brief 성능 통계 리셋
     */
    void ResetPerformanceStats();
    
    /**
     * @brief 설정 업데이트
     * @param settings 새로운 설정
     */
    void UpdateSettings(const CaptureSettings& settings);
    
    /**
     * @brief 현재 설정 반환
     * @return 현재 캡처 설정
     */
    const CaptureSettings& GetSettings() const;

    /**
     * @brief 사용 가능한 모니터 목록 반환
     * @return 모니터 정보 목록
     */
    static std::vector<MonitorInfo> GetAvailableMonitors();
    
    /**
     * @brief 특정 모니터로 전환
     * @param monitorIndex 모니터 인덱스
     * @return 전환 성공 여부
     */
    bool SetMonitor(int monitorIndex);
    
    /**
     * @brief 대상 모니터 설정
     * @param monitorIndex 모니터 인덱스
     * @return 설정 성공 여부
     */
    bool SetTargetMonitor(int monitorIndex);
    
    /**
     * @brief 현재 모니터 인덱스 반환
     * @return 현재 모니터 인덱스
     */
    int GetCurrentMonitorIndex() const;
    
    /**
     * @brief 현재 모니터 인덱스 반환 (별칭)
     * @return 현재 모니터 인덱스
     */
    int GetCurrentMonitor() const;
    
    /**
     * @brief 현재 모니터 정보 반환
     * @return 현재 모니터 정보
     */
    const MonitorInfo& GetCurrentMonitorInfo() const;

private:
    /**
     * @brief 모니터 정보 변환
     * @param info CrossPlatformCapture의 모니터 정보
     * @param index 모니터 인덱스
     * @return 변환된 모니터 정보
     */
    static MonitorInfo ConvertMonitorInfo(const CrossPlatformCapture::MonitorInfo& info, int index);
    
    /**
     * @brief 성능 통계 업데이트
     */
    void UpdatePerformanceStats();
    
    /**
     * @brief FPS 제한 확인
     * @return FPS 제한 필요 여부
     */
    bool ShouldLimitFPS();
};