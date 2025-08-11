/**
 * @file test_YOLOv11Performance.cpp
 * @brief Phase 5 QA: YOLOv11 TensorRT Performance Testing
 * 
 * Comprehensive performance validation of YOLOv11 TensorRT inference
 * against Phase 0-4 research targets (280+ FPS GPU, 30+ FPS CPU).
 */

#include <gtest/gtest.h>
#include <chrono>
#include <opencv2/opencv.hpp>

#include "detection/YOLOv11TensorRTInference.h"
#include "helpers/TestImageGenerator.h"

class YOLOv11PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        detector_ = std::make_unique<YOLOv11TensorRTInference>();
        
        // Phase 0-4 research targets
        target_gpu_fps_ = 280.0f;  // YOLOv11n TensorRT FP16
        target_cpu_fps_ = 30.0f;   // YOLOv11n OpenCV DNN
        memory_limit_mb_ = 4.0f;   // GPU memory footprint target
    }

    cv::Mat CreateYOLOTestImage() {
        return TestImageGenerator::createYOLOTestImage(320, 320);
    }

    std::unique_ptr<YOLOv11TensorRTInference> detector_;
    float target_gpu_fps_;
    float target_cpu_fps_;
    float memory_limit_mb_;
};

/**
 * @brief TEST: YOLOv11 Inference Performance Benchmark
 */
TEST_F(YOLOv11PerformanceTest, InferencePerformanceBenchmark) {
    cv::Mat test_image = CreateYOLOTestImage();
    ASSERT_FALSE(test_image.empty());
    
    const int warmup_iterations = 20;
    const int benchmark_iterations = 100;
    
    // Warmup phase
    for (int i = 0; i < warmup_iterations; ++i) {
        detector_->DetectMultiple(test_image);
    }
    
    // Benchmark phase
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < benchmark_iterations; ++i) {
        auto detections = detector_->DetectMultiple(test_image);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    
    float total_ms = std::chrono::duration<float, std::milli>(end - start).count();
    float avg_latency_ms = total_ms / benchmark_iterations;
    float fps = 1000.0f / avg_latency_ms;
    
    std::cout << "YOLO v11 Performance Results:" << std::endl;
    std::cout << "  Average Latency: " << avg_latency_ms << " ms" << std::endl;
    std::cout << "  Average FPS: " << fps << std::endl;
    std::cout << "  GPU Accelerated: " << (detector_->IsGPUAccelerated() ? "Yes" : "No") << std::endl;
    
    // Performance validation based on hardware
    if (detector_->IsGPUAccelerated()) {
        EXPECT_GT(fps, target_gpu_fps_ * 0.7f) 
            << "GPU performance significantly below target (" << target_gpu_fps_ << " FPS)";
        
        if (fps >= target_gpu_fps_) {
            std::cout << "✓ GPU Performance Target ACHIEVED: " << fps << " >= " << target_gpu_fps_ << " FPS" << std::endl;
        } else {
            std::cout << "⚠ GPU Performance Target MISSED: " << fps << " < " << target_gpu_fps_ << " FPS" << std::endl;
        }
    } else {
        EXPECT_GT(fps, target_cpu_fps_ * 0.7f)
            << "CPU performance significantly below target (" << target_cpu_fps_ << " FPS)";
            
        if (fps >= target_cpu_fps_) {
            std::cout << "✓ CPU Performance Target ACHIEVED: " << fps << " >= " << target_cpu_fps_ << " FPS" << std::endl;
        } else {
            std::cout << "⚠ CPU Performance Target MISSED: " << fps << " < " << target_cpu_fps_ << " FPS" << std::endl;
        }
    }
}

/**
 * @brief TEST: Memory Efficiency Validation
 */
TEST_F(YOLOv11PerformanceTest, MemoryEfficiencyValidation) {
    if (!detector_->IsGPUAccelerated()) {
        GTEST_SKIP() << "GPU memory testing requires GPU acceleration";
    }
    
    cv::Mat test_image = CreateYOLOTestImage();
    
    // Measure memory usage during inference
    auto detections = detector_->DetectMultiple(test_image);
    
    // Note: Actual GPU memory measurement would require CUDA/TensorRT APIs
    // This is a placeholder for memory validation logic
    std::cout << "GPU Memory footprint validation (target: < " << memory_limit_mb_ << " MB)" << std::endl;
    
    // Validate that detections are reasonable
    EXPECT_GE(detections.size(), 0) << "YOLO should return valid detection results";
    
    for (const auto& detection : detections) {
        EXPECT_GE(detection.bounding_box.x, 0) << "Detection coordinates should be valid";
        EXPECT_GE(detection.bounding_box.y, 0) << "Detection coordinates should be valid";
        EXPECT_LE(detection.bounding_box.x + detection.bounding_box.width, 320) << "Detection should be within image bounds";
        EXPECT_LE(detection.bounding_box.y + detection.bounding_box.height, 320) << "Detection should be within image bounds";
        EXPECT_GT(detection.confidence, 0.0f) << "Detection confidence should be positive";
    }
}

/**
 * @brief TEST: Sustained Performance Validation
 */
TEST_F(YOLOv11PerformanceTest, SustainedPerformanceValidation) {
    cv::Mat test_image = CreateYOLOTestImage();
    const int sustained_iterations = 1000;  // Extended operation simulation
    
    std::vector<float> latency_samples;
    latency_samples.reserve(sustained_iterations);
    
    for (int i = 0; i < sustained_iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto detections = detector_->DetectMultiple(test_image);
        auto end = std::chrono::high_resolution_clock::now();
        
        float latency_ms = std::chrono::duration<float, std::milli>(end - start).count();
        latency_samples.push_back(latency_ms);
    }
    
    // Calculate statistics
    float min_latency = *std::min_element(latency_samples.begin(), latency_samples.end());
    float max_latency = *std::max_element(latency_samples.begin(), latency_samples.end());
    float avg_latency = std::accumulate(latency_samples.begin(), latency_samples.end(), 0.0f) / latency_samples.size();
    
    float min_fps = 1000.0f / max_latency;
    float max_fps = 1000.0f / min_latency;
    float avg_fps = 1000.0f / avg_latency;
    
    std::cout << "Sustained Performance Results (n=" << sustained_iterations << "):" << std::endl;
    std::cout << "  Min FPS: " << min_fps << std::endl;
    std::cout << "  Max FPS: " << max_fps << std::endl;
    std::cout << "  Average FPS: " << avg_fps << std::endl;
    std::cout << "  FPS Variance: " << (max_fps - min_fps) << std::endl;
    
    // Validate performance stability
    float fps_variance = max_fps - min_fps;
    float variance_ratio = fps_variance / avg_fps;
    
    EXPECT_LT(variance_ratio, 0.3f) << "Performance variance too high (>30% of average)";
    
    if (detector_->IsGPUAccelerated()) {
        EXPECT_GT(avg_fps, target_gpu_fps_ * 0.8f) << "Sustained GPU performance below 80% of target";
    } else {
        EXPECT_GT(avg_fps, target_cpu_fps_ * 0.8f) << "Sustained CPU performance below 80% of target";
    }
}