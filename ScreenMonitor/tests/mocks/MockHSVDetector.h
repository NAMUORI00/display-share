#pragma once

#include "interfaces/IDetectionAlgorithm.h"
#include <gmock/gmock.h>
#include <opencv2/opencv.hpp>
#include <vector>

/**
 * @brief HSV Color Detection Mock Class for Phase 5 QA Testing
 * 
 * Mock implementation of IDetectionAlgorithm interface for controlled
 * testing of HSV color detection functionality in the 320x320 system.
 */
class MockHSVDetector : public IDetectionAlgorithm {
public:
    MockHSVDetector() = default;
    virtual ~MockHSVDetector() = default;
    
    // IDetectionAlgorithm interface mocks
    MOCK_METHOD(bool, Initialize, (const AlgorithmSettings& settings), (override));
    MOCK_METHOD(DetectionResult, DetectSingle, (const cv::Mat& image), (override));
    MOCK_METHOD(std::vector<DetectionResult>, DetectMultiple, (const cv::Mat& image), (override));
    MOCK_METHOD(std::vector<DetectionResult>, DetectInROI, (const cv::Mat& image, const cv::Rect& roi), (override));
    MOCK_METHOD(bool, UpdateSettings, (const AlgorithmSettings& settings), (override));
    MOCK_METHOD(std::string, GetAlgorithmName, (), (const, override));
    MOCK_METHOD(std::string, GetVersion, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, GetSupportedClasses, (), (const, override));
    MOCK_METHOD(std::string, GetPerformanceStats, (), (const, override));
    MOCK_METHOD(void, Reset, (), (override));
    MOCK_METHOD(void, Cleanup, (), (override));
    
    // Simplified DetectObjects method for compatibility with existing tests
    MOCK_METHOD(std::vector<cv::Rect>, DetectObjects, (const cv::Mat& image), ());
    
    // Helper methods for test setup
    void SetMockDetectionResult(const DetectionResult& result) {
        mock_single_result_ = result;
    }
    
    void SetMockDetectionResults(const std::vector<DetectionResult>& results) {
        mock_multiple_results_ = results;
    }
    
    void SetMockObjectRects(const std::vector<cv::Rect>& rects) {
        mock_object_rects_ = rects;
    }
    
    void SetMockPerformanceStats(const std::string& stats) {
        mock_performance_stats_ = stats;
    }

    void SetupDefaultBehavior() {
        using ::testing::Return;
        using ::testing::_;
        
        ON_CALL(*this, Initialize(_))
            .WillByDefault(Return(true));
            
        ON_CALL(*this, GetAlgorithmName())
            .WillByDefault(Return("MockHSVDetector"));
            
        ON_CALL(*this, GetVersion())
            .WillByDefault(Return("1.0.0-test"));
            
        ON_CALL(*this, DetectObjects(_))
            .WillByDefault(Return(mock_object_rects_));
            
        ON_CALL(*this, DetectMultiple(_))
            .WillByDefault(Return(mock_multiple_results_));
    }

private:
    DetectionResult mock_single_result_;
    std::vector<DetectionResult> mock_multiple_results_;
    std::vector<cv::Rect> mock_object_rects_;
    std::string mock_performance_stats_;
};