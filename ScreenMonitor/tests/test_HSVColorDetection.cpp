/**
 * @file test_HSVColorDetection.cpp
 * @brief Phase 5 QA: HSV Color Detection Testing
 * 
 * Validation of HSV color detection accuracy and performance
 * within the 320x320 region of interest.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <opencv2/opencv.hpp>

#include "detection/HSVColorDetection.h"
#include "helpers/TestImageGenerator.h"

class HSVColorDetectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        detector_ = std::make_unique<HSVColorDetection>();
        target_latency_ms_ = 10.0f;  // Real-time processing target
    }

    cv::Mat CreateHSVTestImage() {
        cv::Mat image(320, 320, CV_8UC3, cv::Scalar(128, 128, 128));
        
        // Add known HSV targets
        cv::rectangle(image, cv::Rect(50, 50, 40, 40), cv::Scalar(0, 0, 255), -1);    // Red
        cv::rectangle(image, cv::Rect(150, 100, 30, 30), cv::Scalar(0, 255, 0), -1);  // Green
        cv::rectangle(image, cv::Rect(250, 150, 35, 35), cv::Scalar(255, 0, 0), -1);  // Blue
        
        return image;
    }

    std::unique_ptr<HSVColorDetection> detector_;
    float target_latency_ms_;
};

/**
 * @brief TEST: HSV Detection Performance
 */
TEST_F(HSVColorDetectionTest, DetectionPerformance) {
    cv::Mat test_image = CreateHSVTestImage();
    
    const int iterations = 100;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        auto detections = detector_->DetectMultiple(test_image);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    float avg_latency_ms = std::chrono::duration<float, std::milli>(end - start).count() / iterations;
    
    EXPECT_LT(avg_latency_ms, target_latency_ms_) << "HSV detection too slow";
    
    std::cout << "HSV Detection Average Latency: " << avg_latency_ms << " ms" << std::endl;
    std::cout << "HSV Detection FPS: " << (1000.0f / avg_latency_ms) << std::endl;
}

/**
 * @brief TEST: Color Detection Accuracy
 */
TEST_F(HSVColorDetectionTest, ColorDetectionAccuracy) {
    cv::Mat test_image = CreateHSVTestImage();
    auto detections = detector_->DetectMultiple(test_image);
    
    EXPECT_GE(detections.size(), 3) << "Should detect at least 3 colored regions";
    
    // Verify detection regions are reasonable
    for (const auto& detection : detections) {
        EXPECT_GE(detection.bounding_box.x, 0) << "Detection X coordinate invalid";
        EXPECT_GE(detection.bounding_box.y, 0) << "Detection Y coordinate invalid";
        EXPECT_LE(detection.bounding_box.x + detection.bounding_box.width, 320) << "Detection extends beyond image width";
        EXPECT_LE(detection.bounding_box.y + detection.bounding_box.height, 320) << "Detection extends beyond image height";
        EXPECT_GT(detection.bounding_box.area(), 100) << "Detection area too small to be meaningful";
        EXPECT_GT(detection.confidence, 0.0f) << "Detection confidence should be positive";
    }
}