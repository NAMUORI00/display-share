#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <chrono>
#include <vector>
#include <string>

/**
 * @brief HSV 색상 기반 객체 탐지기
 * 
 * 고성능 HSV 색상 기반 객체 탐지 시스템
 */
class ColorDetector {
public:
    /**
     * @brief 탐지 결과 구조체
     */
    struct DetectionResult {
        cv::Rect bbox;
        double confidence;
        std::string label;
        
        DetectionResult() : confidence(0.0), label("object") {}
        DetectionResult(const cv::Rect& rect, double conf = 1.0, const std::string& lbl = "object")
            : bbox(rect), confidence(conf), label(lbl) {}
    };

    /**
     * @brief HSV 색상 범위 설정
     */
    struct HSVRange {
        int h_min, s_min, v_min;
        int h_max, s_max, v_max;
        
        HSVRange() : h_min(140), s_min(120), v_min(180), h_max(160), s_max(200), v_max(255) {}
        HSVRange(int hmin, int smin, int vmin, int hmax, int smax, int vmax)
            : h_min(hmin), s_min(smin), v_min(vmin), h_max(hmax), s_max(smax), v_max(vmax) {}
    };

private:
    // HSV 색상 범위
    HSVRange color_range_;
    
    // 모폴로지 연산 커널 크기
    int morph_kernel_size_;
    
    // 컨투어 필터링 매개변수
    int min_contour_area_;
    int min_contour_size_;
    
    // 성능 모니터링
    mutable std::chrono::high_resolution_clock::time_point last_process_time_;
    mutable double processing_time_ms_;
    mutable int frame_count_;
    mutable double avg_fps_;
    
    // 설정 상태
    bool configured_;

public:
    /**
     * @brief ColorDetector 생성자
     */
    ColorDetector();
    
    /**
     * @brief 소멸자
     */
    virtual ~ColorDetector() = default;

    /**
     * @brief 단일 객체 탐지
     * @param image 입력 이미지
     * @return 탐지된 객체의 경계 상자
     */
    cv::Rect detectTarget(const cv::Mat& image);
    
    /**
     * @brief 다중 객체 탐지
     * @param image 입력 이미지
     * @return 탐지된 객체들의 결과 리스트
     */
    std::vector<DetectionResult> detectMultipleTargets(const cv::Mat& image);
    
    /**
     * @brief HSV 색상 범위 설정
     * @param range HSV 색상 범위
     */
    void setColorRange(const HSVRange& range);
    
    /**
     * @brief HSV 색상 범위 설정 (cv::Scalar 버전)
     * @param lower 하한값 (H,S,V)
     * @param upper 상한값 (H,S,V)
     */
    void setColorRange(const cv::Scalar& lower, const cv::Scalar& upper);
    
    /**
     * @brief 현재 HSV 색상 범위 반환
     * @return 현재 설정된 색상 범위
     */
    const HSVRange& getColorRange() const { return color_range_; }

    /**
     * @brief 탐지기 초기화
     * @return 초기화 성공 여부
     */
    bool initialize();
    
    /**
     * @brief 탐지기 리셋
     */
    void reset();
    
    /**
     * @brief 알고리즘 이름 반환
     * @return 알고리즘 이름
     */
    std::string getAlgorithmName() const { return "HSV_ColorDetector"; }
    
    /**
     * @brief 설정 상태 확인
     * @return 설정 완료 여부
     */
    bool isConfigured() const { return configured_; }
    
    /**
     * @brief 성능 통계 반환
     * @return 처리 시간, FPS 등의 성능 정보
     */
    std::string getPerformanceStats() const;

private:
    /**
     * @brief HSV 색상 마스크 생성
     * @param hsv_image HSV 변환된 입력 이미지
     * @return 이진화된 마스크 이미지
     */
    cv::Mat createColorMask(const cv::Mat& hsv_image) const;
    
    /**
     * @brief 컨투어에서 객체 추출
     * @param contours 컨투어 목록
     * @param original_size 원본 이미지 크기
     * @return 탐지된 객체 목록
     */
    std::vector<DetectionResult> extractTargetsFromContours(
        const std::vector<std::vector<cv::Point>>& contours,
        const cv::Size& original_size) const;
    
    /**
     * @brief 성능 통계 업데이트
     */
    void updatePerformanceStats() const;
};