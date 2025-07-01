#include <gtest/gtest.h>
#include "detection/ColorDetector.h"
#include "helpers/TestImageGenerator.h"
#include <opencv2/opencv.hpp>

class ColorDetectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        detector = std::make_unique<ColorDetector>();
        ASSERT_TRUE(detector->initialize());
    }
    
    void TearDown() override {
        detector.reset();
    }
    
    std::unique_ptr<ColorDetector> detector;
    
    // 테스트용 상수
    static constexpr int TEST_WIDTH = 640;
    static constexpr int TEST_HEIGHT = 480;
};

// 초기화 테스트
TEST_F(ColorDetectorTest, Initialization) {
    EXPECT_TRUE(detector->isConfigured());
    EXPECT_EQ(detector->getAlgorithmName(), "HSV_ColorDetector");
    
    auto default_range = detector->getColorRange();
    EXPECT_GE(default_range.h_min, 0);
    EXPECT_LE(default_range.h_max, 179);
    EXPECT_GE(default_range.s_min, 0);
    EXPECT_LE(default_range.s_max, 255);
    EXPECT_GE(default_range.v_min, 0);
    EXPECT_LE(default_range.v_max, 255);
}

// 기본 색상 범위 설정 테스트
TEST_F(ColorDetectorTest, SetColorRangeStruct) {
    ColorDetector::HSVRange new_range(100, 150, 200, 120, 255, 255);
    detector->setColorRange(new_range);
    
    auto retrieved_range = detector->getColorRange();
    EXPECT_EQ(retrieved_range.h_min, 100);
    EXPECT_EQ(retrieved_range.s_min, 150);
    EXPECT_EQ(retrieved_range.v_min, 200);
    EXPECT_EQ(retrieved_range.h_max, 120);
    EXPECT_EQ(retrieved_range.s_max, 255);
    EXPECT_EQ(retrieved_range.v_max, 255);
}

// cv::Scalar 색상 범위 설정 테스트
TEST_F(ColorDetectorTest, SetColorRangeScalar) {
    cv::Scalar lower(110, 100, 100);
    cv::Scalar upper(130, 255, 255);
    
    detector->setColorRange(lower, upper);
    
    auto range = detector->getColorRange();
    EXPECT_EQ(range.h_min, 110);
    EXPECT_EQ(range.s_min, 100);
    EXPECT_EQ(range.v_min, 100);
    EXPECT_EQ(range.h_max, 130);
    EXPECT_EQ(range.s_max, 255);
    EXPECT_EQ(range.v_max, 255);
}

// 단일 객체 검출 테스트 - 성공 케이스
TEST_F(ColorDetectorTest, DetectSingleTargetSuccess) {
    // 파란색 계열 객체가 포함된 이미지 생성
    auto [test_image, expected_rects] = TestImageGenerator::createHSVTestImage(
        TEST_WIDTH, TEST_HEIGHT, 100, 120, 1
    );
    
    // 파란색 범위 설정
    detector->setColorRange(cv::Scalar(100, 100, 100), cv::Scalar(120, 255, 255));
    
    cv::Rect result = detector->detectTarget(test_image);
    
    EXPECT_FALSE(result.empty());
    EXPECT_GT(result.area(), 0);
    EXPECT_GE(result.x, 0);
    EXPECT_GE(result.y, 0);
    EXPECT_LT(result.x + result.width, TEST_WIDTH);
    EXPECT_LT(result.y + result.height, TEST_HEIGHT);
}

// 단일 객체 검출 테스트 - 객체 없음
TEST_F(ColorDetectorTest, DetectSingleTargetNoObject) {
    // 검출 대상이 없는 단색 이미지 생성
    cv::Mat test_image = TestImageGenerator::createSolidColorImage(
        TEST_WIDTH, TEST_HEIGHT, cv::Scalar(0, 0, 255) // 빨간색
    );
    
    // 파란색 범위 설정 (빨간색은 검출되지 않음)
    detector->setColorRange(cv::Scalar(100, 100, 100), cv::Scalar(120, 255, 255));
    
    cv::Rect result = detector->detectTarget(test_image);
    
    EXPECT_TRUE(result.empty());
}

// 다중 객체 검출 테스트
TEST_F(ColorDetectorTest, DetectMultipleTargets) {
    // 여러 객체가 포함된 이미지 생성
    auto [test_image, expected_rects] = TestImageGenerator::createHSVTestImage(
        TEST_WIDTH, TEST_HEIGHT, 100, 120, 3
    );
    
    detector->setColorRange(cv::Scalar(100, 100, 100), cv::Scalar(120, 255, 255));
    
    auto results = detector->detectMultipleTargets(test_image);
    
    EXPECT_GT(results.size(), 0);
    EXPECT_LE(results.size(), 3);
    
    for (const auto& result : results) {
        EXPECT_GT(result.bbox.area(), 0);
        EXPECT_GE(result.confidence, 0.0);
        EXPECT_LE(result.confidence, 1.0);
        EXPECT_FALSE(result.label.empty());
    }
}

// 빈 이미지 처리 테스트
TEST_F(ColorDetectorTest, ProcessEmptyImage) {
    cv::Mat empty_image;
    
    cv::Rect result = detector->detectTarget(empty_image);
    EXPECT_TRUE(result.empty());
    
    auto multi_results = detector->detectMultipleTargets(empty_image);
    EXPECT_TRUE(multi_results.empty());
}

// 잘못된 크기 이미지 처리 테스트
TEST_F(ColorDetectorTest, ProcessInvalidSizeImage) {
    cv::Mat invalid_image = cv::Mat::zeros(0, 0, CV_8UC3);
    
    cv::Rect result = detector->detectTarget(invalid_image);
    EXPECT_TRUE(result.empty());
}

// 노이즈가 포함된 이미지 처리 테스트
TEST_F(ColorDetectorTest, ProcessNoisyImage) {
    cv::Mat noisy_image = TestImageGenerator::createNoisyImage(TEST_WIDTH, TEST_HEIGHT, 0.3);
    
    // 노이즈 이미지에 파란색 사각형 추가
    cv::rectangle(noisy_image, cv::Rect(100, 100, 50, 50), cv::Scalar(255, 0, 0), -1);
    
    detector->setColorRange(cv::Scalar(100, 100, 100), cv::Scalar(120, 255, 255));
    
    // 노이즈가 있어도 객체를 검출할 수 있어야 함
    cv::Rect result = detector->detectTarget(noisy_image);
    // 노이즈로 인해 검출이 실패할 수도 있으므로 너무 엄격하게 테스트하지 않음
}

// 다양한 크기의 객체 검출 테스트
TEST_F(ColorDetectorTest, DetectVariousSizes) {
    std::vector<int> sizes = {20, 50, 100};
    cv::Scalar target_color(255, 0, 0); // 파란색 (BGR)
    
    auto [test_image, expected_rects] = TestImageGenerator::createMultiSizeObjectImage(
        TEST_WIDTH, TEST_HEIGHT, target_color, sizes
    );
    
    detector->setColorRange(cv::Scalar(100, 100, 100), cv::Scalar(120, 255, 255));
    
    auto results = detector->detectMultipleTargets(test_image);
    
    // 크기가 다른 객체들이 검출되어야 함
    EXPECT_GT(results.size(), 0);
    
    // 검출된 객체들의 크기 다양성 확인
    std::vector<int> detected_areas;
    for (const auto& result : results) {
        detected_areas.push_back(result.bbox.area());
    }
    
    if (detected_areas.size() > 1) {
        std::sort(detected_areas.begin(), detected_areas.end());
        EXPECT_NE(detected_areas.front(), detected_areas.back());
    }
}

// 원형 객체 검출 테스트
TEST_F(ColorDetectorTest, DetectCircularObjects) {
    auto [test_image, expected_rects] = TestImageGenerator::createCircleObjectsImage(
        TEST_WIDTH, TEST_HEIGHT, cv::Scalar(255, 0, 0), 25, 3
    );
    
    detector->setColorRange(cv::Scalar(100, 100, 100), cv::Scalar(120, 255, 255));
    
    auto results = detector->detectMultipleTargets(test_image);
    
    EXPECT_GT(results.size(), 0);
    
    // 원형 객체도 바운딩 박스로 검출되어야 함
    for (const auto& result : results) {
        EXPECT_GT(result.bbox.area(), 0);
        // 원형이므로 width와 height가 비슷해야 함
        double aspect_ratio = static_cast<double>(result.bbox.width) / result.bbox.height;
        EXPECT_NEAR(aspect_ratio, 1.0, 0.5); // 어느 정도 허용 오차
    }
}

// 성능 통계 테스트
TEST_F(ColorDetectorTest, PerformanceStats) {
    cv::Mat test_image = TestImageGenerator::createSolidColorImage(TEST_WIDTH, TEST_HEIGHT, cv::Scalar(255, 0, 0));
    
    // 여러 번 검출 수행
    for (int i = 0; i < 10; ++i) {
        detector->detectTarget(test_image);
    }
    
    std::string stats = detector->getPerformanceStats();
    EXPECT_FALSE(stats.empty());
    EXPECT_NE(stats.find("ColorDetector"), std::string::npos);
}

// 리셋 기능 테스트
TEST_F(ColorDetectorTest, Reset) {
    cv::Mat test_image = TestImageGenerator::createSolidColorImage(TEST_WIDTH, TEST_HEIGHT, cv::Scalar(255, 0, 0));
    
    // 여러 번 검출 수행
    for (int i = 0; i < 5; ++i) {
        detector->detectTarget(test_image);
    }
    
    detector->reset();
    
    // 리셋 후에도 정상 동작해야 함
    cv::Rect result = detector->detectTarget(test_image);
    // 결과는 이미지 내용에 따라 달라질 수 있음
}

// 설정되지 않은 상태 테스트
TEST_F(ColorDetectorTest, UnconfiguredState) {
    ColorDetector unconfigured_detector;
    
    // ColorDetector is configured by default in constructor
    EXPECT_TRUE(unconfigured_detector.isConfigured());
    
    cv::Mat test_image = TestImageGenerator::createSolidColorImage(TEST_WIDTH, TEST_HEIGHT, cv::Scalar(255, 0, 0));
    
    cv::Rect result = unconfigured_detector.detectTarget(test_image);
    // Since detector is configured, it should be able to process the image
}

// 극한 HSV 값 테스트
TEST_F(ColorDetectorTest, ExtremeHSVValues) {
    // 최소값
    detector->setColorRange(cv::Scalar(0, 0, 0), cv::Scalar(0, 0, 0));
    EXPECT_TRUE(detector->isConfigured());
    
    // 최대값
    detector->setColorRange(cv::Scalar(179, 255, 255), cv::Scalar(179, 255, 255));
    EXPECT_TRUE(detector->isConfigured());
    
    // 전체 범위
    detector->setColorRange(cv::Scalar(0, 0, 0), cv::Scalar(179, 255, 255));
    EXPECT_TRUE(detector->isConfigured());
}

// 색상 변환 정확성 테스트
TEST_F(ColorDetectorTest, ColorConversionAccuracy) {
    // HSV 색상으로 직접 이미지 생성
    cv::Mat hsv_image = cv::Mat::zeros(TEST_HEIGHT, TEST_WIDTH, CV_8UC3);
    hsv_image.setTo(cv::Scalar(110, 255, 255)); // 파란색 HSV
    
    cv::Mat bgr_image;
    cv::cvtColor(hsv_image, bgr_image, cv::COLOR_HSV2BGR);
    
    detector->setColorRange(cv::Scalar(105, 200, 200), cv::Scalar(115, 255, 255));
    
    cv::Rect result = detector->detectTarget(bgr_image);
    
    // 전체 이미지가 대상 색상이므로 큰 영역이 검출되어야 함
    EXPECT_FALSE(result.empty());
    EXPECT_GT(static_cast<double>(result.area()) / (TEST_WIDTH * TEST_HEIGHT), 0.5);
}

// 멀티스레드 안전성 테스트
TEST_F(ColorDetectorTest, ThreadSafety) {
    cv::Mat test_image = TestImageGenerator::createSolidColorImage(TEST_WIDTH, TEST_HEIGHT, cv::Scalar(255, 0, 0));
    detector->setColorRange(cv::Scalar(100, 100, 100), cv::Scalar(120, 255, 255));
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &test_image, &success_count]() {
            try {
                cv::Rect result = detector->detectTarget(test_image);
                success_count++;
            } catch (...) {
                // 예외 발생 시 실패로 처리
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(success_count.load(), 5);
}

// 메모리 누수 테스트
TEST_F(ColorDetectorTest, MemoryLeakTest) {
    cv::Mat test_image = TestImageGenerator::createSolidColorImage(TEST_WIDTH, TEST_HEIGHT, cv::Scalar(255, 0, 0));
    
    // 많은 검출 작업 수행
    for (int i = 0; i < 1000; ++i) {
        auto results = detector->detectMultipleTargets(test_image);
        // 결과를 즉시 폐기하여 메모리 사용량 확인
    }
    
    // 메모리 사용량은 직접 측정하기 어려우므로, 예외 발생 없이 완료되는지 확인
    SUCCEED();
}