#include "detection/ObjectDetector.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <fstream>
#include <iostream>

ObjectDetector::ObjectDetector()
#ifdef ONNX_GPU_ONLY
    : backend_type_(YOLOBackend::ONNX_RUNTIME_GPU)  // GPU 전용 모드
    , using_gpu_(true)                              // GPU 강제 활성화
#else
    : backend_type_(YOLOBackend::OPENCV_DNN)        // 기본 CPU 모드
    , using_gpu_(false)
#endif
    , confidence_threshold_(0.25f)  // YOLO v11 권장값
    , nms_threshold_(0.45f)         // YOLO v11 권장값
    , input_size_(640, 640)         // YOLO v11 기본 크기
    , model_loaded_(false)
    , processing_time_ms_(0.0)
    , frame_count_(0)
    , avg_fps_(0.0)
{
#ifdef HAVE_ONNXRUNTIME
    ort_env_ = nullptr;
    ort_session_ = nullptr;
    ort_session_options_ = nullptr;
#endif
    
    // COCO 데이터셋 클래스 이름 초기화
    initializeCOCOClasses();
    last_process_time_ = std::chrono::high_resolution_clock::now();
}

bool ObjectDetector::loadModel(const std::string& model_path, const std::string& config_path)
{
#ifdef HAVE_OPENCV_DNN
    try {
        if (!config_path.empty()) {
            net_ = cv::dnn::readNetFromDarknet(config_path, model_path);
        } else {
            // ONNX, TensorFlow 등 다른 형식 지원
            net_ = cv::dnn::readNet(model_path);
        }
        
        if (net_.empty()) {
            model_loaded_ = false;
            return false;
        }
        
        // GPU 사용 가능 시 설정
        if (cv::dnn::DNN_BACKEND_CUDA) {
            net_.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            net_.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
        }
        
        model_loaded_ = true;
        return true;
    }
    catch (const cv::Exception& e) {
        model_loaded_ = false;
        return false;
    }
#else
    // DNN 모듈이 없는 경우 Mock 동작
    model_loaded_ = false;
    return false;
#endif
}

cv::Rect ObjectDetector::detectTarget(const cv::Mat& image) 
{
    if (image.empty()) {
        return cv::Rect();
    }
    
    auto results = detectMultipleTargets(image);
    
    if (results.empty()) {
        return cv::Rect();
    }
    
    // 가장 높은 신뢰도의 객체 반환
    auto best_result = std::max_element(results.begin(), results.end(),
        [](const DetectionResult& a, const DetectionResult& b) {
            return a.confidence < b.confidence;
        });
    
    return best_result->bbox;
}

std::vector<ObjectDetector::DetectionResult> ObjectDetector::detectMultipleTargets(const cv::Mat& image)
{
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (image.empty()) {
        return std::vector<DetectionResult>();
    }
    
    if (!model_loaded_) {
        return std::vector<DetectionResult>();
    }
    
    std::vector<DetectionResult> results;
    
#ifdef ONNX_GPU_ONLY
    // GPU 전용 모드: ONNX Runtime만 사용
    if (backend_type_ == YOLOBackend::ONNX_RUNTIME_GPU) {
#else
    // 일반 모드: OpenCV DNN도 지원
    if (backend_type_ == YOLOBackend::OPENCV_DNN) {
#ifdef HAVE_OPENCV_DNN
        // YOLO v11 OpenCV DNN 추론
        cv::Mat blob = preprocessYOLOv11(image);
        net_.setInput(blob);
        
        std::vector<cv::Mat> outputs;
        net_.forward(outputs, net_.getUnconnectedOutLayersNames());
        
        results = postprocessYOLOv11OpenCV(outputs, image.size());
#endif
    } else if (backend_type_ == YOLOBackend::ONNX_RUNTIME_GPU) {
#endif
#ifdef HAVE_ONNXRUNTIME
        // YOLO v11 ONNX Runtime 추론
        cv::Mat preprocessed = preprocessYOLOv11(image);
        
        // OpenCV Mat을 float 벡터로 변환
        std::vector<float> input_tensor;
        if (preprocessed.isContinuous()) {
            input_tensor.assign((float*)preprocessed.data, 
                              (float*)preprocessed.data + preprocessed.total() * preprocessed.channels());
        }
        
        auto onnx_outputs = runONNXInference(input_tensor);
        results = postprocessYOLOv11ONNX(onnx_outputs, image.size());
#endif
    }
    
    updatePerformanceStats();
    return results;
}

std::vector<ObjectDetector::DetectionResult> ObjectDetector::detectSpecificClasses(
    const cv::Mat& image, const std::vector<std::string>& target_classes)
{
    auto all_results = detectMultipleTargets(image);
    std::vector<DetectionResult> filtered_results;
    
    for (const auto& result : all_results) {
        if (std::find(target_classes.begin(), target_classes.end(), result.label) != target_classes.end()) {
            filtered_results.push_back(result);
        }
    }
    
    return filtered_results;
}

void ObjectDetector::setConfidenceThreshold(float threshold)
{
    confidence_threshold_ = std::max(0.0f, std::min(1.0f, threshold));
}

void ObjectDetector::setNMSThreshold(float threshold)
{
    nms_threshold_ = std::max(0.0f, std::min(1.0f, threshold));
}

void ObjectDetector::setInputSize(const cv::Size& size)
{
    input_size_ = size;
}

std::string ObjectDetector::getPerformanceStats() const
{
    std::stringstream ss;
    ss << "ObjectDetector Performance: "
       << "FPS=" << avg_fps_
       << ", ProcessTime=" << processing_time_ms_ << "ms"
       << ", Frames=" << frame_count_
       << ", ModelLoaded=" << (model_loaded_ ? "Yes" : "No");
    return ss.str();
}

void ObjectDetector::reset()
{
    model_loaded_ = false;
    using_gpu_ = false;
    frame_count_ = 0;
    avg_fps_ = 0.0;
    processing_time_ms_ = 0.0;
    last_process_time_ = std::chrono::high_resolution_clock::now();
    
#ifdef HAVE_ONNXRUNTIME
    // ONNX Runtime 리소스 정리
    ort_session_.reset();
    ort_session_options_.reset();
    ort_env_.reset();
    input_names_.clear();
    output_names_.clear();
    input_shape_.clear();
    output_shape_.clear();
#endif
}

void ObjectDetector::initializeCOCOClasses()
{
    class_names_ = {
        "person", "bicycle", "car", "motorbike", "aeroplane", "bus", "train", "truck",
        "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench",
        "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra",
        "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
        "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove",
        "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup",
        "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
        "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "sofa",
        "pottedplant", "bed", "diningtable", "toilet", "tvmonitor", "laptop", "mouse",
        "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink",
        "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier",
        "toothbrush"
    };
}

cv::Mat ObjectDetector::preprocessImage(const cv::Mat& image)
{
#ifdef HAVE_OPENCV_DNN
    cv::Mat blob;
    cv::dnn::blobFromImage(image, blob, 1/255.0, input_size_, cv::Scalar(0,0,0), true, false);
    return blob;
#else
    // Mock: 원본 이미지 반환
    return image.clone();
#endif
}

std::vector<ObjectDetector::DetectionResult> ObjectDetector::postprocessOutputs(
    const std::vector<cv::Mat>& outputs, const cv::Size& image_size)
{
#ifdef HAVE_OPENCV_DNN
    std::vector<DetectionResult> results;
    std::vector<int> class_ids;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;
    
    // 각 출력 레이어 처리
    for (const auto& output : outputs) {
        for (int i = 0; i < output.rows; ++i) {
            const float* data = output.ptr<float>(i);
            
            // 객체 신뢰도
            float objectness = data[4];
            if (objectness < confidence_threshold_) continue;
            
            // 클래스 확률 찾기
            cv::Mat scores = output.row(i).colRange(5, output.cols);
            cv::Point class_id_point;
            double max_class_score;
            cv::minMaxLoc(scores, 0, &max_class_score, 0, &class_id_point);
            
            float confidence = objectness * max_class_score;
            if (confidence < confidence_threshold_) continue;
            
            // 경계 상자 계산
            float center_x = data[0] * image_size.width;
            float center_y = data[1] * image_size.height;
            float width = data[2] * image_size.width;
            float height = data[3] * image_size.height;
            
            int left = static_cast<int>(center_x - width / 2);
            int top = static_cast<int>(center_y - height / 2);
            
            class_ids.push_back(class_id_point.x);
            confidences.push_back(confidence);
            boxes.push_back(cv::Rect(left, top, static_cast<int>(width), static_cast<int>(height)));
        }
    }
    
    // NMS 적용
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confidence_threshold_, nms_threshold_, indices);
    
    // 최종 결과 생성
    for (int idx : indices) {
        std::string label = (class_ids[idx] < class_names_.size()) ? 
                           class_names_[class_ids[idx]] : "unknown";
        results.emplace_back(boxes[idx], confidences[idx], label, class_ids[idx]);
    }
    
    return results;
#else
    // Mock: 빈 결과 반환
    return std::vector<DetectionResult>();
#endif
}

void ObjectDetector::updatePerformanceStats() const
{
    auto current_time = std::chrono::high_resolution_clock::now();
    
    // 처리 시간 계산
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        current_time - last_process_time_);
    processing_time_ms_ = duration.count() / 1000.0;
    
    // FPS 계산
    frame_count_++;
    if (frame_count_ > 1) {
        double time_diff_sec = duration.count() / 1000000.0;
        if (time_diff_sec > 0) {
            double current_fps = 1.0 / time_diff_sec;
            avg_fps_ = (avg_fps_ * (frame_count_ - 1) + current_fps) / frame_count_;
        }
    }
    
    last_process_time_ = current_time;
}

int ObjectDetector::findClassId(const std::string& class_name) const
{
    auto it = std::find(class_names_.begin(), class_names_.end(), class_name);
    return (it != class_names_.end()) ? std::distance(class_names_.begin(), it) : -1;
}

std::string ObjectDetector::getModelInfo() const
{
    if (model_loaded_) {
        return "YOLO Model Loaded (Mock)";
    } else {
        return "No Model Loaded";
    }
}

bool ObjectDetector::setUseGPU(bool use_gpu)
{
#ifdef HAVE_OPENCV_DNN
    using_gpu_ = use_gpu;
    if (model_loaded_ && use_gpu) {
        try {
            net_.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            net_.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
            return true;
        } catch (...) {
            using_gpu_ = false;
            return false;
        }
    }
    return !use_gpu; // GPU를 사용하지 않으려 할 때는 성공
#else
    // Mock: GPU 사용 불가
    using_gpu_ = false;
    return false;
#endif
}

bool ObjectDetector::isUsingGPU() const
{
    return using_gpu_;
}

std::string ObjectDetector::getGPUStatus() const
{
    std::stringstream ss;
    
#ifdef ONNX_GPU_ONLY
    ss << "GPU 전용 모드: ";
#ifdef HAVE_DIRECTML
    ss << "DirectML 지원 (Windows DirectX 12)";
#else
    ss << "DirectML 미지원, CUDA 시도";
#endif
    
    if (model_loaded_) {
        ss << " - 모델 로드됨";
    } else {
        ss << " - 모델 미로드";
    }
    
    if (using_gpu_) {
        ss << " - GPU 활성화";
    } else {
        ss << " - GPU 비활성화 (CPU 폴백)";
    }
#else
    ss << "일반 모드: ";
    if (using_gpu_) {
        ss << "GPU acceleration enabled";
    } else {
        ss << "CPU mode";
    }
#endif
    
    return ss.str();
}

// YOLO v11 특화 구현

bool ObjectDetector::loadYOLOv11Model(const std::string& onnx_path, YOLOBackend backend)
{
    reset();
    
#ifdef ONNX_GPU_ONLY
    // GPU 전용 모드: ONNX Runtime GPU만 허용
    if (backend != YOLOBackend::ONNX_RUNTIME_GPU) {
        std::cerr << "GPU 전용 모드: ONNX Runtime GPU만 지원됩니다." << std::endl;
        return false;
    }
    backend_type_ = YOLOBackend::ONNX_RUNTIME_GPU;
    using_gpu_ = true;  // GPU 강제 활성화
#else
    backend_type_ = backend;
#endif
    
#ifndef ONNX_GPU_ONLY
    if (backend_type_ == YOLOBackend::OPENCV_DNN) {
#ifdef HAVE_OPENCV_DNN
        try {
            net_ = cv::dnn::readNet(onnx_path);
            if (net_.empty()) {
                return false;
            }
            
            // GPU 사용 설정
            if (using_gpu_) {
                net_.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
                net_.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
            } else {
                net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
                net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            }
            
            model_loaded_ = true;
            return true;
        } catch (const cv::Exception& e) {
            return false;
        }
#else
        return false;
#endif
    } else 
#endif
    if (backend_type_ == YOLOBackend::ONNX_RUNTIME_GPU) {
#ifdef HAVE_ONNXRUNTIME
        return initONNXRuntime(onnx_path);
#else
        std::cerr << "ONNX Runtime이 설치되지 않았습니다." << std::endl;
        return false;
#endif
    }
    
    return false;
}

bool ObjectDetector::loadClassNames(const std::string& class_names_path)
{
    std::ifstream file(class_names_path);
    if (!file.is_open()) {
        return false;
    }
    
    class_names_.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            class_names_.push_back(line);
        }
    }
    
    return !class_names_.empty();
}

bool ObjectDetector::setBackend(YOLOBackend backend)
{
    if (backend_type_ == backend) {
        return true;
    }
    
    // 백엔드 변경 시 모델 재로드 필요
    model_loaded_ = false;
    backend_type_ = backend;
    return true;
}

cv::Mat ObjectDetector::preprocessYOLOv11(const cv::Mat& image)
{
    if (backend_type_ == YOLOBackend::OPENCV_DNN) {
#ifdef HAVE_OPENCV_DNN
        cv::Mat blob;
        cv::dnn::blobFromImage(image, blob, 1.0/255.0, input_size_, 
                              cv::Scalar(), true, false, CV_32F);
        return blob;
#endif
    }
    
    // ONNX Runtime용 전처리
    cv::Mat resized, normalized;
    cv::resize(image, resized, input_size_);
    resized.convertTo(normalized, CV_32F, 1.0/255.0);
    
    return normalized;
}

std::vector<ObjectDetector::DetectionResult> ObjectDetector::postprocessYOLOv11OpenCV(
    const std::vector<cv::Mat>& outputs, const cv::Size& image_size)
{
#ifdef HAVE_OPENCV_DNN
    std::vector<DetectionResult> results;
    
    if (outputs.empty()) {
        return results;
    }
    
    const cv::Mat& output = outputs[0];
    const int num_detections = output.size[1];
    const int num_classes = output.size[2] - 4;  // 좌표 4개 제외
    
    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> class_ids;
    
    for (int i = 0; i < num_detections; ++i) {
        const float* data = output.ptr<float>(0, i);
        
        // YOLO v11 형식: [cx, cy, w, h, conf_class0, conf_class1, ...]
        float cx = data[0];
        float cy = data[1];
        float w = data[2];
        float h = data[3];
        
        // 가장 높은 클래스 신뢰도 찾기
        float max_confidence = 0.0f;
        int best_class_id = -1;
        
        for (int c = 0; c < num_classes; ++c) {
            float class_confidence = data[4 + c];
            if (class_confidence > max_confidence) {
                max_confidence = class_confidence;
                best_class_id = c;
            }
        }
        
        if (max_confidence > confidence_threshold_) {
            cv::Rect box = convertYOLOv11Box(cx, cy, w, h, image_size.width, image_size.height);
            
            boxes.push_back(box);
            confidences.push_back(max_confidence);
            class_ids.push_back(best_class_id);
        }
    }
    
    // NMS 적용
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confidence_threshold_, nms_threshold_, indices);
    
    // 최종 결과 생성
    for (int idx : indices) {
        std::string label = (class_ids[idx] < class_names_.size()) ? 
                           class_names_[class_ids[idx]] : "unknown";
        results.emplace_back(boxes[idx], confidences[idx], label, class_ids[idx]);
    }
    
    return results;
#else
    return std::vector<DetectionResult>();
#endif
}

cv::Rect ObjectDetector::convertYOLOv11Box(float cx, float cy, float w, float h,
                                          int img_width, int img_height)
{
    // YOLO v11 출력은 이미 픽셀 좌표
    int x = static_cast<int>(cx - w / 2);
    int y = static_cast<int>(cy - h / 2);
    int width = static_cast<int>(w);
    int height = static_cast<int>(h);
    
    // 이미지 경계 내로 제한
    x = std::max(0, std::min(x, img_width - 1));
    y = std::max(0, std::min(y, img_height - 1));
    width = std::min(width, img_width - x);
    height = std::min(height, img_height - y);
    
    return cv::Rect(x, y, width, height);
}

#ifdef HAVE_ONNXRUNTIME
bool ObjectDetector::initONNXRuntime(const std::string& model_path)
{
    try {
        // ONNX Runtime 환경 초기화
        ort_env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "YOLOv11");
        
        // 세션 옵션 설정
        ort_session_options_ = std::make_unique<Ort::SessionOptions>();
        ort_session_options_->SetIntraOpNumThreads(1);
        ort_session_options_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        
#ifdef ONNX_GPU_ONLY
        // DirectML GPU 전용 설정 (Windows)
#ifdef HAVE_DIRECTML
        try {
            ort_session_options_->AppendExecutionProvider_DML(0);  // Device 0 사용
            std::cout << "DirectML GPU Provider 활성화됨" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "DirectML Provider 초기화 실패, CPU 사용: " << e.what() << std::endl;
            // DirectML 실패 시 CPU fallback
        }
#else
        // DirectML 미지원 시 CUDA 시도
        if (using_gpu_) {
            try {
                OrtCUDAProviderOptions cuda_options;
                ort_session_options_->AppendExecutionProvider_CUDA(cuda_options);
                std::cout << "CUDA GPU Provider 활성화됨" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "CUDA Provider 초기화 실패: " << e.what() << std::endl;
            }
        }
#endif
#else
        // 기본 모드: GPU 설정
        if (using_gpu_) {
            OrtCUDAProviderOptions cuda_options;
            ort_session_options_->AppendExecutionProvider_CUDA(cuda_options);
        }
#endif
        
        // 세션 생성
        ort_session_ = std::make_unique<Ort::Session>(*ort_env_, model_path.c_str(), *ort_session_options_);
        
        // 입출력 정보 획득
        Ort::AllocatorWithDefaultOptions allocator;
        
        // 입력 이름
        input_names_.clear();
        size_t num_input_nodes = ort_session_->GetInputCount();
        for (size_t i = 0; i < num_input_nodes; ++i) {
            auto input_name = ort_session_->GetInputNameAllocated(i, allocator);
            input_names_.push_back(input_name.get());
        }
        
        // 출력 이름
        output_names_.clear();
        size_t num_output_nodes = ort_session_->GetOutputCount();
        for (size_t i = 0; i < num_output_nodes; ++i) {
            auto output_name = ort_session_->GetOutputNameAllocated(i, allocator);
            output_names_.push_back(output_name.get());
        }
        
        // 입력 형태 정보
        auto input_shape_info = ort_session_->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo();
        input_shape_ = input_shape_info.GetShape();
        
        model_loaded_ = true;
        return true;
        
    } catch (const Ort::Exception& e) {
        return false;
    }
}

std::vector<std::vector<float>> ObjectDetector::runONNXInference(const std::vector<float>& input_tensor)
{
    if (!ort_session_ || input_tensor.empty()) {
        return {};
    }
    
    try {
        // 입력 텐서 생성
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<Ort::Value> input_values;
        
        input_values.emplace_back(Ort::Value::CreateTensor<float>(
            memory_info, const_cast<float*>(input_tensor.data()), input_tensor.size(),
            input_shape_.data(), input_shape_.size()));
        
        // 추론 실행
        auto output_values = ort_session_->Run(Ort::RunOptions{nullptr}, 
                                             input_names_.data(), input_values.data(), 
                                             input_values.size(),
                                             output_names_.data(), output_names_.size());
        
        // 출력 결과 변환
        std::vector<std::vector<float>> results;
        for (auto& output_value : output_values) {
            auto* float_array = output_value.GetTensorMutableData<float>();
            auto shape = output_value.GetTensorTypeAndShapeInfo().GetShape();
            
            size_t total_size = 1;
            for (auto dim : shape) {
                total_size *= dim;
            }
            
            results.emplace_back(float_array, float_array + total_size);
        }
        
        return results;
        
    } catch (const Ort::Exception& e) {
        return {};
    }
}

std::vector<ObjectDetector::DetectionResult> ObjectDetector::postprocessYOLOv11ONNX(
    const std::vector<std::vector<float>>& outputs, const cv::Size& image_size)
{
    std::vector<DetectionResult> results;
    
    if (outputs.empty()) {
        return results;
    }
    
    const auto& output = outputs[0];
    // YOLO v11 출력 형태 처리 (구체적인 형태는 모델에 따라 다를 수 있음)
    
    // 간단한 구현 - 실제로는 모델의 출력 형태에 맞게 조정 필요
    
    return results;
}
#endif

