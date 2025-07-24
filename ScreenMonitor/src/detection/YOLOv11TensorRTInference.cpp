#include "../../include/detection/YOLOv11TensorRTInference.h"
#include "../../include/core/Constants.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include <filesystem>

#ifdef HAVE_TENSORRT
#include <NvInferPlugin.h>
#endif

YOLOv11TensorRTInference::YOLOv11TensorRTInference()
    : current_backend_(BackendType::AUTO_DETECT)
    , is_initialized_(false)
    , total_inferences_(0)
    , total_inference_time_ms_(0.0)
    , average_inference_time_ms_(0.0) {
    
    // 기본 설정 초기화
    settings_.onnx_model_path = "models/yolo11n.onnx";
    settings_.tensorrt_engine_path = "models/yolo11n.trt";
    settings_.class_names_path = "models/coco_classes.txt";
    settings_.confidence_threshold = 0.25;
    settings_.nms_threshold = 0.45f;
    settings_.input_width = 640;
    settings_.input_height = 640;
    settings_.max_detections = 100;
    settings_.enable_fp16 = true;
    
    std::cout << "[YOLOv11TensorRT] Constructor initialized" << std::endl;
}

YOLOv11TensorRTInference::~YOLOv11TensorRTInference() {
    Cleanup();
}

bool YOLOv11TensorRTInference::Initialize(const AlgorithmSettings& settings) {
    try {
        // 설정을 TensorRTSettings로 캐스팅
        const TensorRTSettings* trt_settings = dynamic_cast<const TensorRTSettings*>(&settings);
        if (trt_settings) {
            if (!ValidateSettings(*trt_settings)) {
                std::cerr << "[YOLOv11TensorRT] Invalid settings provided" << std::endl;
                return false;
            }
            settings_ = *trt_settings;
        } else {
            std::cout << "[YOLOv11TensorRT] Using default settings" << std::endl;
        }
        
        // 클래스 이름 로드
        if (!LoadClassNames(settings_.class_names_path)) {
            std::cerr << "[YOLOv11TensorRT] Failed to load class names" << std::endl;
            return false;
        }
        
        // 최적 백엔드 선택
        current_backend_ = SelectOptimalBackend();
        
        bool initialization_success = false;
        
        switch (current_backend_) {
            case BackendType::TENSORRT:
            case BackendType::AUTO_DETECT:
            case BackendType::TENSORRT_FALLBACK:
                initialization_success = InitializeTensorRT();
                if (!initialization_success && current_backend_ != BackendType::TENSORRT) {
                    std::cout << "[YOLOv11TensorRT] TensorRT failed, falling back to OpenCV DNN" << std::endl;
                    initialization_success = InitializeOpenCVDNN();
                    current_backend_ = BackendType::OPENCV_DNN;
                }
                break;
                
            case BackendType::OPENCV_DNN:
                initialization_success = InitializeOpenCVDNN();
                break;
        }
        
        if (initialization_success) {
            // 성능 메트릭 초기화
            total_inferences_ = 0;
            total_inference_time_ms_ = 0.0;
            average_inference_time_ms_ = 0.0;
            last_inference_time_ = std::chrono::high_resolution_clock::now();
            
            is_initialized_ = true;
            
            std::cout << "[YOLOv11TensorRT] Initialized successfully with backend: ";
            switch (current_backend_) {
                case BackendType::TENSORRT: std::cout << "TensorRT"; break;
                case BackendType::OPENCV_DNN: std::cout << "OpenCV DNN"; break;
                default: std::cout << "Unknown"; break;
            }
            std::cout << std::endl;
            
            std::cout << "  - Input Size: " << settings_.input_width << "x" << settings_.input_height << std::endl;
            std::cout << "  - Classes: " << class_names_.size() << std::endl;
            std::cout << "  - Confidence Threshold: " << settings_.confidence_threshold << std::endl;
            std::cout << "  - NMS Threshold: " << settings_.nms_threshold << std::endl;
            
            return true;
        }
        
        std::cerr << "[YOLOv11TensorRT] All initialization methods failed" << std::endl;
        return false;
        
    } catch (const std::exception& e) {
        std::cerr << "[YOLOv11TensorRT] Initialization failed: " << e.what() << std::endl;
        is_initialized_ = false;
        return false;
    }
}

IDetectionAlgorithm::DetectionResult YOLOv11TensorRTInference::DetectSingle(const cv::Mat& image) {
    if (!is_initialized_ || image.empty()) {
        return DetectionResult();
    }
    
    std::vector<DetectionResult> results = DetectMultiple(image);
    
    if (!results.empty()) {
        // 신뢰도가 가장 높은 결과 반환
        auto best_result = *std::max_element(results.begin(), results.end(),
            [](const DetectionResult& a, const DetectionResult& b) {
                return a.confidence < b.confidence;
            });
        
        return best_result;
    }
    
    return DetectionResult();
}

std::vector<IDetectionAlgorithm::DetectionResult> YOLOv11TensorRTInference::DetectMultiple(const cv::Mat& image) {
    if (!is_initialized_ || image.empty()) {
        return std::vector<DetectionResult>();
    }
    
    std::lock_guard<std::mutex> lock(inference_mutex_);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<DetectionResult> results;
    
    try {
        switch (current_backend_) {
            case BackendType::TENSORRT:
                results = InferTensorRT(image);
                break;
            case BackendType::OPENCV_DNN:
                results = InferOpenCVDNN(image);
                break;
            default:
                std::cerr << "[YOLOv11TensorRT] Unknown backend type" << std::endl;
                return std::vector<DetectionResult>();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double inference_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        UpdatePerformanceMetrics(inference_time);
        
        return results;
        
    } catch (const std::exception& e) {
        std::cerr << "[YOLOv11TensorRT] Inference failed: " << e.what() << std::endl;
        return std::vector<DetectionResult>();
    }
}

std::vector<IDetectionAlgorithm::DetectionResult> YOLOv11TensorRTInference::DetectInROI(
    const cv::Mat& image, const cv::Rect& roi) {
    
    if (!is_initialized_ || image.empty()) {
        return std::vector<DetectionResult>();
    }
    
    // ROI 유효성 검사
    cv::Rect safe_roi = roi & cv::Rect(0, 0, image.cols, image.rows);
    if (safe_roi.width <= 0 || safe_roi.height <= 0) {
        return std::vector<DetectionResult>();
    }
    
    try {
        // ROI 영역만 추출
        cv::Mat roi_image = image(safe_roi);
        
        // ROI에서 감지 수행
        std::vector<DetectionResult> roi_results = DetectMultiple(roi_image);
        
        // 좌표를 원본 이미지 기준으로 변환
        for (auto& result : roi_results) {
            result.bounding_box.x += safe_roi.x;
            result.bounding_box.y += safe_roi.y;
            result.center.x += safe_roi.x;
            result.center.y += safe_roi.y;
        }
        
        return roi_results;
        
    } catch (const std::exception& e) {
        std::cerr << "[YOLOv11TensorRT] ROI detection failed: " << e.what() << std::endl;
        return std::vector<DetectionResult>();
    }
}

bool YOLOv11TensorRTInference::UpdateSettings(const AlgorithmSettings& settings) {
    const TensorRTSettings* trt_settings = dynamic_cast<const TensorRTSettings*>(&settings);
    if (!trt_settings) {
        std::cerr << "[YOLOv11TensorRT] Invalid settings type" << std::endl;
        return false;
    }
    
    if (!ValidateSettings(*trt_settings)) {
        std::cerr << "[YOLOv11TensorRT] Invalid settings values" << std::endl;
        return false;
    }
    
    settings_ = *trt_settings;
    std::cout << "[YOLOv11TensorRT] Settings updated successfully" << std::endl;
    return true;
}

std::string YOLOv11TensorRTInference::GetAlgorithmName() const {
    return "YOLOv11 TensorRT Inference";
}

std::string YOLOv11TensorRTInference::GetVersion() const {
    return Constants::Version::VERSION_STRING;
}

std::vector<std::string> YOLOv11TensorRTInference::GetSupportedClasses() const {
    return class_names_;
}

std::string YOLOv11TensorRTInference::GetPerformanceStats() const {
    std::ostringstream stats;
    stats << "YOLOv11 TensorRT Performance:\n";
    stats << "  Backend: ";
    switch (current_backend_) {
        case BackendType::TENSORRT: stats << "TensorRT"; break;
        case BackendType::OPENCV_DNN: stats << "OpenCV DNN"; break;
        default: stats << "Unknown"; break;
    }
    stats << "\n";
    stats << "  Total Inferences: " << total_inferences_.load() << "\n";
    stats << "  Average Inference Time: " << average_inference_time_ms_.load() << " ms\n";
    stats << "  Total Processing Time: " << total_inference_time_ms_.load() << " ms\n";
    
    if (total_inferences_ > 0) {
        double avg_fps = 1000.0 / average_inference_time_ms_.load();
        stats << "  Average FPS: " << avg_fps << "\n";
    }
    
    return stats.str();
}

void YOLOv11TensorRTInference::Reset() {
    total_inferences_ = 0;
    total_inference_time_ms_ = 0.0;
    average_inference_time_ms_ = 0.0;
    last_inference_time_ = std::chrono::high_resolution_clock::now();
    
    std::cout << "[YOLOv11TensorRT] Performance statistics reset" << std::endl;
}

void YOLOv11TensorRTInference::Cleanup() {
    is_initialized_ = false;
    
#ifdef HAVE_TENSORRT
    FreeCUDAMemory();
    
    context_.reset();
    engine_.reset();
    runtime_.reset();
#endif
    
    opencv_net_ = cv::dnn::Net();
    opencv_net_initialized_ = false;
    
    std::cout << "[YOLOv11TensorRT] Cleanup completed" << std::endl;
}

YOLOv11TensorRTInference::BackendType YOLOv11TensorRTInference::GetCurrentBackend() const {
    return current_backend_;
}

bool YOLOv11TensorRTInference::IsTensorRTAvailable() const {
#ifdef HAVE_TENSORRT
    return true;
#else
    return false;
#endif
}

size_t YOLOv11TensorRTInference::GetGPUMemoryUsage() const {
#ifdef HAVE_TENSORRT
    if (current_backend_ == BackendType::TENSORRT && engine_) {
        return input_size_ + output_size_;
    }
#endif
    return 0;
}

bool YOLOv11TensorRTInference::BuildTensorRTEngine(bool force_rebuild) {
#ifdef HAVE_TENSORRT
    try {
        std::filesystem::path engine_path(settings_.tensorrt_engine_path);
        std::filesystem::path onnx_path(settings_.onnx_model_path);
        
        // 이미 엔진이 존재하고 강제 재빌드가 아닌 경우
        if (std::filesystem::exists(engine_path) && !force_rebuild) {
            std::cout << "[YOLOv11TensorRT] Engine already exists: " << engine_path << std::endl;
            return true;
        }
        
        // ONNX 파일 확인
        if (!std::filesystem::exists(onnx_path)) {
            std::cerr << "[YOLOv11TensorRT] ONNX file not found: " << onnx_path << std::endl;
            return false;
        }
        
        std::cout << "[YOLOv11TensorRT] Building TensorRT engine from ONNX..." << std::endl;
        
        // TensorRT 로거
        auto logger = std::make_unique<nvinfer1::Logger>();
        logger->reportError = [](nvinfer1::ILogger::Severity severity, const char* msg) {
            if (severity <= nvinfer1::ILogger::Severity::kWARNING) {
                std::cerr << "[TensorRT] " << msg << std::endl;
            }
        };
        
        // 빌더 생성
        auto builder = std::unique_ptr<nvinfer1::IBuilder>(nvinfer1::createInferBuilder(*logger));
        if (!builder) {
            std::cerr << "[YOLOv11TensorRT] Failed to create TensorRT builder" << std::endl;
            return false;
        }
        
        // 네트워크 생성
        const auto explicitBatch = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
        auto network = std::unique_ptr<nvinfer1::INetworkDefinition>(builder->createNetworkV2(explicitBatch));
        if (!network) {
            std::cerr << "[YOLOv11TensorRT] Failed to create network" << std::endl;
            return false;
        }
        
        // ONNX 파서 생성
        auto parser = std::unique_ptr<nvonnxparser::IParser>(nvonnxparser::createParser(*network, *logger));
        if (!parser) {
            std::cerr << "[YOLOv11TensorRT] Failed to create ONNX parser" << std::endl;
            return false;
        }
        
        // ONNX 파일 파싱
        if (!parser->parseFromFile(onnx_path.string().c_str(), static_cast<int>(nvinfer1::ILogger::Severity::kWARNING))) {
            std::cerr << "[YOLOv11TensorRT] Failed to parse ONNX file" << std::endl;
            for (int i = 0; i < parser->getNbErrors(); ++i) {
                std::cerr << "  Error " << i << ": " << parser->getError(i)->desc() << std::endl;
            }
            return false;
        }
        
        // 빌더 설정
        auto config = std::unique_ptr<nvinfer1::IBuilderConfig>(builder->createBuilderConfig());
        if (!config) {
            std::cerr << "[YOLOv11TensorRT] Failed to create builder config" << std::endl;
            return false;
        }
        
        config->setMaxWorkspaceSize(settings_.max_workspace_size);
        
        if (settings_.enable_fp16 && builder->platformHasFastFp16()) {
            config->setFlag(nvinfer1::BuilderFlag::kFP16);
            std::cout << "[YOLOv11TensorRT] FP16 optimization enabled" << std::endl;
        }
        
        if (settings_.enable_int8 && builder->platformHasFastInt8()) {
            config->setFlag(nvinfer1::BuilderFlag::kINT8);
            std::cout << "[YOLOv11TensorRT] INT8 optimization enabled" << std::endl;
        }
        
        // 엔진 빌드
        std::cout << "[YOLOv11TensorRT] Building engine... This may take several minutes." << std::endl;
        auto engine = std::unique_ptr<nvinfer1::ICudaEngine>(builder->buildEngineWithConfig(*network, *config));
        if (!engine) {
            std::cerr << "[YOLOv11TensorRT] Failed to build engine" << std::endl;
            return false;
        }
        
        // 엔진 시리얼라이제이션
        auto serialized = std::unique_ptr<nvinfer1::IHostMemory>(engine->serialize());
        if (!serialized) {
            std::cerr << "[YOLOv11TensorRT] Failed to serialize engine" << std::endl;
            return false;
        }
        
        // 엔진 파일 저장
        std::filesystem::create_directories(engine_path.parent_path());
        std::ofstream engine_file(engine_path, std::ios::binary);
        if (!engine_file.is_open()) {
            std::cerr << "[YOLOv11TensorRT] Failed to open engine file for writing: " << engine_path << std::endl;
            return false;
        }
        
        engine_file.write(static_cast<const char*>(serialized->data()), serialized->size());
        engine_file.close();
        
        std::cout << "[YOLOv11TensorRT] Engine built and saved to: " << engine_path << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[YOLOv11TensorRT] Engine build failed: " << e.what() << std::endl;
        return false;
    }
#else
    std::cerr << "[YOLOv11TensorRT] TensorRT not available" << std::endl;
    return false;
#endif
}

void YOLOv11TensorRTInference::WarmupEngine(int warmup_iterations) {
    if (!is_initialized_) {
        return;
    }
    
    std::cout << "[YOLOv11TensorRT] Warming up engine with " << warmup_iterations << " iterations..." << std::endl;
    
    // 더미 이미지 생성
    cv::Mat dummy_image(settings_.input_height, settings_.input_width, CV_8UC3);
    cv::randu(dummy_image, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));
    
    for (int i = 0; i < warmup_iterations; ++i) {
        DetectMultiple(dummy_image);
    }
    
    std::cout << "[YOLOv11TensorRT] Engine warmup completed" << std::endl;
}

// Private methods implementation

YOLOv11TensorRTInference::BackendType YOLOv11TensorRTInference::SelectOptimalBackend() {
#ifdef HAVE_TENSORRT
    // CUDA 디바이스 확인
    int device_count = 0;
    cudaError_t cuda_status = cudaGetDeviceCount(&device_count);
    
    if (cuda_status == cudaSuccess && device_count > 0) {
        std::cout << "[YOLOv11TensorRT] CUDA devices available: " << device_count << std::endl;
        
        // TensorRT 엔진 파일 존재 여부 확인
        if (std::filesystem::exists(settings_.tensorrt_engine_path)) {
            return BackendType::TENSORRT;
        } else if (std::filesystem::exists(settings_.onnx_model_path)) {
            return BackendType::TENSORRT_FALLBACK;
        }
    } else {
        std::cout << "[YOLOv11TensorRT] No CUDA devices available, using OpenCV DNN" << std::endl;
    }
#endif
    
    return BackendType::OPENCV_DNN;
}

bool YOLOv11TensorRTInference::InitializeTensorRT() {
#ifdef HAVE_TENSORRT
    try {
        // 엔진 빌드 또는 로드
        if (!BuildTensorRTEngine(false)) {
            return false;
        }
        
        if (!LoadTensorRTEngine()) {
            return false;
        }
        
        if (!AllocateCUDAMemory()) {
            return false;
        }
        
        std::cout << "[YOLOv11TensorRT] TensorRT initialized successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[YOLOv11TensorRT] TensorRT initialization failed: " << e.what() << std::endl;
        return false;
    }
#else
    std::cerr << "[YOLOv11TensorRT] TensorRT not available at compile time" << std::endl;
    return false;
#endif
}

bool YOLOv11TensorRTInference::InitializeOpenCVDNN() {
    try {
        std::filesystem::path onnx_path(settings_.onnx_model_path);
        
        if (!std::filesystem::exists(onnx_path)) {
            std::cerr << "[YOLOv11TensorRT] ONNX file not found: " << onnx_path << std::endl;
            return false;
        }
        
        opencv_net_ = cv::dnn::readNet(onnx_path.string());
        
        if (opencv_net_.empty()) {
            std::cerr << "[YOLOv11TensorRT] Failed to load ONNX model with OpenCV DNN" << std::endl;
            return false;
        }
        
        // GPU 백엔드 시도
        try {
            opencv_net_.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            opencv_net_.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
            
            // GPU 테스트를 위한 더미 추론
            cv::Mat dummy_input = cv::Mat::zeros(settings_.input_height, settings_.input_width, CV_8UC3);
            cv::Mat blob = PreprocessImageOpenCV(dummy_input);
            opencv_net_.setInput(blob);
            cv::Mat output = opencv_net_.forward();
            
            std::cout << "[YOLOv11TensorRT] OpenCV DNN initialized with CUDA backend" << std::endl;
            
        } catch (const std::exception&) {
            // GPU 백엔드 실패 시 CPU로 폴백
            opencv_net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            opencv_net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            std::cout << "[YOLOv11TensorRT] OpenCV DNN initialized with CPU backend" << std::endl;
        }
        
        opencv_net_initialized_ = true;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[YOLOv11TensorRT] OpenCV DNN initialization failed: " << e.what() << std::endl;
        opencv_net_initialized_ = false;
        return false;
    }
}

#ifdef HAVE_TENSORRT
bool YOLOv11TensorRTInference::LoadTensorRTEngine() {
    try {
        std::filesystem::path engine_path(settings_.tensorrt_engine_path);
        
        if (!std::filesystem::exists(engine_path)) {
            std::cerr << "[YOLOv11TensorRT] Engine file not found: " << engine_path << std::endl;
            return false;
        }
        
        // 엔진 파일 읽기
        std::ifstream engine_file(engine_path, std::ios::binary);
        if (!engine_file.is_open()) {
            std::cerr << "[YOLOv11TensorRT] Failed to open engine file: " << engine_path << std::endl;
            return false;
        }
        
        engine_file.seekg(0, std::ios::end);
        size_t engine_size = engine_file.tellg();
        engine_file.seekg(0, std::ios::beg);
        
        std::vector<char> engine_data(engine_size);
        engine_file.read(engine_data.data(), engine_size);
        engine_file.close();
        
        // 런타임 생성
        runtime_ = std::unique_ptr<nvinfer1::IRuntime>(nvinfer1::createInferRuntime(trt_logger));
        if (!runtime_) {
            std::cerr << "[YOLOv11TensorRT] Failed to create TensorRT runtime" << std::endl;
            return false;
        }
        
        // 엔진 역직렬화
        engine_ = std::unique_ptr<nvinfer1::ICudaEngine>(
            runtime_->deserializeCudaEngine(engine_data.data(), engine_size, nullptr));
        if (!engine_) {
            std::cerr << "[YOLOv11TensorRT] Failed to deserialize engine" << std::endl;
            return false;
        }
        
        // 실행 컨텍스트 생성
        context_ = std::unique_ptr<nvinfer1::IExecutionContext>(engine_->createExecutionContext());
        if (!context_) {
            std::cerr << "[YOLOv11TensorRT] Failed to create execution context" << std::endl;
            return false;
        }
        
        // 바인딩 정보 수집
        for (int i = 0; i < engine_->getNbBindings(); ++i) {
            auto dims = engine_->getBindingDimensions(i);
            auto dtype = engine_->getBindingDataType(i);
            size_t size = 1;
            for (int j = 0; j < dims.nbDims; ++j) {
                size *= dims.d[j];
            }
            
            if (engine_->bindingIsInput(i)) {
                input_binding_index_ = i;
                input_size_ = size * sizeof(float);
                std::cout << "[YOLOv11TensorRT] Input binding " << i << ": ";
                for (int j = 0; j < dims.nbDims; ++j) {
                    std::cout << dims.d[j];
                    if (j < dims.nbDims - 1) std::cout << "x";
                }
                std::cout << " (" << input_size_ << " bytes)" << std::endl;
            } else {
                output_binding_index_ = i;
                output_size_ = size * sizeof(float);
                std::cout << "[YOLOv11TensorRT] Output binding " << i << ": ";
                for (int j = 0; j < dims.nbDims; ++j) {
                    std::cout << dims.d[j];
                    if (j < dims.nbDims - 1) std::cout << "x";
                }
                std::cout << " (" << output_size_ << " bytes)" << std::endl;
            }
        }
        
        std::cout << "[YOLOv11TensorRT] Engine loaded successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[YOLOv11TensorRT] Failed to load engine: " << e.what() << std::endl;
        return false;
    }
}

bool YOLOv11TensorRTInference::AllocateCUDAMemory() {
    try {
        // CUDA 스트림 생성
        cudaError_t status = cudaStreamCreate(&cuda_stream_);
        if (status != cudaSuccess) {
            std::cerr << "[YOLOv11TensorRT] Failed to create CUDA stream: " << cudaGetErrorString(status) << std::endl;
            return false;
        }
        
        // GPU 메모리 할당
        status = cudaMalloc(&gpu_input_buffer_, input_size_);
        if (status != cudaSuccess) {
            std::cerr << "[YOLOv11TensorRT] Failed to allocate GPU input memory: " << cudaGetErrorString(status) << std::endl;
            return false;
        }
        
        status = cudaMalloc(&gpu_output_buffer_, output_size_);
        if (status != cudaSuccess) {
            std::cerr << "[YOLOv11TensorRT] Failed to allocate GPU output memory: " << cudaGetErrorString(status) << std::endl;
            return false;
        }
        
        // CPU 출력 버퍼 할당
        cpu_output_buffer_ = malloc(output_size_);
        if (!cpu_output_buffer_) {
            std::cerr << "[YOLOv11TensorRT] Failed to allocate CPU output memory" << std::endl;
            return false;
        }
        
        std::cout << "[YOLOv11TensorRT] CUDA memory allocated successfully" << std::endl;
        std::cout << "  - Input buffer: " << input_size_ / (1024 * 1024) << " MB" << std::endl;
        std::cout << "  - Output buffer: " << output_size_ / (1024 * 1024) << " MB" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[YOLOv11TensorRT] CUDA memory allocation failed: " << e.what() << std::endl;
        return false;
    }
}

void YOLOv11TensorRTInference::FreeCUDAMemory() {
    if (gpu_input_buffer_) {
        cudaFree(gpu_input_buffer_);
        gpu_input_buffer_ = nullptr;
    }
    
    if (gpu_output_buffer_) {
        cudaFree(gpu_output_buffer_);
        gpu_output_buffer_ = nullptr;
    }
    
    if (cpu_output_buffer_) {
        free(cpu_output_buffer_);
        cpu_output_buffer_ = nullptr;
    }
    
    if (cuda_stream_) {
        cudaStreamDestroy(cuda_stream_);
        cuda_stream_ = nullptr;
    }
    
    std::cout << "[YOLOv11TensorRT] CUDA memory freed" << std::endl;
}
#endif

bool YOLOv11TensorRTInference::LoadClassNames(const std::string& class_names_path) {
    try {
        std::filesystem::path names_path(class_names_path);
        
        if (std::filesystem::exists(names_path)) {
            std::ifstream file(names_path);
            if (!file.is_open()) {
                std::cerr << "[YOLOv11TensorRT] Failed to open class names file: " << names_path << std::endl;
                return false;
            }
            
            std::string line;
            class_names_.clear();
            while (std::getline(file, line)) {
                if (!line.empty()) {
                    class_names_.push_back(line);
                }
            }
            file.close();
            
            std::cout << "[YOLOv11TensorRT] Loaded " << class_names_.size() << " class names from: " << names_path << std::endl;
        } else {
            // COCO 클래스 이름 기본값
            class_names_ = {
                "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck",
                "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench",
                "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra",
                "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
                "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove",
                "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup",
                "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
                "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
                "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse",
                "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink",
                "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
            };
            
            std::cout << "[YOLOv11TensorRT] Using default COCO class names (" << class_names_.size() << " classes)" << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[YOLOv11TensorRT] Failed to load class names: " << e.what() << std::endl;
        return false;
    }
}

std::vector<IDetectionAlgorithm::DetectionResult> YOLOv11TensorRTInference::InferTensorRT(const cv::Mat& image) {
#ifdef HAVE_TENSORRT
    // TensorRT 추론 구현은 복잡하므로 기본 구조만 제공
    // 실제 구현에서는 CUDA 커널을 사용한 전처리, 추론, 후처리가 필요
    std::cerr << "[YOLOv11TensorRT] TensorRT inference not yet fully implemented" << std::endl;
    return std::vector<DetectionResult>();
#else
    return std::vector<DetectionResult>();
#endif
}

std::vector<IDetectionAlgorithm::DetectionResult> YOLOv11TensorRTInference::InferOpenCVDNN(const cv::Mat& image) {
    if (!opencv_net_initialized_) {
        return std::vector<DetectionResult>();
    }
    
    try {
        // 전처리
        cv::Mat blob = PreprocessImageOpenCV(image);
        
        // 추론
        opencv_net_.setInput(blob);
        cv::Mat output = opencv_net_.forward();
        
        // 후처리
        float* output_data = (float*)output.data;
        return PostprocessOutput(output_data, cv::Size(image.cols, image.rows));
        
    } catch (const std::exception& e) {
        std::cerr << "[YOLOv11TensorRT] OpenCV DNN inference failed: " << e.what() << std::endl;
        return std::vector<DetectionResult>();
    }
}

cv::Mat YOLOv11TensorRTInference::PreprocessImageOpenCV(const cv::Mat& image) {
    // YOLOv11 입력 형식으로 전처리
    cv::Mat blob;
    cv::dnn::blobFromImage(image, blob, 1.0/255.0, 
                          cv::Size(settings_.input_width, settings_.input_height), 
                          cv::Scalar(0, 0, 0), true, false, CV_32F);
    return blob;
}

std::vector<IDetectionAlgorithm::DetectionResult> YOLOv11TensorRTInference::PostprocessOutput(
    const float* output_data, const cv::Size& image_size) {
    
    std::vector<DetectionResult> detections;
    
    // YOLOv11 출력 포맷에 따른 후처리
    // 실제 구현에서는 모델 출력 구조에 맞게 수정 필요
    
    return ApplyNMS(detections);
}

std::vector<IDetectionAlgorithm::DetectionResult> YOLOv11TensorRTInference::ApplyNMS(
    const std::vector<DetectionResult>& detections) {
    
    if (detections.size() <= 1) {
        return detections;
    }
    
    // OpenCV NMS 적용
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<int> class_ids;
    
    for (const auto& detection : detections) {
        boxes.push_back(detection.bounding_box);
        scores.push_back(static_cast<float>(detection.confidence));
        class_ids.push_back(detection.class_id);
    }
    
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, scores, 
                     static_cast<float>(settings_.confidence_threshold), 
                     settings_.nms_threshold, indices);
    
    std::vector<DetectionResult> nms_results;
    for (int idx : indices) {
        if (idx >= 0 && idx < static_cast<int>(detections.size())) {
            nms_results.push_back(detections[idx]);
        }
    }
    
    // 최대 감지 수 제한
    if (nms_results.size() > static_cast<size_t>(settings_.max_detections)) {
        nms_results.resize(settings_.max_detections);
    }
    
    return nms_results;
}

void YOLOv11TensorRTInference::UpdatePerformanceMetrics(double inference_time) const {
    uint64_t current_inferences = total_inferences_.fetch_add(1) + 1;
    double current_total = total_inference_time_ms_.fetch_add(inference_time) + inference_time;
    double new_average = current_total / current_inferences;
    
    average_inference_time_ms_.store(new_average);
    last_inference_time_ = std::chrono::high_resolution_clock::now();
}

bool YOLOv11TensorRTInference::ValidateSettings(const TensorRTSettings& settings) const {
    if (settings.confidence_threshold < Constants::Vision::MIN_CONFIDENCE ||
        settings.confidence_threshold > Constants::Vision::MAX_CONFIDENCE) {
        return false;
    }
    
    if (settings.nms_threshold < 0.0f || settings.nms_threshold > 1.0f) {
        return false;
    }
    
    if (settings.input_width <= 0 || settings.input_height <= 0) {
        return false;
    }
    
    if (settings.max_detections <= 0 || settings.max_detections > Constants::Vision::MAX_DETECTION_RESULTS) {
        return false;
    }
    
    return true;
}

cv::Rect YOLOv11TensorRTInference::ConvertYOLOCoordinates(float x, float y, float w, float h, 
                                                         int img_width, int img_height) {
    // YOLO 형식 (중심점, 정규화) → OpenCV 형식 (좌상단, 픽셀)
    float x1 = (x - w / 2) * img_width;
    float y1 = (y - h / 2) * img_height;
    float width = w * img_width;
    float height = h * img_height;
    
    // 경계 검사
    x1 = std::max(0.0f, std::min(x1, static_cast<float>(img_width)));
    y1 = std::max(0.0f, std::min(y1, static_cast<float>(img_height)));
    width = std::min(width, static_cast<float>(img_width) - x1);
    height = std::min(height, static_cast<float>(img_height) - y1);
    
    return cv::Rect(static_cast<int>(x1), static_cast<int>(y1), 
                   static_cast<int>(width), static_cast<int>(height));
}

bool YOLOv11TensorRTInference::HandleErrorAndFallback(const std::string& error_message) {
    std::cerr << "[YOLOv11TensorRT] Error: " << error_message << std::endl;
    
    if (current_backend_ == BackendType::TENSORRT || current_backend_ == BackendType::TENSORRT_FALLBACK) {
        std::cout << "[YOLOv11TensorRT] Attempting fallback to OpenCV DNN..." << std::endl;
        
        if (InitializeOpenCVDNN()) {
            current_backend_ = BackendType::OPENCV_DNN;
            std::cout << "[YOLOv11TensorRT] Fallback successful" << std::endl;
            return true;
        }
    }
    
    std::cerr << "[YOLOv11TensorRT] Fallback failed" << std::endl;
    return false;
}