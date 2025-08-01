#pragma once

#include <chrono>
#include <string>
#include <atomic>
#include <memory>

namespace ScreenMonitor {

/**
 * @brief 간단한 성능 메트릭 수집 클래스
 * 
 * ImprovedPerformanceMonitor를 대체하는 경량화된 성능 측정 시스템
 * 필수 기능만 제공하여 시스템 복잡성을 줄임
 */
class SimpleMetrics {
public:
    SimpleMetrics() = default;
    ~SimpleMetrics() = default;

    // 복사 및 이동 방지
    SimpleMetrics(const SimpleMetrics&) = delete;
    SimpleMetrics& operator=(const SimpleMetrics&) = delete;
    SimpleMetrics(SimpleMetrics&&) = delete;
    SimpleMetrics& operator=(SimpleMetrics&&) = delete;

    /**
     * @brief FPS 업데이트
     * @param fps 현재 FPS 값
     */
    void updateFPS(double fps) noexcept {
        current_fps_.store(fps, std::memory_order_relaxed);
    }

    /**
     * @brief 프레임 처리 시간 업데이트
     * @param process_time_ms 프레임 처리 시간 (밀리초)
     */
    void updateProcessTime(double process_time_ms) noexcept {
        current_process_time_.store(process_time_ms, std::memory_order_relaxed);
    }

    /**
     * @brief 현재 FPS 반환
     * @return 현재 FPS 값
     */
    double getCurrentFPS() const noexcept {
        return current_fps_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 현재 프레임 처리 시간 반환
     * @return 현재 프레임 처리 시간 (밀리초)
     */
    double getCurrentProcessTime() const noexcept {
        return current_process_time_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 메트릭 초기화
     */
    void reset() noexcept {
        current_fps_.store(0.0, std::memory_order_relaxed);
        current_process_time_.store(0.0, std::memory_order_relaxed);
    }

    /**
     * @brief 간단한 성능 보고서 생성
     * @return 성능 정보 문자열
     */
    std::string getReport() const {
        const double fps = getCurrentFPS();
        const double process_time = getCurrentProcessTime();
        
        return "FPS: " + std::to_string(fps) + 
               ", Process Time: " + std::to_string(process_time) + "ms";
    }

private:
    std::atomic<double> current_fps_{0.0};
    std::atomic<double> current_process_time_{0.0};
};

} // namespace ScreenMonitor