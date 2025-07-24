#pragma once

#include "../interfaces/IDetectionAlgorithm.h"
#include "../core/Constants.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <mutex>

// TensorRT 헤더 (조건부 포함)
#ifdef HAVE_TENSORRT
#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <cuda_runtime_api.h>
#endif

/**
 * @brief YOLOv11 + TensorRT 추론 클래스
 * 
 * NVIDIA TensorRT를 사용하여 YOLOv11 모델의 고성능 추론을 제공합니다.
 * GPU가 없거나 TensorRT가 없는 경우 OpenCV DNN으로 폴백됩니다.
 */
class YOLOv11TensorRTInference : public IDetectionAlgorithm {
public:
    /**
     * @brief YOLOv11 TensorRT 설정
     */
    struct TensorRTSettings : public AlgorithmSettings {
        // 모델 경로
        std::string onnx_model_path = "models/yolo11n.onnx";
        std::string tensorrt_engine_path = "models/yolo11n.trt";
        std::string class_names_path = "models/coco_classes.txt";
        
        // TensorRT 엔진 설정
        int max_batch_size = 1;
        size_t max_workspace_size = 1024 * 1024 * 1024; // 1GB
        bool enable_fp16 = true;
        bool enable_int8 = false;
        
        // 추론 설정
        int input_width = 640;
        int input_height = 640;
        float nms_threshold = 0.45f;
        int max_detections = 100;
        
        // 성능 설정
        bool enable_cuda_graph = true;
        bool enable_tactic_sources = true;
        int dla_core = -1; // -1: 비활성화, 0/1: DLA 코어 사용
        
        TensorRTSettings() {
            confidence_threshold = 0.25;
        }
    };

    /**
     * @brief 추론 백엔드 타입
     */
    enum class BackendType {
        AUTO_DETECT,        // 자동 감지 (TensorRT → OpenCV DNN)
        TENSORRT,           // TensorRT 전용
        OPENCV_DNN,         // OpenCV DNN 전용
        TENSORRT_FALLBACK   // TensorRT 실패 시 OpenCV DNN 폴백
    };

    YOLOv11TensorRTInference();
    virtual ~YOLOv11TensorRTInference();

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
     * @brief TensorRT 엔진 빌드 (ONNX → TRT)
     * @param force_rebuild 기존 엔진이 있어도 강제로 다시 빌드
     * @return 빌드 성공 여부
     */
    bool BuildTensorRTEngine(bool force_rebuild = false);

    /**
     * @brief 현재 사용 중인 백엔드 반환
     * @return 백엔드 타입
     */
    BackendType GetCurrentBackend() const;

    /**
     * @brief TensorRT 사용 가능 여부 확인
     * @return TensorRT 사용 가능 여부
     */
    bool IsTensorRTAvailable() const;

    /**
     * @brief GPU 메모리 사용량 조회
     * @return GPU 메모리 사용량 (MB)
     */
    size_t GetGPUMemoryUsage() const;

    /**
     * @brief 배치 추론 수행
     * @param images 입력 이미지 배치
     * @return 배치별 감지 결과
     */
    std::vector<std::vector<DetectionResult>> DetectBatch(const std::vector<cv::Mat>& images);

    /**
     * @brief 엔진 워밍업 (첫 추론 성능 개선)
     * @param warmup_iterations 워밍업 반복 횟수
     */
    void WarmupEngine(int warmup_iterations = 10);

private:
    TensorRTSettings settings_;
    BackendType current_backend_;
    bool is_initialized_;
    
    // 클래스 이름
    std::vector<std::string> class_names_;
    
    // 성능 메트릭
    mutable std::atomic<uint64_t> total_inferences_;
    mutable std::atomic<double> total_inference_time_ms_;
    mutable std::atomic<double> average_inference_time_ms_;
    mutable std::chrono::high_resolution_clock::time_point last_inference_time_;
    
    // 스레드 안전성
    mutable std::mutex inference_mutex_;

#ifdef HAVE_TENSORRT
    // TensorRT 관련 멤버
    std::unique_ptr<nvinfer1::IRuntime> runtime_;
    std::unique_ptr<nvinfer1::ICudaEngine> engine_;
    std::unique_ptr<nvinfer1::IExecutionContext> context_;
    
    // CUDA 메모리 관리
    void* gpu_input_buffer_ = nullptr;
    void* gpu_output_buffer_ = nullptr;
    void* cpu_output_buffer_ = nullptr;
    cudaStream_t cuda_stream_ = nullptr;
    
    // 엔진 정보
    int input_binding_index_ = -1;
    int output_binding_index_ = -1;
    size_t input_size_ = 0;
    size_t output_size_ = 0;
#endif

    // OpenCV DNN 폴백
    cv::dnn::Net opencv_net_;
    bool opencv_net_initialized_ = false;

    /**
     * @brief TensorRT 초기화
     * @return 초기화 성공 여부
     */
    bool InitializeTensorRT();

    /**
     * @brief OpenCV DNN 초기화
     * @return 초기화 성공 여부
     */
    bool InitializeOpenCVDNN();

    /**
     * @brief TensorRT 엔진 로드
     * @return 로드 성공 여부
     */
    bool LoadTensorRTEngine();

    /**
     * @brief CUDA 메모리 할당
     * @return 할당 성공 여부
     */
    bool AllocateCUDAMemory();

    /**
     * @brief CUDA 메모리 해제
     */
    void FreeCUDAMemory();

    /**
     * @brief 이미지 전처리 (TensorRT용)
     * @param image 입력 이미지
     * @param input_tensor 출력 텐서
     */
    void PreprocessImageTensorRT(const cv::Mat& image, float* input_tensor);

    /**
     * @brief 이미지 전처리 (OpenCV DNN용)
     * @param image 입력 이미지
     * @return 전처리된 blob
     */
    cv::Mat PreprocessImageOpenCV(const cv::Mat& image);

    /**
     * @brief TensorRT 추론 실행
     * @param image 입력 이미지
     * @return 감지 결과 목록
     */
    std::vector<DetectionResult> InferTensorRT(const cv::Mat& image);

    /**
     * @brief OpenCV DNN 추론 실행
     * @param image 입력 이미지
     * @return 감지 결과 목록
     */
    std::vector<DetectionResult> InferOpenCVDNN(const cv::Mat& image);

    /**
     * @brief 출력 후처리 (NMS 적용)
     * @param output_data 추론 결과 데이터
     * @param image_size 원본 이미지 크기
     * @return 후처리된 감지 결과
     */
    std::vector<DetectionResult> PostprocessOutput(const float* output_data, const cv::Size& image_size);

    /**
     * @brief Non-Maximum Suppression 적용
     * @param detections 감지 결과 목록
     * @return NMS 적용된 결과
     */
    std::vector<DetectionResult> ApplyNMS(const std::vector<DetectionResult>& detections);

    /**
     * @brief 클래스 이름 파일 로드
     * @param class_names_path 클래스 이름 파일 경로
     * @return 로드 성공 여부
     */
    bool LoadClassNames(const std::string& class_names_path);

    /**
     * @brief 성능 메트릭 업데이트
     * @param inference_time 추론 시간 (밀리초)
     */
    void UpdatePerformanceMetrics(double inference_time) const;

    /**
     * @brief 설정 유효성 검사
     * @param settings 검사할 설정
     * @return 유효성 여부
     */
    bool ValidateSettings(const TensorRTSettings& settings) const;

    /**
     * @brief 좌표 변환 (YOLO → OpenCV)
     * @param x 중심 X (정규화)
     * @param y 중심 Y (정규화)
     * @param w 너비 (정규화)
     * @param h 높이 (정규화)
     * @param img_width 이미지 너비
     * @param img_height 이미지 높이
     * @return 변환된 바운딩 박스
     */
    cv::Rect ConvertYOLOCoordinates(float x, float y, float w, float h, int img_width, int img_height);

    /**
     * @brief 백엔드 자동 선택
     * @return 선택된 백엔드
     */
    BackendType SelectOptimalBackend();

    /**
     * @brief 오류 처리 및 폴백
     * @param error_message 오류 메시지
     * @return 폴백 성공 여부
     */
    bool HandleErrorAndFallback(const std::string& error_message);
};