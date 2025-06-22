#include "detection/ObjectDetector.h"
#include <algorithm>
#include <chrono>
#include <sstream>

ObjectDetector::ObjectDetector()
    : confidence_threshold_(0.5f)
    , nms_threshold_(0.4f)
    , input_size_(416, 416)
    , model_loaded_(false)
    , processing_time_ms_(0.0)
    , frame_count_(0)
    , avg_fps_(0.0)
{
    // COCO 데이터셋 클래스 이름 초기화
    initializeCOCOClasses();
    last_process_time_ = std::chrono::high_resolution_clock::now();
}

bool ObjectDetector::loadModel(const std::string& model_path, const std::string& config_path)
{
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
}

cv::Rect ObjectDetector::detectTarget(const cv::Mat& image) 
{
    if (image.empty() || !model_loaded_) {
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
    
    if (image.empty() || !model_loaded_) {
        return std::vector<DetectionResult>();
    }
    
    // 이미지 전처리
    cv::Mat blob = preprocessImage(image);
    net_.setInput(blob);
    
    // 네트워크 실행
    std::vector<cv::Mat> outputs;
    net_.forward(outputs, net_.getUnconnectedOutLayersNames());
    
    // 후처리
    auto results = postprocessOutputs(outputs, image.size());
    
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
    frame_count_ = 0;
    avg_fps_ = 0.0;
    processing_time_ms_ = 0.0;
    last_process_time_ = std::chrono::high_resolution_clock::now();
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
    cv::Mat blob;
    cv::dnn::blobFromImage(image, blob, 1/255.0, input_size_, cv::Scalar(0,0,0), true, false);
    return blob;
}

std::vector<ObjectDetector::DetectionResult> ObjectDetector::postprocessOutputs(
    const std::vector<cv::Mat>& outputs, const cv::Size& image_size)
{
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