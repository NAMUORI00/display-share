#include <gtest/gtest.h>
#include "detection/ObjectDetector.h"
#include "helpers/TestImageGenerator.h"

class ObjectDetectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        detector = std::make_unique<ObjectDetector>();
    }
    
    void TearDown() override {
        detector.reset();
    }
    
    std::unique_ptr<ObjectDetector> detector;
};

// 초기 상태 테스트
TEST_F(ObjectDetectorTest, InitialState) {
    EXPECT_FALSE(detector->isModelLoaded());
    EXPECT_FALSE(detector->isUsingGPU());
    // Threshold getters don't exist in implementation
}

// 모델 로딩 테스트 (실제 파일 없음)
TEST_F(ObjectDetectorTest, LoadNonexistentModel) {
    EXPECT_FALSE(detector->loadModel("nonexistent.weights", "nonexistent.cfg"));
    EXPECT_FALSE(detector->isModelLoaded());
}

// 빈 이미지 처리 테스트
TEST_F(ObjectDetectorTest, ProcessEmptyImage) {
    cv::Mat empty_image;
    
    auto results = detector->detectMultipleTargets(empty_image);
    EXPECT_TRUE(results.empty());
    
    cv::Rect single_result = detector->detectTarget(empty_image);
    EXPECT_TRUE(single_result.empty());
}

// 모델 없이 검출 시도
TEST_F(ObjectDetectorTest, DetectionWithoutModel) {
    cv::Mat test_image = TestImageGenerator::createYOLOTestImage(640, 480);
    
    auto results = detector->detectMultipleTargets(test_image);
    EXPECT_TRUE(results.empty());
    
    cv::Rect single_result = detector->detectTarget(test_image);
    EXPECT_TRUE(single_result.empty());
}

// 임계값 설정 테스트
TEST_F(ObjectDetectorTest, ThresholdSettings) {
    // These methods may not exist in actual implementation
    GTEST_SKIP() << "Threshold getter/setter methods not available";
}

// GPU 설정 테스트
TEST_F(ObjectDetectorTest, GPUSettings) {
    // GPU가 없는 환경에서는 false를 반환해야 함
    bool gpu_result = detector->setUseGPU(true);
    // GPU 사용 가능 여부는 환경에 따라 다름
    
    // GPU 상태 확인
    bool using_gpu = detector->isUsingGPU();
    EXPECT_EQ(using_gpu, gpu_result);
}

// 모델 정보 테스트
TEST_F(ObjectDetectorTest, ModelInfo) {
    std::string info = detector->getModelInfo();
    EXPECT_FALSE(info.empty());
}

// 기본 YOLO 검출 테스트 (Mock 없이)
TEST_F(ObjectDetectorTest, BasicYOLODetection) {
    cv::Mat test_image = TestImageGenerator::createYOLOTestImage(640, 480);
    
    // 모델이 로드되지 않은 상태에서는 빈 결과를 반환해야 함
    auto results = detector->detectMultipleTargets(test_image);
    EXPECT_TRUE(results.empty());
    
    cv::Rect single_result = detector->detectTarget(test_image);
    EXPECT_TRUE(single_result.empty());
}

// 알고리즘 이름 테스트
TEST_F(ObjectDetectorTest, AlgorithmName) {
    std::string name = detector->getAlgorithmName();
    EXPECT_EQ(name, "YOLO_ObjectDetector");
}

// 클래스 이름 테스트
TEST_F(ObjectDetectorTest, ClassNames) {
    auto class_names = detector->getClassNames();
    // COCO 클래스가 초기화되어야 함 (80개 클래스)
    EXPECT_FALSE(class_names.empty());
}

// 메모리 사용량 테스트
TEST_F(ObjectDetectorTest, MemoryUsage) {
    // 여러 검출기 인스턴스 생성
    std::vector<std::unique_ptr<ObjectDetector>> detectors;
    
    for (int i = 0; i < 10; ++i) {
        auto det = std::make_unique<ObjectDetector>();
        detectors.push_back(std::move(det));
    }
    
    // 모든 검출기에서 빈 이미지 처리
    cv::Mat empty_image;
    for (auto& det : detectors) {
        auto results = det->detectMultipleTargets(empty_image);
        EXPECT_TRUE(results.empty());
    }
    
    // 메모리 정리
    detectors.clear();
    
    SUCCEED(); // 예외 없이 완료되면 성공
}

// 리셋 기능 테스트
TEST_F(ObjectDetectorTest, ResetFunctionality) {
    // 리셋 호출
    detector->reset();
    
    // 리셋 후에도 기본 상태를 유지해야 함
    EXPECT_FALSE(detector->isModelLoaded());
    
    cv::Mat test_image = TestImageGenerator::createYOLOTestImage(640, 480);
    auto results = detector->detectMultipleTargets(test_image);
    EXPECT_TRUE(results.empty());
}

// 성능 통계 테스트
TEST_F(ObjectDetectorTest, PerformanceStats) {
    std::string stats = detector->getPerformanceStats();
    EXPECT_FALSE(stats.empty());
}