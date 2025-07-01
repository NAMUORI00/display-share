#pragma once

#include "capture/HighSpeedCapture.h"
#include <gmock/gmock.h>
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

/**
 * @brief 화면 캡처 Mock 클래스
 */
class MockScreenCapture {
public:
    /**
     * @brief Mock 생성자
     */
    MockScreenCapture();
    
    /**
     * @brief Mock 소멸자
     */
    virtual ~MockScreenCapture() = default;
    
    // Mock 메서드들
    MOCK_METHOD(bool, Initialize, (), ());
    MOCK_METHOD(bool, Initialize, (const HighSpeedCapture::CaptureSettings& settings), ());
    MOCK_METHOD(void, Cleanup, (), ());
    MOCK_METHOD(bool, CaptureFrame, (), ());
    MOCK_METHOD(bool, CaptureScreen, (), ());
    MOCK_METHOD(cv::Mat, GetLatestFrame, (), (const));
    MOCK_METHOD(bool, SetTargetMonitor, (int monitorIndex), ());
    MOCK_METHOD(int, GetCurrentMonitorIndex, (), (const));
    MOCK_METHOD(std::vector<HighSpeedCapture::MonitorInfo>, GetAvailableMonitors, (), (const));
    MOCK_METHOD(void, UpdateSettings, (const HighSpeedCapture::CaptureSettings& settings), ());
    MOCK_METHOD(HighSpeedCapture::CaptureSettings, GetSettings, (), (const));
    
    // 헬퍼 메서드들
    
    /**
     * @brief 테스트용 프레임 설정
     * @param frame 반환할 프레임
     */
    void SetMockFrame(const cv::Mat& frame);
    
    /**
     * @brief 테스트용 모니터 목록 설정
     * @param monitors 모니터 정보 목록
     */
    void SetMockMonitors(const std::vector<HighSpeedCapture::MonitorInfo>& monitors);
    
    /**
     * @brief 캡처 성공률 설정
     * @param success_rate 성공률 (0.0-1.0)
     */
    void SetCaptureSuccessRate(double success_rate);
    
    /**
     * @brief 프레임 시퀀스 설정 (여러 프레임을 순서대로 반환)
     * @param frames 프레임 시퀀스
     */
    void SetFrameSequence(const std::vector<cv::Mat>& frames);
    
    /**
     * @brief 현재 프레임 인덱스 반환
     * @return 현재 프레임 인덱스
     */
    int GetCurrentFrameIndex() const { return m_current_frame_index; }
    
    /**
     * @brief 캡처 호출 횟수 반환
     * @return 캡처 메서드가 호출된 횟수
     */
    int GetCaptureCallCount() const { return m_capture_call_count; }
    
    /**
     * @brief 초기화 상태 시뮬레이션
     * @param initialized 초기화 상태
     */
    void SimulateInitialized(bool initialized) { m_is_initialized = initialized; }
    
    /**
     * @brief 에러 시뮬레이션
     * @param should_fail 실패 여부
     */
    void SimulateError(bool should_fail) { m_should_fail = should_fail; }
    
    /**
     * @brief 지연 시뮬레이션
     * @param delay_ms 지연 시간 (밀리초)
     */
    void SimulateDelay(int delay_ms) { m_delay_ms = delay_ms; }
    
    /**
     * @brief 기본 동작 설정
     */
    void SetupDefaultBehavior();
    
    /**
     * @brief 실패 시나리오 설정
     */
    void SetupFailureScenario();
    
    /**
     * @brief 성능 테스트 시나리오 설정
     */
    void SetupPerformanceScenario();
    
private:
    cv::Mat m_mock_frame;
    std::vector<cv::Mat> m_frame_sequence;
    std::vector<HighSpeedCapture::MonitorInfo> m_mock_monitors;
    HighSpeedCapture::CaptureSettings m_mock_settings;
    
    int m_current_frame_index = 0;
    int m_capture_call_count = 0;
    double m_success_rate = 1.0;
    bool m_is_initialized = false;
    bool m_should_fail = false;
    int m_delay_ms = 0;
    
    // 내부 헬퍼 함수들
    bool ShouldSucceed();
    void SimulateProcessingDelay();
    cv::Mat GetNextFrame();
};