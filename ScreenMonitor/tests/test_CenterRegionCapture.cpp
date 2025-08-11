/**
 * @file test_CenterRegionCapture.cpp
 * @brief Phase 5 QA: Center Region 320x320 Capture Testing
 * 
 * Comprehensive validation of the CenterRegionCapture component,
 * focusing on precise 320x320 region extraction performance and accuracy.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <opencv2/opencv.hpp>

#include "capture/CenterRegionCapture.h"
#include "helpers/TestImageGenerator.h"

class CenterRegionCaptureTest : public ::testing::Test {
protected:
    void SetUp() override {
        capture_ = std::make_unique<CenterRegionCapture>();
        target_latency_ms_ = 5.0f;  // <5ms capture target
    }

    std::unique_ptr<CenterRegionCapture> capture_;
    float target_latency_ms_;
};

/**
 * @brief TEST: 320x320 Region Extraction Performance
 */
TEST_F(CenterRegionCaptureTest, RegionExtractionPerformance) {
    // Test different source resolutions
    std::vector<cv::Size> test_resolutions = {
        {1920, 1080}, {2560, 1440}, {3840, 2160}, {1366, 768}
    };

    for (const auto& resolution : test_resolutions) {
        cv::Mat source_image = TestImageGenerator::createCheckerboardImage(
            resolution.width, resolution.height
        );

        cv::Mat center_region;
        auto start = std::chrono::high_resolution_clock::now();
        bool success = capture_->ExtractCenterRegion(source_image, center_region);
        auto end = std::chrono::high_resolution_clock::now();

        float latency_ms = std::chrono::duration<float, std::milli>(end - start).count();

        EXPECT_TRUE(success) << "ExtractCenterRegion should succeed";
        EXPECT_LT(latency_ms, target_latency_ms_) 
            << "Extraction too slow for " << resolution.width << "x" << resolution.height;
        
        if (success) {
            EXPECT_EQ(center_region.rows, 320) << "Height mismatch";
            EXPECT_EQ(center_region.cols, 320) << "Width mismatch";
        }

        std::cout << "Resolution " << resolution.width << "x" << resolution.height 
                  << ": " << latency_ms << " ms" << std::endl;
    }
}

/**
 * @brief TEST: Coordinate Accuracy Validation
 */
TEST_F(CenterRegionCaptureTest, CoordinateAccuracy) {
    cv::Size source_size(1920, 1080);
    cv::Mat source_image = cv::Mat::zeros(source_size, CV_8UC3);
    
    // Draw test pattern at known coordinates
    int center_x = (source_size.width - 320) / 2;
    int center_y = (source_size.height - 320) / 2;
    
    // Mark specific points that should appear in center region
    cv::circle(source_image, cv::Point(center_x + 50, center_y + 50), 5, cv::Scalar(255, 0, 0), -1);
    cv::circle(source_image, cv::Point(center_x + 270, center_y + 270), 5, cv::Scalar(0, 255, 0), -1);
    
    cv::Mat center_region;
    bool success = capture_->ExtractCenterRegion(source_image, center_region);
    ASSERT_TRUE(success) << "ExtractCenterRegion should succeed";
    
    // Verify marked points are present
    cv::Vec3b pixel1 = center_region.at<cv::Vec3b>(50, 50);
    cv::Vec3b pixel2 = center_region.at<cv::Vec3b>(270, 270);
    
    EXPECT_EQ(pixel1[2], 255) << "Red marker not found at expected position";
    EXPECT_EQ(pixel2[1], 255) << "Green marker not found at expected position";
}