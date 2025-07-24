#pragma once

#include <opencv2/core.hpp>
#include <vector>
#include <string>
#include <memory>

/**
 * @brief 감지 알고리즘 인터페이스
 * 
 * 다양한 컴퓨터 비전 알고리즘의 추상화 인터페이스입니다.
 */
class IDetectionAlgorithm {
public:
    /**
     * @brief 감지 결과 구조체
     */
    struct DetectionResult {
        cv::Rect bounding_box;
        double confidence;
        std::string label;
        int class_id;
        cv::Point2f center;
        
        DetectionResult() 
            : confidence(0.0), label("unknown"), class_id(-1), center(0, 0) {}
            
        DetectionResult(const cv::Rect& bbox, double conf, const std::string& lbl, int cls_id = -1)
            : bounding_box(bbox), confidence(conf), label(lbl), class_id(cls_id) {
            center = cv::Point2f(bbox.x + bbox.width / 2.0f, bbox.y + bbox.height / 2.0f);
        }
    };

    /**
     * @brief 알고리즘 설정 구조체 (기본)
     */
    struct AlgorithmSettings {
        double confidence_threshold = 0.5;
        bool enable_preprocessing = true;
        bool enable_postprocessing = true;
        
        virtual ~AlgorithmSettings() = default;
    };

    virtual ~IDetectionAlgorithm() = default;

    /**
     * @brief 알고리즘 초기화
     * @param settings 알고리즘 설정
     * @return 초기화 성공 여부
     */
    virtual bool Initialize(const AlgorithmSettings& settings) = 0;

    /**
     * @brief 단일 객체 감지
     * @param image 입력 이미지
     * @return 감지된 객체 (신뢰도가 가장 높은 것)
     */
    virtual DetectionResult DetectSingle(const cv::Mat& image) = 0;

    /**
     * @brief 다중 객체 감지
     * @param image 입력 이미지
     * @return 감지된 모든 객체 목록
     */
    virtual std::vector<DetectionResult> DetectMultiple(const cv::Mat& image) = 0;

    /**
     * @brief 특정 영역에서 감지
     * @param image 입력 이미지
     * @param roi 관심 영역 (Region of Interest)
     * @return 해당 영역에서 감지된 객체 목록
     */
    virtual std::vector<DetectionResult> DetectInROI(const cv::Mat& image, const cv::Rect& roi) = 0;

    /**
     * @brief 알고리즘 설정 업데이트
     * @param settings 새로운 설정
     * @return 설정 성공 여부
     */
    virtual bool UpdateSettings(const AlgorithmSettings& settings) = 0;

    /**
     * @brief 알고리즘 이름 반환
     * @return 알고리즘 이름
     */
    virtual std::string GetAlgorithmName() const = 0;

    /**
     * @brief 알고리즘 버전 반환
     * @return 버전 문자열
     */
    virtual std::string GetVersion() const = 0;

    /**
     * @brief 지원하는 클래스 목록
     * @return 클래스 이름 목록
     */
    virtual std::vector<std::string> GetSupportedClasses() const = 0;

    /**
     * @brief 성능 통계 반환
     * @return 처리 시간, FPS 등의 성능 정보
     */
    virtual std::string GetPerformanceStats() const = 0;

    /**
     * @brief 알고리즘 리셋
     */
    virtual void Reset() = 0;

    /**
     * @brief 알고리즘 정리
     */
    virtual void Cleanup() = 0;
};