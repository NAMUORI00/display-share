#pragma once

#include "detection/HSVColorDetection.h"
#include <gmock/gmock.h>
#include <opencv2/opencv.hpp>
#include <vector>

namespace ScreenMonitor {

/**
 * @brief HSV 색상 검출기 Mock 클래스 (간소화된 버전)
 */
class MockHSVDetector {
public:
    MockHSVDetector() = default;
    virtual ~MockHSVDetector() = default;
    
    // 핵심 Mock 메서드들만 유지
    MOCK_METHOD(cv::Rect, detectTarget, (const cv::Mat& image), ());
    MOCK_METHOD(std::vector<DetectionResult>, detectMultipleTargets, (const cv::Mat& image), ());
    MOCK_METHOD(void, setHSVRange, (const HSVRange& range), ());
    MOCK_METHOD(HSVRange, getHSVRange, (), (const));
    MOCK_METHOD(bool, initialize, (), ());
    
    // 헬퍼 메서드들
    void SetMockDetectionResult(const cv::Rect& result) {
        m_mock_detection = result;
    }
    
    void SetMockDetectionResults(const std::vector<DetectionResult>& results) {
        m_mock_results = results;
    }
    
    void SetupDefaultBehavior() {
        using ::testing::Return;
        using ::testing::Invoke;
        
        ON_CALL(*this, detectTarget)
            .WillByDefault(Return(m_mock_detection));
            
        ON_CALL(*this, detectMultipleTargets)
            .WillByDefault(Return(m_mock_results));
            
        ON_CALL(*this, initialize)
            .WillByDefault(Return(true));
    }
    
private:
    cv::Rect m_mock_detection{100, 100, 50, 50};
    std::vector<DetectionResult> m_mock_results;
    HSVRange m_mock_range{140, 160, 120, 200, 180, 255};
};

/**
 * @brief YOLO 객체 검출기 Mock 클래스 (간소화된 버전)
 */
class MockYOLODetector {
public:
    MockYOLODetector() = default;
    virtual ~MockYOLODetector() = default;
    
    // 핵심 Mock 메서드들만 유지
    MOCK_METHOD(std::vector<DetectionBox>, detect, (const cv::Mat& image), ());
    MOCK_METHOD(bool, initialize, (const std::string& model_path), ());
    MOCK_METHOD(bool, isInitialized, (), (const));
    
    // 헬퍼 메서드들
    void SetMockDetectionResults(const std::vector<DetectionBox>& results) {
        m_mock_results = results;
    }
    
    void SetupDefaultBehavior() {
        using ::testing::Return;
        using ::testing::Invoke;
        
        ON_CALL(*this, detect)
            .WillByDefault(Return(m_mock_results));
            
        ON_CALL(*this, initialize)
            .WillByDefault(Return(true));
            
        ON_CALL(*this, isInitialized)
            .WillByDefault(Return(true));
    }
    
private:
    std::vector<DetectionBox> m_mock_results;
};

} // namespace ScreenMonitor