#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
// DNN module temporarily disabled for build testing
// #ifdef HAVE_OPENCV_DNN
// #include <opencv2/dnn.hpp>
// #endif
#ifdef HAVE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif
#include <vector>
#include <string>
#include <chrono>
#include <memory>

/**
 * @brief YOLO 백엔드 타입
 */
enum class YOLOBackend {
    OPENCV_DNN,         // OpenCV DNN 모듈 사용 (CPU)
    ONNX_RUNTIME_GPU    // ONNX Runtime GPU 전용 (DirectML)
};

/**
 * @brief YOLO 기반 객체 탐지기
 * 
 * DirectML GPU 가속 YOLO v11 ONNX 시스템 (GPU 전용)
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
    // YOLO 백엔드 설정
    YOLOBackend backend_type_;
    
    // OpenCV DNN 백엔드 - temporarily disabled
// #ifdef HAVE_OPENCV_DNN
//     cv::dnn::Net net_;
// #endif
    
    // ONNX Runtime 백엔드
#ifdef HAVE_ONNXRUNTIME
    std::unique_ptr<Ort::Env> ort_env_;
    std::unique_ptr<Ort::Session> ort_session_;
    std::unique_ptr<Ort::SessionOptions> ort_session_options_;
    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;
    std::vector<int64_t> input_shape_;
    std::vector<int64_t> output_shape_;
#endif
    
    // YOLO 모델 설정
    std::vector<std::string> class_names_;
    cv::Size input_size_;
    float confidence_threshold_;
    float nms_threshold_;
    bool model_loaded_;
    bool using_gpu_;
    
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
     * @brief YOLO v11 ONNX 모델 로드 (권장)
     * @param onnx_path ONNX 모델 파일 경로
     * @param backend 사용할 백엔드 (기본값: OPENCV_DNN)
     * @return 로드 성공 여부
     */
    bool loadYOLOv11Model(const std::string& onnx_path, YOLOBackend backend = YOLOBackend::OPENCV_DNN);
    
    /**
     * @brief 클래스 이름 파일 로드
     * @param class_names_path 클래스 이름 파일 경로
     * @return 로드 성공 여부
     */
    bool loadClassNames(const std::string& class_names_path);
    
    /**
     * @brief 백엔드 변경
     * @param backend 새로운 백엔드
     * @return 변경 성공 여부
     */
    bool setBackend(YOLOBackend backend);
    
    /**
     * @brief 현재 백엔드 반환
     * @return 현재 사용 중인 백엔드
     */
    YOLOBackend getBackend() const { return backend_type_; }
    
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
     * @brief 모델 정보 반환
     * @return 모델 정보 문자열
     */
    std::string getModelInfo() const;
    
    /**
     * @brief GPU 사용 설정
     * @param use_gpu GPU 사용 여부
     * @return 설정 성공 여부
     */
    bool setUseGPU(bool use_gpu);
    
    /**
     * @brief GPU 사용 상태 확인
     * @return GPU 사용 여부
     */
    bool isUsingGPU() const;
    
    /**
     * @brief GPU 상태 및 DirectML 지원 확인
     * @return GPU 상태 정보 문자열
     */
    std::string getGPUStatus() const;
    
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
    
    // YOLO v11 특화 메서드들
    
    /**
     * @brief YOLO v11용 이미지 전처리
     * @param image 입력 이미지
     * @return 전처리된 blob 또는 텐서
     */
    cv::Mat preprocessYOLOv11(const cv::Mat& image);
    
    /**
     * @brief YOLO v11 OpenCV DNN 후처리
     * @param outputs 네트워크 출력
     * @param image_size 원본 이미지 크기
     * @return 탐지 결과 목록
     */
    std::vector<DetectionResult> postprocessYOLOv11OpenCV(const std::vector<cv::Mat>& outputs, 
                                                         const cv::Size& image_size);
    
#ifdef HAVE_ONNXRUNTIME
    /**
     * @brief ONNX Runtime 세션 초기화
     * @param model_path ONNX 모델 경로
     * @return 초기화 성공 여부
     */
    bool initONNXRuntime(const std::string& model_path);
    
    /**
     * @brief YOLO v11 ONNX Runtime 추론
     * @param input_tensor 입력 텐서
     * @return 출력 텐서
     */
    std::vector<std::vector<float>> runONNXInference(const std::vector<float>& input_tensor);
    
    /**
     * @brief YOLO v11 ONNX Runtime 후처리
     * @param outputs ONNX 출력
     * @param image_size 원본 이미지 크기
     * @return 탐지 결과 목록
     */
    std::vector<DetectionResult> postprocessYOLOv11ONNX(const std::vector<std::vector<float>>& outputs,
                                                       const cv::Size& image_size);
#endif
    
    /**
     * @brief 좌표 변환 (YOLO v11 형식 → OpenCV Rect)
     * @param cx 중심 X 좌표 (정규화)
     * @param cy 중심 Y 좌표 (정규화)
     * @param w 폭 (정규화)
     * @param h 높이 (정규화)
     * @param img_width 이미지 폭
     * @param img_height 이미지 높이
     * @return 변환된 바운딩 박스
     */
    cv::Rect convertYOLOv11Box(float cx, float cy, float w, float h, 
                              int img_width, int img_height);
};