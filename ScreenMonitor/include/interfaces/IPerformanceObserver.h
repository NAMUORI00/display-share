#pragma once

#include <string>
#include <chrono>

/**
 * @brief 성능 관찰자 인터페이스
 * 
 * Observer 패턴을 구현하여 성능 메트릭 변경을 통지받습니다.
 */
class IPerformanceObserver {
public:
    /**
     * @brief 성능 메트릭 구조체
     */
    struct PerformanceMetrics {
        float fps = 0.0f;
        double frame_time_ms = 0.0;
        double processing_time_ms = 0.0;
        size_t memory_usage_mb = 0;
        int dropped_frames = 0;
        std::chrono::steady_clock::time_point timestamp;
        
        PerformanceMetrics() {
            timestamp = std::chrono::steady_clock::now();
        }
    };

    virtual ~IPerformanceObserver() = default;

    /**
     * @brief FPS 업데이트 통지
     * @param fps 현재 FPS
     */
    virtual void OnFPSUpdated(float fps) = 0;

    /**
     * @brief 프레임 시간 업데이트 통지
     * @param frame_time_ms 프레임 처리 시간 (밀리초)
     */
    virtual void OnFrameTimeUpdated(double frame_time_ms) = 0;

    /**
     * @brief 메모리 사용량 업데이트 통지
     * @param memory_usage_mb 메모리 사용량 (MB)
     */
    virtual void OnMemoryUsageUpdated(size_t memory_usage_mb) = 0;

    /**
     * @brief 프레임 드롭 통지
     * @param dropped_count 드롭된 프레임 수
     */
    virtual void OnFrameDropped(int dropped_count) = 0;

    /**
     * @brief 전체 성능 메트릭 업데이트 통지
     * @param metrics 성능 메트릭 집합
     */
    virtual void OnPerformanceUpdated(const PerformanceMetrics& metrics) = 0;

    /**
     * @brief 에러 발생 통지
     * @param error_message 에러 메시지
     */
    virtual void OnError(const std::string& error_message) = 0;
};