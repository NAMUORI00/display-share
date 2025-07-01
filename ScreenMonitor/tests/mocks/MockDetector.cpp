#include "MockDetector.h"
#include <random>
#include <algorithm>

using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;

// MockColorDetector 구현
MockColorDetector::MockColorDetector() {
    m_mock_detection = cv::Rect(100, 100, 50, 50);
    m_mock_range = ColorDetector::HSVRange(140, 120, 180, 160, 200, 255);
    SetupDefaultBehavior();
}

void MockColorDetector::SetMockDetectionResult(const cv::Rect& result) {
    m_mock_detection = result;
}

void MockColorDetector::SetMockDetectionResults(const std::vector<ColorDetector::DetectionResult>& results) {
    m_mock_results = results;
}

void MockColorDetector::SetDetectionSuccessRate(double success_rate) {
    m_success_rate = std::max(0.0, std::min(1.0, success_rate));
}

void MockColorDetector::SimulateNoDetection() {
    m_mock_detection = cv::Rect();
    m_mock_results.clear();
}

void MockColorDetector::SimulateMultipleDetections(int count) {
    m_mock_results = GenerateRandomDetections(count);
}

void MockColorDetector::SetupDefaultBehavior() {
    ON_CALL(*this, detectTarget(_))
        .WillByDefault(Invoke([this](const cv::Mat& image) -> cv::Rect {
            m_detection_call_count++;
            if (image.empty() || !ShouldDetect()) {
                return cv::Rect();
            }
            return m_mock_detection;
        }));
    
    ON_CALL(*this, detectMultipleTargets(_))
        .WillByDefault(Invoke([this](const cv::Mat& image) -> std::vector<ColorDetector::DetectionResult> {
            m_detection_call_count++;
            if (image.empty() || !ShouldDetect()) {
                return {};
            }
            return m_mock_results;
        }));
    
    ON_CALL(*this, setColorRange(_))
        .WillByDefault(Invoke([this](const ColorDetector::HSVRange& range) {
            m_mock_range = range;
            m_configured = true;
        }));
    
    ON_CALL(*this, setColorRange(_, _))
        .WillByDefault(Invoke([this](const cv::Scalar& lower, const cv::Scalar& upper) {
            m_mock_range.h_min = static_cast<int>(lower[0]);
            m_mock_range.s_min = static_cast<int>(lower[1]);
            m_mock_range.v_min = static_cast<int>(lower[2]);
            m_mock_range.h_max = static_cast<int>(upper[0]);
            m_mock_range.s_max = static_cast<int>(upper[1]);
            m_mock_range.v_max = static_cast<int>(upper[2]);
            m_configured = true;
        }));
    
    ON_CALL(*this, getColorRange())
        .WillByDefault(Return(m_mock_range));
    
    ON_CALL(*this, initialize())
        .WillByDefault(Invoke([this]() {
            m_configured = true;
            return true;
        }));
    
    ON_CALL(*this, reset())
        .WillByDefault(Invoke([this]() {
            m_detection_call_count = 0;
        }));
    
    ON_CALL(*this, getAlgorithmName())
        .WillByDefault(Return("MockHSVDetector"));
    
    ON_CALL(*this, isConfigured())
        .WillByDefault(Invoke([this]() { return m_configured; }));
    
    ON_CALL(*this, getPerformanceStats())
        .WillByDefault(Return("Mock Performance Stats: Calls=" + std::to_string(m_detection_call_count)));
}

void MockColorDetector::SetupFailureScenario() {
    m_configured = false;
    m_success_rate = 0.0;
    
    ON_CALL(*this, initialize())
        .WillByDefault(Return(false));
    
    ON_CALL(*this, isConfigured())
        .WillByDefault(Return(false));
}

void MockColorDetector::SetupPerformanceScenario() {
    m_success_rate = 0.95;
    m_configured = true;
    
    // 다양한 크기의 검출 결과 생성
    std::vector<ColorDetector::DetectionResult> results;
    results.emplace_back(cv::Rect(50, 50, 30, 30), 0.9, "small_object");
    results.emplace_back(cv::Rect(200, 150, 60, 60), 0.85, "medium_object");
    results.emplace_back(cv::Rect(400, 300, 100, 100), 0.95, "large_object");
    
    SetMockDetectionResults(results);
}

bool MockColorDetector::ShouldDetect() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    return dis(gen) < m_success_rate;
}

cv::Rect MockColorDetector::GenerateRandomDetection() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> pos_dis(0, 500);
    std::uniform_int_distribution<> size_dis(20, 100);
    
    int x = pos_dis(gen);
    int y = pos_dis(gen);
    int w = size_dis(gen);
    int h = size_dis(gen);
    
    return cv::Rect(x, y, w, h);
}

std::vector<ColorDetector::DetectionResult> MockColorDetector::GenerateRandomDetections(int count) {
    std::vector<ColorDetector::DetectionResult> results;
    
    for (int i = 0; i < count; ++i) {
        cv::Rect rect = GenerateRandomDetection();
        double confidence = 0.7 + (static_cast<double>(rand()) / RAND_MAX) * 0.3;
        std::string label = "object_" + std::to_string(i);
        
        results.emplace_back(rect, confidence, label);
    }
    
    return results;
}

// MockObjectDetector 구현
MockObjectDetector::MockObjectDetector() {
    SetupDefaultBehavior();
}

void MockObjectDetector::SetMockDetectionResults(const std::vector<ObjectDetector::DetectionResult>& results) {
    m_mock_results = results;
}

void MockObjectDetector::SetDetectionSuccessRate(double success_rate) {
    m_success_rate = std::max(0.0, std::min(1.0, success_rate));
}

void MockObjectDetector::SimulateModelLoaded(bool loaded) {
    m_model_loaded = loaded;
}

void MockObjectDetector::SimulateGPUAvailable(bool available) {
    m_gpu_available = available;
}

void MockObjectDetector::SetupDefaultBehavior() {
    ON_CALL(*this, loadModel(_, _))
        .WillByDefault(Invoke([this](const std::string& model_path, const std::string& config_path) {
            // 파일 존재 시뮬레이션
            m_model_loaded = !model_path.empty() && !config_path.empty();
            return m_model_loaded;
        }));
    
    ON_CALL(*this, detectMultipleTargets(_))
        .WillByDefault(Invoke([this](const cv::Mat& image) -> std::vector<ObjectDetector::DetectionResult> {
            if (image.empty() || !m_model_loaded || !ShouldDetect()) {
                return {};
            }
            return m_mock_results;
        }));
    
    ON_CALL(*this, detectSingleTarget(_))
        .WillByDefault(Invoke([this](const cv::Mat& image) -> ObjectDetector::DetectionResult {
            if (image.empty() || !m_model_loaded || !ShouldDetect() || m_mock_results.empty()) {
                return ObjectDetector::DetectionResult();
            }
            return m_mock_results[0];
        }));
    
    ON_CALL(*this, isModelLoaded())
        .WillByDefault(Invoke([this]() { return m_model_loaded; }));
    
    ON_CALL(*this, setConfidenceThreshold(_))
        .WillByDefault(Invoke([this](float threshold) {
            m_confidence_threshold = threshold;
        }));
    
    ON_CALL(*this, setNMSThreshold(_))
        .WillByDefault(Invoke([this](float threshold) {
            m_nms_threshold = threshold;
        }));
    
    ON_CALL(*this, getConfidenceThreshold())
        .WillByDefault(Invoke([this]() { return m_confidence_threshold; }));
    
    ON_CALL(*this, getNMSThreshold())
        .WillByDefault(Invoke([this]() { return m_nms_threshold; }));
    
    ON_CALL(*this, getModelInfo())
        .WillByDefault(Return("MockYOLO v1.0"));
    
    ON_CALL(*this, setUseGPU(_))
        .WillByDefault(Invoke([this](bool use_gpu) {
            return m_gpu_available && use_gpu;
        }));
    
    ON_CALL(*this, isUsingGPU())
        .WillByDefault(Invoke([this]() { return m_gpu_available; }));
}

void MockObjectDetector::SetupFailureScenario() {
    m_model_loaded = false;
    m_gpu_available = false;
    m_success_rate = 0.0;
    
    ON_CALL(*this, loadModel(_, _))
        .WillByDefault(Return(false));
    
    ON_CALL(*this, isModelLoaded())
        .WillByDefault(Return(false));
}

void MockObjectDetector::SetupYOLOScenario() {
    m_model_loaded = true;
    m_gpu_available = true;
    m_success_rate = 0.85;
    
    SetMockDetectionResults(GenerateYOLOResults());
}

std::vector<ObjectDetector::DetectionResult> MockObjectDetector::GenerateYOLOResults() {
    std::vector<ObjectDetector::DetectionResult> results;
    
    // COCO 클래스 시뮬레이션
    std::vector<std::string> coco_classes = {
        "person", "bicycle", "car", "motorcycle", "airplane",
        "bus", "train", "truck", "boat", "traffic light"
    };
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> class_dis(0, coco_classes.size() - 1);
    std::uniform_int_distribution<> pos_dis(0, 400);
    std::uniform_int_distribution<> size_dis(50, 200);
    std::uniform_real_distribution<> conf_dis(0.6, 0.95);
    
    int num_objects = std::uniform_int_distribution<>(1, 5)(gen);
    
    for (int i = 0; i < num_objects; ++i) {
        ObjectDetector::DetectionResult result;
        result.location = cv::Rect(pos_dis(gen), pos_dis(gen), size_dis(gen), size_dis(gen));
        result.confidence = conf_dis(gen);
        result.label = coco_classes[class_dis(gen)];
        result.class_id = class_dis(gen);
        
        results.push_back(result);
    }
    
    return results;
}

bool MockObjectDetector::ShouldDetect() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    return dis(gen) < m_success_rate;
}