#pragma once

#include "detection/ColorDetector.h"
#include "detection/ObjectDetector.h"
#include <gmock/gmock.h>
#include <opencv2/opencv.hpp>
#include <vector>

/**
 * @brief 색상 검출기 Mock 클래스
 */
class MockColorDetector {
public:
    MockColorDetector();
    virtual ~MockColorDetector() = default;
    
    // Mock 메서드들
    MOCK_METHOD(cv::Rect, detectTarget, (const cv::Mat& image), ());
    MOCK_METHOD(std::vector<ColorDetector::DetectionResult>, detectMultipleTargets, (const cv::Mat& image), ());
    MOCK_METHOD(void, setColorRange, (const ColorDetector::HSVRange& range), ());
    MOCK_METHOD(void, setColorRange, (const cv::Scalar& lower, const cv::Scalar& upper), ());
    MOCK_METHOD(ColorDetector::HSVRange, getColorRange, (), (const));
    MOCK_METHOD(bool, initialize, (), ());
    MOCK_METHOD(void, reset, (), ());
    MOCK_METHOD(std::string, getAlgorithmName, (), (const));
    MOCK_METHOD(bool, isConfigured, (), (const));
    MOCK_METHOD(std::string, getPerformanceStats, (), (const));
    
    // 헬퍼 메서드들
    void SetMockDetectionResult(const cv::Rect& result);
    void SetMockDetectionResults(const std::vector<ColorDetector::DetectionResult>& results);
    void SetDetectionSuccessRate(double success_rate);
    void SimulateNoDetection();
    void SimulateMultipleDetections(int count);
    void SetupDefaultBehavior();
    void SetupFailureScenario();
    void SetupPerformanceScenario();
    
private:
    cv::Rect m_mock_detection;
    std::vector<ColorDetector::DetectionResult> m_mock_results;
    ColorDetector::HSVRange m_mock_range;
    double m_success_rate = 1.0;
    bool m_configured = true;
    int m_detection_call_count = 0;
    
    bool ShouldDetect();
    cv::Rect GenerateRandomDetection();
    std::vector<ColorDetector::DetectionResult> GenerateRandomDetections(int count);
};

/**
 * @brief 객체 검출기 Mock 클래스
 */
class MockObjectDetector {
public:
    MockObjectDetector();
    virtual ~MockObjectDetector() = default;
    
    // Mock 메서드들
    MOCK_METHOD(bool, loadModel, (const std::string& model_path, const std::string& config_path), ());
    MOCK_METHOD(std::vector<ObjectDetector::DetectionResult>, detectMultipleTargets, (const cv::Mat& image), ());
    MOCK_METHOD(cv::Rect, detectTarget, (const cv::Mat& image), ());
    MOCK_METHOD(bool, isModelLoaded, (), (const));
    MOCK_METHOD(void, setConfidenceThreshold, (float threshold), ());
    MOCK_METHOD(void, setNMSThreshold, (float threshold), ());
    MOCK_METHOD(float, getConfidenceThreshold, (), (const));
    MOCK_METHOD(float, getNMSThreshold, (), (const));
    MOCK_METHOD(std::string, getModelInfo, (), (const));
    MOCK_METHOD(bool, setUseGPU, (bool use_gpu), ());
    MOCK_METHOD(bool, isUsingGPU, (), (const));
    
    // 헬퍼 메서드들
    void SetMockDetectionResults(const std::vector<ObjectDetector::DetectionResult>& results);
    void SetDetectionSuccessRate(double success_rate);
    void SimulateModelLoaded(bool loaded);
    void SimulateGPUAvailable(bool available);
    void SetupDefaultBehavior();
    void SetupFailureScenario();
    void SetupYOLOScenario();
    
private:
    std::vector<ObjectDetector::DetectionResult> m_mock_results;
    bool m_model_loaded = false;
    bool m_gpu_available = false;
    float m_confidence_threshold = 0.5f;
    float m_nms_threshold = 0.4f;
    double m_success_rate = 1.0;
    
    std::vector<ObjectDetector::DetectionResult> GenerateYOLOResults();
    bool ShouldDetect();
};