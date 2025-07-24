#pragma once

#include "../interfaces/IDetectionAlgorithm.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <chrono>
#include <atomic>

/**
 * @brief HSV 색상 기반 감지 알고리즘 구현
 * 
 * HSV 색공간에서 특정 색상 범위를 감지하는 알고리즘입니다.
 */
class HSVColorDetection : public IDetectionAlgorithm {
public:
    /**
     * @brief HSV 색상 감지 설정
     */
    struct HSVSettings : public AlgorithmSettings {
        cv::Scalar lower_bound = cv::Scalar(140, 120, 180);  // HSV 하한값
        cv::Scalar upper_bound = cv::Scalar(160, 200, 255);  // HSV 상한값
        int min_area = 100;                                  // 최소 영역 크기
        int max_area = 50000;                               // 최대 영역 크기
        bool enable_morphology = true;                       // 모폴로지 연산 활성화
        int morphology_kernel_size = 5;                      // 모폴로지 커널 크기
        bool enable_gaussian_blur = true;                    // 가우시안 블러 활성화
        int gaussian_kernel_size = 5;                        // 가우시안 커널 크기
        double gaussian_sigma = 1.0;                         // 가우시안 시그마
        
        HSVSettings() {
            confidence_threshold = 0.7;  // HSV는 면적 비율로 신뢰도 계산
        }
    };

    HSVColorDetection();
    virtual ~HSVColorDetection();

    // IDetectionAlgorithm 인터페이스 구현
    bool Initialize(const AlgorithmSettings& settings) override;
    DetectionResult DetectSingle(const cv::Mat& image) override;
    std::vector<DetectionResult> DetectMultiple(const cv::Mat& image) override;
    std::vector<DetectionResult> DetectInROI(const cv::Mat& image, const cv::Rect& roi) override;
    bool UpdateSettings(const AlgorithmSettings& settings) override;
    std::string GetAlgorithmName() const override;
    std::string GetVersion() const override;
    std::vector<std::string> GetSupportedClasses() const override;
    std::string GetPerformanceStats() const override;
    void Reset() override;
    void Cleanup() override;

    /**
     * @brief HSV 범위 설정
     * @param lower HSV 하한값
     * @param upper HSV 상한값
     */
    void SetHSVRange(const cv::Scalar& lower, const cv::Scalar& upper);

    /**
     * @brief 현재 HSV 범위 조회
     * @param lower 출력: HSV 하한값
     * @param upper 출력: HSV 상한값
     */
    void GetHSVRange(cv::Scalar& lower, cv::Scalar& upper) const;

    /**
     * @brief 영역 크기 제한 설정
     * @param min_area 최소 영역
     * @param max_area 최대 영역
     */
    void SetAreaConstraints(int min_area, int max_area);

    /**
     * @brief 마스크 이미지 반환 (디버깅용)
     * @return 최근 처리된 마스크 이미지
     */
    cv::Mat GetLastMask() const;

private:
    HSVSettings hsv_settings_;
    bool is_initialized_;
    
    // 성능 메트릭
    mutable std::atomic<uint64_t> total_detections_;
    mutable std::atomic<double> average_processing_time_ms_;
    mutable std::atomic<double> total_processing_time_ms_;
    mutable std::chrono::high_resolution_clock::time_point last_detection_time_;
    
    // 캐시된 이미지들 (디버깅용)
    mutable cv::Mat last_mask_;
    mutable cv::Mat last_morphology_result_;
    mutable std::mutex cache_mutex_;

    /**
     * @brief 이미지 전처리
     * @param image 입력 이미지
     * @param preprocessed 출력: 전처리된 이미지
     */
    void PreprocessImage(const cv::Mat& image, cv::Mat& preprocessed) const;

    /**
     * @brief HSV 마스크 생성
     * @param hsv_image HSV 이미지
     * @param mask 출력: 생성된 마스크
     */
    void CreateHSVMask(const cv::Mat& hsv_image, cv::Mat& mask) const;

    /**
     * @brief 모폴로지 연산 적용
     * @param mask 입력 마스크
     * @param result 출력: 모폴로지 적용 결과
     */
    void ApplyMorphology(const cv::Mat& mask, cv::Mat& result) const;

    /**
     * @brief 컨투어 찾기 및 필터링
     * @param mask 마스크 이미지
     * @param contours 출력: 찾은 컨투어들
     */
    void FindAndFilterContours(const cv::Mat& mask, std::vector<std::vector<cv::Point>>& contours) const;

    /**
     * @brief 컨투어를 DetectionResult로 변환
     * @param contour 컨투어
     * @param image_size 원본 이미지 크기
     * @return 변환된 감지 결과
     */
    DetectionResult ContourToDetectionResult(const std::vector<cv::Point>& contour, const cv::Size& image_size) const;

    /**
     * @brief 신뢰도 계산 (컨투어 면적 기반)
     * @param contour 컨투어
     * @param image_size 이미지 크기
     * @return 신뢰도 (0.0 ~ 1.0)
     */
    double CalculateConfidence(const std::vector<cv::Point>& contour, const cv::Size& image_size) const;

    /**
     * @brief 성능 메트릭 업데이트
     * @param processing_time 처리 시간 (밀리초)
     */
    void UpdatePerformanceStats(double processing_time) const;

    /**
     * @brief 설정 유효성 검사
     * @param settings 검사할 설정
     * @return 유효성 여부
     */
    bool ValidateSettings(const HSVSettings& settings) const;
};