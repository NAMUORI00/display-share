#pragma once

#include <opencv2/opencv.hpp>
#include <memory>
#include <atomic>
#include <chrono>

/**
 * @brief 320x320 중심 영역 캡처 최적화 유틸리티
 * 
 * 전체 화면 캡처에서 320x320 중심 영역만을 효율적으로 추출하여
 * 메모리 사용량과 처리 시간을 최적화합니다.
 */
class CenterRegionCapture {
public:
    /**
     * @brief 중심 영역 정보 구조체
     */
    struct RegionInfo {
        int x;          ///< 중심 영역 시작 X 좌표
        int y;          ///< 중심 영역 시작 Y 좌표
        int width;      ///< 중심 영역 너비 (320)
        int height;     ///< 중심 영역 높이 (320)
        int source_width;   ///< 원본 이미지 너비
        int source_height;  ///< 원본 이미지 높이
        double scale_x; ///< X축 스케일 팩터 (좌표 변환용)
        double scale_y; ///< Y축 스케일 팩터 (좌표 변환용)
    };

    /**
     * @brief 성능 메트릭 구조체
     */
    struct PerformanceMetrics {
        double extraction_time_ms;  ///< 영역 추출 시간 (밀리초)
        uint64_t total_extractions; ///< 총 추출 횟수
        double average_time_ms;     ///< 평균 추출 시간 (밀리초)
        size_t memory_usage_bytes;  ///< 메모리 사용량 (바이트)
    };

    static constexpr int TARGET_WIDTH = 320;   ///< 목표 영역 너비
    static constexpr int TARGET_HEIGHT = 320;  ///< 목표 영역 높이

    CenterRegionCapture();
    ~CenterRegionCapture() = default;

    /**
     * @brief 전체 화면 이미지에서 320x320 중심 영역 추출
     * @param source_image 원본 전체 화면 이미지
     * @param output_region 추출된 320x320 중심 영역 (출력)
     * @return 추출 성공 여부
     */
    bool ExtractCenterRegion(const cv::Mat& source_image, cv::Mat& output_region);

    /**
     * @brief 중심 영역 정보 계산
     * @param source_width 원본 이미지 너비
     * @param source_height 원본 이미지 높이
     * @return 계산된 중심 영역 정보
     */
    RegionInfo CalculateRegionInfo(int source_width, int source_height) const;

    /**
     * @brief 320x320 좌표를 전체 화면 좌표로 변환
     * @param region_x 320x320 영역 내 X 좌표
     * @param region_y 320x320 영역 내 Y 좌표
     * @param region_info 영역 정보
     * @param screen_x 전체 화면 X 좌표 (출력)
     * @param screen_y 전체 화면 Y 좌표 (출력)
     */
    void TransformToScreenCoordinates(int region_x, int region_y, 
                                    const RegionInfo& region_info,
                                    int& screen_x, int& screen_y) const;

    /**
     * @brief 전체 화면 좌표를 320x320 좌표로 변환
     * @param screen_x 전체 화면 X 좌표
     * @param screen_y 전체 화면 Y 좌표
     * @param region_info 영역 정보
     * @param region_x 320x320 영역 내 X 좌표 (출력)
     * @param region_y 320x320 영역 내 Y 좌표 (출력)
     * @return 좌표가 320x320 영역 내에 있는지 여부
     */
    bool TransformToRegionCoordinates(int screen_x, int screen_y,
                                    const RegionInfo& region_info, 
                                    int& region_x, int& region_y) const;

    /**
     * @brief 현재 성능 메트릭 조회
     * @return 성능 메트릭
     */
    PerformanceMetrics GetPerformanceMetrics() const;

    /**
     * @brief 성능 메트릭 초기화
     */
    void ResetPerformanceMetrics();

    /**
     * @brief 메모리 사전 할당 최적화
     * @param expected_width 예상 이미지 너비
     * @param expected_height 예상 이미지 높이
     */
    void OptimizeMemoryAllocation(int expected_width, int expected_height);

private:
    // 성능 메트릭
    mutable std::atomic<uint64_t> total_extractions_;
    mutable std::atomic<double> total_extraction_time_ms_;
    mutable std::chrono::high_resolution_clock::time_point last_extraction_time_;
    
    // 메모리 최적화를 위한 사전 할당된 버퍼
    cv::Mat temp_buffer_;
    std::atomic<size_t> allocated_memory_bytes_;

    /**
     * @brief 추출 시간 측정 및 메트릭 업데이트
     * @param extraction_time_ms 추출 시간 (밀리초)
     */
    void UpdatePerformanceMetrics(double extraction_time_ms) const;
};