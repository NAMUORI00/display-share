#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <vector>
#include <string>
#include <chrono>

/**
 * @brief YOLO 기반 객체 탐지기
 * 
 * AI 기반 고성능 객체 탐지 시스템
 */
class ObjectDetector {
public:
    /**
     * @brief 탐지 결과 구조체
     */
    struct DetectionResult {
        cv::Rect bbox;
        double confidence;
        std::string label;
        int class_id;
        
        DetectionResult() : confidence(0.0), label("object"), class_id(-1) {}
        DetectionResult(const cv::Rect& rect, double conf, const std::string& lbl, int cls_id = -1)
            : bbox(rect), confidence(conf), label(lbl), class_id(cls_id) {}
    };

private:
    // YOLO 모델 설정
    cv::dnn::Net net_;
    std::vector<std::string> class_names_;
    cv::Size input_size_;
    float confidence_threshold_;
    float nms_threshold_;
    bool model_loaded_;
    
    // 성능 모니터링
    mutable std::chrono::high_resolution_clock::time_point last_process_time_;
    mutable double processing_time_ms_;
    mutable int frame_count_;
    mutable double avg_fps_;

public:
    /**
     * @brief ObjectDetector 생성자
     */
    ObjectDetector();
    
    /**
     * @brief 소멸자
     */
    virtual ~ObjectDetector() = default;

    /**
     * @brief YOLO 모델 로드
     * @param model_path 모델 파일 경로
     * @param config_path 설정 파일 경로 (선택사항)
     * @return 로드 성공 여부
     */
    bool loadModel(const std::string& model_path, const std::string& config_path = "");
    
    /**
     * @brief 단일 객체 탐지 (가장 높은 신뢰도)
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
     * @brief 특정 클래스 객체만 탐지
     * @param image 입력 이미지
     * @param target_classes 탐지할 클래스 이름들
     * @return 탐지된 객체들의 결과 리스트
     */
    std::vector<DetectionResult> detectSpecificClasses(const cv::Mat& image, 
                                                      const std::vector<std::string>& target_classes);
    
    /**
     * @brief 신뢰도 임계값 설정
     * @param threshold 신뢰도 임계값 (0.0 ~ 1.0)
     */
    void setConfidenceThreshold(float threshold);
    
    /**
     * @brief NMS 임계값 설정
     * @param threshold NMS 임계값 (0.0 ~ 1.0)
     */
    void setNMSThreshold(float threshold);
    
    /**
     * @brief 입력 이미지 크기 설정
     * @param size 입력 크기
     */
    void setInputSize(const cv::Size& size);
    
    /**
     * @brief 모델 로드 상태 확인
     * @return 모델 로드 여부
     */
    bool isModelLoaded() const { return model_loaded_; }
    
    /**
     * @brief 지원하는 클래스 목록 반환
     * @return 클래스 이름 목록
     */
    const std::vector<std::string>& getClassNames() const { return class_names_; }
    
    /**
     * @brief 알고리즘 이름 반환
     * @return 알고리즘 이름
     */
    std::string getAlgorithmName() const { return "YOLO_ObjectDetector"; }
    
    /**
     * @brief 성능 통계 반환
     * @return 처리 시간, FPS 등의 성능 정보
     */
    std::string getPerformanceStats() const;
    
    /**
     * @brief 탐지기 리셋
     */
    void reset();

private:
    /**
     * @brief COCO 데이터셋 클래스 이름 초기화
     */
    void initializeCOCOClasses();
    
    /**
     * @brief 이미지를 네트워크 입력 형태로 전처리
     * @param image 입력 이미지
     * @return 전처리된 blob
     */
    cv::Mat preprocessImage(const cv::Mat& image);
    
    /**
     * @brief 네트워크 출력을 후처리하여 탐지 결과 생성
     * @param outputs 네트워크 출력
     * @param image_size 원본 이미지 크기
     * @return 탐지 결과 목록
     */
    std::vector<DetectionResult> postprocessOutputs(const std::vector<cv::Mat>& outputs, 
                                                   const cv::Size& image_size);
    
    /**
     * @brief 성능 통계 업데이트
     */
    void updatePerformanceStats() const;
    
    /**
     * @brief 클래스 이름으로 ID 찾기
     * @param class_name 클래스 이름
     * @return 클래스 ID (-1: 찾지 못함)
     */
    int findClassId(const std::string& class_name) const;
};