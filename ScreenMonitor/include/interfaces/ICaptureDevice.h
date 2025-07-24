#pragma once

#include <opencv2/core.hpp>
#include <vector>
#include <string>
#include <functional>

/**
 * @brief 화면 캡처 디바이스 인터페이스
 * 
 * 화면 캡처 기능의 추상화 인터페이스로 다양한 캡처 구현체를 지원합니다.
 */
class ICaptureDevice {
public:
    /**
     * @brief 모니터 정보 구조체
     */
    struct MonitorInfo {
        int index;
        std::string name;
        int width;
        int height;
        int x;
        int y;
        bool is_primary;
        
        MonitorInfo() : index(0), width(0), height(0), x(0), y(0), is_primary(false) {}
        MonitorInfo(int idx, const std::string& n, int w, int h, int px, int py, bool primary)
            : index(idx), name(n), width(w), height(h), x(px), y(py), is_primary(primary) {}
    };

    /**
     * @brief 캡처 설정 구조체
     */
    struct CaptureSettings {
        int target_fps = 60;
        bool enable_cursor = false;
        bool enable_border = false;
        cv::Rect capture_area = cv::Rect(0, 0, 0, 0); // 전체 화면인 경우 (0,0,0,0)
        
        CaptureSettings() = default;
        CaptureSettings(int fps, bool cursor, bool border, const cv::Rect& area)
            : target_fps(fps), enable_cursor(cursor), enable_border(border), capture_area(area) {}
    };

    virtual ~ICaptureDevice() = default;

    /**
     * @brief 캡처 디바이스 초기화
     * @param settings 캡처 설정
     * @return 초기화 성공 여부
     */
    virtual bool Initialize(const CaptureSettings& settings) = 0;

    /**
     * @brief 캡처 시작
     * @param monitor_index 모니터 인덱스
     * @return 시작 성공 여부
     */
    virtual bool StartCapture(int monitor_index) = 0;

    /**
     * @brief 캡처 중지
     */
    virtual void StopCapture() = 0;

    /**
     * @brief 단일 프레임 캡처
     * @param output_frame 출력 프레임
     * @return 캡처 성공 여부
     */
    virtual bool CaptureFrame(cv::Mat& output_frame) = 0;

    /**
     * @brief 사용 가능한 모니터 목록 조회
     * @return 모니터 정보 목록
     */
    virtual std::vector<MonitorInfo> GetAvailableMonitors() const = 0;

    /**
     * @brief 현재 캡처 상태 확인
     * @return 캡처 중 여부
     */
    virtual bool IsCapturing() const = 0;

    /**
     * @brief 현재 사용 중인 모니터 인덱스
     * @return 모니터 인덱스
     */
    virtual int GetCurrentMonitorIndex() const = 0;

    /**
     * @brief 캡처 설정 업데이트
     * @param settings 새로운 설정
     * @return 설정 성공 여부
     */
    virtual bool UpdateSettings(const CaptureSettings& settings) = 0;

    /**
     * @brief 디바이스 성능 통계
     * @return 성능 정보 문자열
     */
    virtual std::string GetPerformanceStats() const = 0;

    /**
     * @brief 디바이스 정리
     */
    virtual void Cleanup() = 0;
};