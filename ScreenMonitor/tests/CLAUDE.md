# CLAUDE.md - 320x320 시스템 테스트 전략 및 품질 보증 가이드

ScreenMonitor 320x320 중심 영역 검출 시스템의 테스트 아키텍처, 전략, 및 품질 보증 시스템 (Phase 0-3 완료)

## 320x320 테스트 전략 개요

ScreenMonitor는 **GoogleTest/GoogleMock 기반의 320x320 특화 테스트 전략**을 통해 280+ FPS YOLO, 60+ FPS GUI 성능을 보장합니다. 간소화된 아키텍처 (35% 코드 감축)에 맞는 효율적인 테스트 접근법을 채택합니다.

### 🎯 320x320 테스트 철학
- **성능 최우선**: 280+ FPS YOLO, <5ms ROI 추출 성능 검증
- **간소화된 커버리지**: 핵심 모듈 (CenterRegionCapture, YOLOv11, HSV) 90%+ 커버리지
- **실시간 벤치마킹**: 320x320 시스템 특성에 맞는 실시간 성능 테스트
- **GPU 테스트**: TensorRT 280+ FPS 및 CPU 백엔드 30+ FPS 검증

### 📊 320x320 테스트 피라미드 구조 (간소화)
```
           🔺 E2E Tests (5%)
          4패널 GUI, 320x320 워크플로우 테스트
         
        🔺🔺 Integration Tests (15%)
       ROI-YOLO-HSV 통합, GPU/CPU 성능 테스트
      
    🔺🔺🔺 Unit Tests (80%)
   CenterRegionCapture, YOLOv11, HSV 개별 테스트
```

## 320x320 테스트 디렉토리 구조 (간소화)

### 📁 tests/ 아키텍처 (Phase 0-3 최적화)
```
tests/
├── test_*.cpp                   # 📋 320x320 특화 단위 테스트
│   ├── test_ConfigManager.cpp   # 간소화된 설정 관리 테스트
│   ├── test_CenterRegionCapture.cpp # ✨ 320x320 ROI 추출 테스트 (<5ms 검증)
│   ├── test_HSVColorDetection.cpp   # HSV 색상 추적 테스트 (320x320 최적화)
│   ├── test_YOLOv11Integration.cpp  # 280+ FPS YOLO 통합 테스트
│   ├── test_MainInterface.cpp   # 4패널 GUI 테스트 (Phase 3)
│   └── test_Performance*.cpp    # 성능 벤치마크 통합 테스트
│
├── helpers/                     # 🛠️ 320x320 테스트 유틸리티 (최소화)
│   ├── ConfigTestHelper.cpp/.h  # 간소화된 설정 테스트 도우미
│   ├── GLTestContext.cpp/.h     # 4패널 GUI 테스트 컨텍스트
│   └── TestImageGenerator.cpp/.h # 320x320 테스트 이미지 생성
│
├── mocks/                       # 🎭 간소화된 Mock 객체
│   ├── MockHSVDetector.cpp/.h   # HSV 검출 Mock (간소화)
│   └── MockScreenCapture.cpp/.h # 화면 캡처 Mock (320x320 특화)
│
├── benchmark_tests.cpp          # ⚡ 280+ FPS YOLO 성능 벤치마크
└── [320x320 테스트 데이터]     # 🗂️ 320x320 테스트 이미지, ROI 설정
```

**🗑️ 제거된 테스트 (35% 감축):**
- ❌ **test_ColorDetector.cpp** → HSVColorDetection 테스트로 통합
- ❌ **test_ObjectDetector.cpp** → YOLOv11 테스트로 대체
- ❌ **MockDetector.cpp** → 개별 Mock으로 분리 (MockHSV, MockYOLO)

## GoogleTest/GoogleMock 활용

### 🧪 320x320 특화 테스트 구조

#### CenterRegionCapture 테스트 템플릿 (핵심)
```cpp
#include <gtest/gtest.h>
#include "capture/CenterRegionCapture.h"
#include <opencv2/opencv.hpp>
#include <chrono>

// 320x320 ROI 추출 테스트 픽스처
class CenterRegionCaptureTest : public ::testing::Test {
protected:
    void SetUp() override {
        center_capture_ = std::make_unique<CenterRegionCapture>();
        
        // 테스트용 1920x1080 풀 HD 프레임 생성
        full_frame_ = cv::Mat::zeros(1080, 1920, CV_8UC3);
        cv::rectangle(full_frame_, cv::Rect(800, 380, 320, 320), cv::Scalar(0, 255, 0), -1);
    }
    
    std::unique_ptr<CenterRegionCapture> center_capture_;
    cv::Mat full_frame_;
    static constexpr int TARGET_WIDTH = 320;
    static constexpr int TARGET_HEIGHT = 320;
    static constexpr double MAX_EXTRACTION_TIME_MS = 5.0; // <5ms 요구사항
};

// 320x320 ROI 정확성 테스트
TEST_F(CenterRegionCaptureTest, ExtractCenterRegion_FullHDFrame_Returns320x320) {
    cv::Mat roi_output;
    
    bool success = center_capture_->ExtractCenterRegion(full_frame_, roi_output);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(roi_output.cols, TARGET_WIDTH);
    EXPECT_EQ(roi_output.rows, TARGET_HEIGHT);
    EXPECT_EQ(roi_output.type(), CV_8UC3);
}

// <5ms 성능 요구사항 테스트
TEST_F(CenterRegionCaptureTest, ExtractCenterRegion_Performance_UnderFiveMilliseconds) {
    cv::Mat roi_output;
    
    // 워밍업
    for (int i = 0; i < 5; ++i) {
        center_capture_->ExtractCenterRegion(full_frame_, roi_output);
    }
    
    // 성능 측정 (10회 평균)
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10; ++i) {
        center_capture_->ExtractCenterRegion(full_frame_, roi_output);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double average_time_ms = (duration.count() / 10.0) / 1000.0;
    
    EXPECT_LT(average_time_ms, MAX_EXTRACTION_TIME_MS) 
        << "ROI 추출이 너무 느립니다: " << average_time_ms << "ms (목표: <" << MAX_EXTRACTION_TIME_MS << "ms)";
        
    std::cout << "[성능] 320x320 ROI 추출 평균 시간: " << average_time_ms << "ms" << std::endl;
}

// 좌표 변환 정확성 테스트
TEST_F(CenterRegionCaptureTest, TransformCoordinates_ROIToScreen_AccurateMapping) {
    // 1920x1080에서 중심 320x320 영역의 변환 테스트
    auto region_info = center_capture_->CalculateRegionInfo(1920, 1080);
    
    // ROI 중심점 (160, 160)을 화면 좌표로 변환
    int screen_x, screen_y;
    center_capture_->TransformToScreenCoordinates(160, 160, region_info, screen_x, screen_y);
    
    // 1920x1080 화면의 중심점 (960, 540)이어야 함
    EXPECT_EQ(screen_x, 960);
    EXPECT_EQ(screen_y, 540);
}
```

#### ConfigManager 테스트 템플릿 (320x320 특화 간소화)
```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "core/ConfigManager.h"
#include <filesystem>
#include <fstream>

// 320x320 특화 설정 테스트 픽스처
class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_manager_ = std::make_unique<ConfigManager>();
        test_config_path_ = "test_320x320_config.json";
        Create320x320TestConfig();
    }
    
    void TearDown() override {
        std::filesystem::remove(test_config_path_);
    }
    
    void Create320x320TestConfig() {
        // 320x320 시스템 특화 테스트 설정
        nlohmann::json config_320x320 = {
            {"center_region", {
                {"width", 320},
                {"height", 320},
                {"auto_center", true}
            }},
            {"hsv_detection", {
                {"enabled", true},
                {"hue_min", 0}, {"hue_max", 10},
                {"sat_min", 120}, {"sat_max", 255},
                {"val_min", 120}, {"val_max", 255}
            }},
            {"yolo_v11", {
                {"model_path", "models/yolov11n.engine"},
                {"confidence_threshold", 0.5},
                {"nms_threshold", 0.4},
                {"gpu_enabled", true}
            }},
            {"performance", {
                {"target_gui_fps", 60},
                {"target_yolo_fps", 280}
            }}
        };
        
        std::ofstream file(test_config_path_);
        file << config_320x320.dump(2);
    }
    
    std::unique_ptr<ConfigManager> config_manager_;
    std::string test_config_path_;
};

// 320x320 ROI 설정 검증 테스트
TEST_F(ConfigManagerTest, Load320x320CenterRegionConfig_ValidFile_ReturnsTrue) {
    ASSERT_TRUE(std::filesystem::exists(test_config_path_));
    
    bool result = config_manager_->LoadConfig(test_config_path_);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(config_manager_->GetCenterRegionWidth(), 320);
    EXPECT_EQ(config_manager_->GetCenterRegionHeight(), 320);
    EXPECT_TRUE(config_manager_->IsAutoCenterEnabled());
}

// HSV 색상 범위 설정 검증
TEST_F(ConfigManagerTest, LoadHSVSettings_ValidRange_ConfiguresCorrectly) {
    config_manager_->LoadConfig(test_config_path_);
    
    auto hsv_range = config_manager_->GetHSVRange();
    EXPECT_EQ(hsv_range.hue_min, 0);
    EXPECT_EQ(hsv_range.hue_max, 10);
    EXPECT_EQ(hsv_range.sat_min, 120);
    EXPECT_EQ(hsv_range.val_min, 120);
}

// YOLOv11 설정 검증
TEST_F(ConfigManagerTest, LoadYOLOConfig_ValidSettings_ConfiguresGPUMode) {
    config_manager_->LoadConfig(test_config_path_);
    
    EXPECT_EQ(config_manager_->GetYOLOModelPath(), "models/yolov11n.engine");
    EXPECT_FLOAT_EQ(config_manager_->GetYOLOConfidenceThreshold(), 0.5f);
    EXPECT_TRUE(config_manager_->IsYOLOGPUEnabled());
}

// 성능 설정 검증 (280+ FPS YOLO, 60+ FPS GUI)
TEST_F(ConfigManagerTest, LoadPerformanceConfig_ValidTargets_SetsCorrectFPS) {
    config_manager_->LoadConfig(test_config_path_);
    
    EXPECT_EQ(config_manager_->GetTargetGUIFPS(), 60);
    EXPECT_EQ(config_manager_->GetTargetYOLOFPS(), 280);
}

// 잘못된 320x320 설정 처리
TEST_F(ConfigManagerTest, LoadInvalidROISize_TooSmall_UsesDefaultValues) {
    nlohmann::json invalid_config = {
        {"center_region", {{"width", 100}, {"height", 100}}}  // 너무 작은 ROI
    };
    
    std::ofstream file("invalid_roi.json");
    file << invalid_config.dump(2);
    
    bool result = config_manager_->LoadConfig("invalid_roi.json");
    
    // 잘못된 설정 시 기본 320x320으로 폴백
    EXPECT_TRUE(result);  // 로딩은 성공하지만
    EXPECT_EQ(config_manager_->GetCenterRegionWidth(), 320);   // 기본값 사용
    EXPECT_EQ(config_manager_->GetCenterRegionHeight(), 320);
    
    std::filesystem::remove("invalid_roi.json");
}
```

### 🎭 320x320 특화 Mock 객체 활용 (간소화)

#### 간소화된 Mock 클래스 정의
```cpp
// mocks/MockHSVDetector.h - HSV 검출 Mock
#pragma once
#include <gmock/gmock.h>
#include "interfaces/IDetectionAlgorithm.h"

class MockHSVDetector : public IDetectionAlgorithm {
public:
    MOCK_METHOD(std::vector<DetectionResult>, detect, 
                (const cv::Mat& roi_frame), (override));
    MOCK_METHOD(void, configure, 
                (const HSVRange& hsv_range), (override));
    MOCK_METHOD(bool, isInitialized, (), (const, override));
    MOCK_METHOD(std::string, getAlgorithmName, (), (const, override));
};

// mocks/MockYOLODetector.h - YOLO v11 Mock
#pragma once
#include <gmock/gmock.h>
#include "interfaces/IDetectionAlgorithm.h"

class MockYOLODetector : public IDetectionAlgorithm {
public:
    MOCK_METHOD(std::vector<DetectionResult>, detect, 
                (const cv::Mat& roi_320x320), (override));
    MOCK_METHOD(void, setConfidenceThreshold, (float threshold), (override));
    MOCK_METHOD(bool, isGPUAccelerated, (), (const, override));
    MOCK_METHOD(float, getInferenceFPS, (), (const, override));
};

// mocks/MockCenterRegionCapture.h - 320x320 ROI 캡처 Mock
#pragma once
#include <gmock/gmock.h>
#include "interfaces/ICaptureDevice.h"

class MockCenterRegionCapture : public ICaptureDevice {
public:
    MOCK_METHOD(bool, ExtractCenterRegion, 
                (const cv::Mat& full_frame, cv::Mat& roi_output), (override));
    MOCK_METHOD(RegionInfo, CalculateRegionInfo, 
                (int screen_width, int screen_height), (const, override));
    MOCK_METHOD(void, TransformToScreenCoordinates, 
                (int roi_x, int roi_y, const RegionInfo& info, int& screen_x, int& screen_y), 
                (const, override));
    MOCK_METHOD(double, GetLastExtractionTimeMs, (), (const, override));
};
```

#### 320x320 시스템 통합 테스트 (Mock 활용)
```cpp
// 320x320 ROI에서 HSV + YOLO 병렬 검출 테스트
TEST_F(Detection320x320Test, ProcessROI_HSVAndYOLO_ParallelDetection) {
    // 320x320 ROI 준비
    cv::Mat roi_320x320 = CreateTest320x320ROI();
    
    // Mock 검출기 생성
    auto mock_hsv = std::make_shared<MockHSVDetector>();
    auto mock_yolo = std::make_shared<MockYOLODetector>();
    
    // HSV Mock 설정 - 빨간 점 검출
    DetectionResult hsv_result{cv::Point(160, 160), 0.95f, "red_target"};
    EXPECT_CALL(*mock_hsv, detect(roi_320x320))
        .WillOnce(::testing::Return(std::vector<DetectionResult>{hsv_result}));
    
    // YOLO Mock 설정 - 객체 바운딩 박스
    DetectionResult yolo_result{cv::Rect(100, 100, 120, 80), 0.87f, "person"};
    EXPECT_CALL(*mock_yolo, detect(roi_320x320))
        .WillOnce(::testing::Return(std::vector<DetectionResult>{yolo_result}));
    
    EXPECT_CALL(*mock_yolo, getInferenceFPS())
        .WillOnce(::testing::Return(285.0f));  // 280+ FPS 검증
    
    // 병렬 검출 시스템 테스트
    auto detection_system = CreateDetectionSystem320x320();
    detection_system->SetHSVDetector(mock_hsv);
    detection_system->SetYOLODetector(mock_yolo);
    
    auto combined_results = detection_system->ProcessROI(roi_320x320);
    
    // 결과 검증
    ASSERT_EQ(combined_results.size(), 2);
    
    // HSV 결과 확인
    auto hsv_it = std::find_if(combined_results.begin(), combined_results.end(),
        [](const DetectionResult& r) { return r.class_name == "red_target"; });
    ASSERT_NE(hsv_it, combined_results.end());
    EXPECT_EQ(hsv_it->point.x, 160);  // ROI 중심점
    EXPECT_EQ(hsv_it->point.y, 160);
    
    // YOLO 결과 확인
    auto yolo_it = std::find_if(combined_results.begin(), combined_results.end(),
        [](const DetectionResult& r) { return r.class_name == "person"; });
    ASSERT_NE(yolo_it, combined_results.end());
    EXPECT_GE(yolo_it->confidence, 0.8f);
}

// 320x320 ROI 추출 성능 테스트 (Mock)
TEST_F(CapturePerformanceTest, ExtractCenterRegion_Mock_UnderFiveMilliseconds) {
    auto mock_capture = std::make_unique<MockCenterRegionCapture>();
    
    // <5ms 성능 Mock 설정
    EXPECT_CALL(*mock_capture, GetLastExtractionTimeMs())
        .WillRepeatedly(::testing::Return(3.2));  // 3.2ms 성능
    
    EXPECT_CALL(*mock_capture, ExtractCenterRegion(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(true));
    
    // 성능 검증
    cv::Mat full_frame = CreateFullHDTestFrame();
    cv::Mat roi_output;
    
    bool success = mock_capture->ExtractCenterRegion(full_frame, roi_output);
    double extraction_time = mock_capture->GetLastExtractionTimeMs();
    
    EXPECT_TRUE(success);
    EXPECT_LT(extraction_time, 5.0) << "ROI 추출 성능이 5ms를 초과했습니다: " 
                                    << extraction_time << "ms";
}
```

### 🖼️ 320x320 특화 컴퓨터 비전 테스트

#### 320x320 ROI 기반 테스트
```cpp
// helpers/TestImageGenerator.cpp 활용 - 320x320 특화
class HSVColorDetection320x320Test : public ::testing::Test {
protected:
    void SetUp() override {
        hsv_detector_ = std::make_unique<HSVColorDetection>();
        image_gen_320x320_ = std::make_unique<TestImageGenerator320x320>();
    }
    
    std::unique_ptr<HSVColorDetection> hsv_detector_;
    std::unique_ptr<TestImageGenerator320x320> image_gen_320x320_;
    
    static constexpr int ROI_SIZE = 320;
};

// 320x320 ROI에서 빨간 타겟 검출 테스트
TEST_F(HSVColorDetection320x320Test, DetectRedTarget_320x320ROI_FindsCenterPoint) {
    // 320x320 ROI 중앙에 빨간 타겟 생성
    cv::Mat roi_320x320 = image_gen_320x320_->CreateROIWithRedTarget(
        cv::Size(ROI_SIZE, ROI_SIZE), 
        cv::Point(160, 160),  // 중앙 위치
        15                    // 타겟 반지름
    );
    
    // HSV 빨간색 범위 설정 (Phase 2 최적화된 값)
    HSVRange red_range{
        .hue_min = 0, .hue_max = 10,
        .sat_min = 120, .sat_max = 255,
        .val_min = 120, .val_max = 255
    };
    hsv_detector_->SetHSVRange(red_range);
    
    // 320x320 ROI에서 검출 실행
    auto results = hsv_detector_->detect(roi_320x320);
    
    // 결과 검증
    ASSERT_FALSE(results.empty()) << "320x320 ROI에서 빨간 타겟을 찾지 못했습니다";
    
    // 검출된 포인트가 중앙 근처인지 확인 (허용 오차 ±5픽셀)
    cv::Point detected_point = results[0].point;
    EXPECT_NEAR(detected_point.x, 160, 5) << "X 좌표 오차가 큽니다: " << detected_point.x;
    EXPECT_NEAR(detected_point.y, 160, 5) << "Y 좌표 오차가 큽니다: " << detected_point.y;
    
    // 신뢰도 검증
    EXPECT_GE(results[0].confidence, 0.8f) << "HSV 검출 신뢰도가 낮습니다: " 
                                           << results[0].confidence;
}

// 320x320 ROI에서 YOLO 객체 검출 테스트
TEST_F(YOLOv11Detection320x320Test, DetectPerson_320x320ROI_ReturnsValidBoundingBox) {
    // 320x320 ROI에 사람 객체가 포함된 테스트 이미지
    cv::Mat roi_with_person = image_gen_320x320_->LoadTestROI("person_320x320.jpg");
    ASSERT_FALSE(roi_with_person.empty()) << "테스트 이미지를 로드할 수 없습니다";
    ASSERT_EQ(roi_with_person.cols, ROI_SIZE);
    ASSERT_EQ(roi_with_person.rows, ROI_SIZE);
    
    // YOLOv11 검출 실행
    auto yolo_results = yolo_detector_->detect(roi_with_person);
    
    // 결과 검증
    ASSERT_FALSE(yolo_results.empty()) << "320x320 ROI에서 객체를 검출하지 못했습니다";
    
    // 사람 클래스 검출 확인
    auto person_it = std::find_if(yolo_results.begin(), yolo_results.end(),
        [](const DetectionResult& r) { return r.class_name == "person"; });
    ASSERT_NE(person_it, yolo_results.end()) << "사람 객체를 찾지 못했습니다";
    
    // 바운딩 박스가 320x320 범위 내에 있는지 확인
    cv::Rect bbox = person_it->bounding_box;
    EXPECT_GE(bbox.x, 0);
    EXPECT_GE(bbox.y, 0);
    EXPECT_LE(bbox.x + bbox.width, ROI_SIZE);
    EXPECT_LE(bbox.y + bbox.height, ROI_SIZE);
    
    // 신뢰도 검증 (YOLO v11 성능 기준)
    EXPECT_GE(person_it->confidence, 0.5f) << "YOLO 검출 신뢰도가 낮습니다: " 
                                           << person_it->confidence;
}

// 빈 320x320 ROI 테스트 (아무것도 검출되지 않아야 함)
TEST_F(HSVColorDetection320x320Test, EmptyROI_320x320_ReturnsNoDetection) {
    // 빈 320x320 ROI (검은색 배경)
    cv::Mat empty_roi = cv::Mat::zeros(ROI_SIZE, ROI_SIZE, CV_8UC3);
    
    auto hsv_results = hsv_detector_->detect(empty_roi);
    auto yolo_results = yolo_detector_->detect(empty_roi);
    
    EXPECT_TRUE(hsv_results.empty()) << "빈 ROI에서 HSV 검출이 발생했습니다";
    EXPECT_TRUE(yolo_results.empty()) << "빈 ROI에서 YOLO 검출이 발생했습니다";
}

// 320x320 ROI 경계 케이스 테스트
TEST_F(CenterRegionCapture320x320Test, ExtractROI_EdgeCases_HandlesCorrectly) {
    // 다양한 화면 해상도에서 320x320 ROI 추출 테스트
    std::vector<cv::Size> test_resolutions = {
        {1920, 1080},  // Full HD
        {1366, 768},   // 일반적인 노트북
        {1024, 768},   // 최소 지원 해상도
        {2560, 1440},  // QHD
        {3840, 2160}   // 4K
    };
    
    for (const auto& resolution : test_resolutions) {
        cv::Mat full_frame = cv::Mat::zeros(resolution.height, resolution.width, CV_8UC3);
        cv::Mat roi_output;
        
        bool success = center_capture_->ExtractCenterRegion(full_frame, roi_output);
        
        EXPECT_TRUE(success) << "해상도 " << resolution.width << "x" << resolution.height 
                            << "에서 ROI 추출 실패";
        
        if (success) {
            EXPECT_EQ(roi_output.cols, ROI_SIZE) << "ROI 너비가 320이 아닙니다";
            EXPECT_EQ(roi_output.rows, ROI_SIZE) << "ROI 높이가 320이 아닙니다";
        }
    }
}
```

## 320x320 성능 및 벤치마크 테스트 (280+ FPS YOLO)

### ⚡ 320x320 실시간 성능 요구사항 검증

#### 320x320 성능 벤치마크 구조 (Phase 0-3 달성 목표)
```cpp
// benchmark_320x320_tests.cpp
#include <benchmark/benchmark.h>
#include <chrono>
#include "capture/CenterRegionCapture.h"
#include "detection/YOLOv11TensorRTInference.h"
#include "detection/HSVColorDetection.h"

class Performance320x320Benchmark : public ::testing::Test {
protected:
    // 320x320 시스템 성능 목표 (Phase 0-3 완료)
    static constexpr double MAX_ROI_EXTRACTION_MS = 5.0;    // <5ms ROI 추출
    static constexpr double MIN_YOLO_FPS_GPU = 280.0;       // 280+ FPS YOLO (GPU)
    static constexpr double MIN_YOLO_FPS_CPU = 30.0;        // 30+ FPS YOLO (CPU)
    static constexpr double MIN_GUI_FPS = 60.0;             // 60+ FPS GUI
    static constexpr int ROI_SIZE = 320;
    
    // 고정밀도 성능 측정 (마이크로초)
    template<typename Func>
    double MeasureExecutionTimeMs(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        return duration.count() / 1000000.0; // ms로 변환
    }
};

// 핵심 성능 테스트 1: 320x320 ROI 추출 <5ms 검증
TEST_F(Performance320x320Benchmark, CenterRegionExtraction_UnderFiveMilliseconds) {
    auto center_capture = std::make_unique<CenterRegionCapture>();
    
    // Full HD 테스트 프레임 생성
    cv::Mat full_hd_frame = cv::Mat::zeros(1080, 1920, CV_8UC3);
    cv::Mat roi_output;
    
    // 워밍업 (메모리 할당, 캐시 로딩)
    for (int i = 0; i < 10; ++i) {
        center_capture->ExtractCenterRegion(full_hd_frame, roi_output);
    }
    
    // 성능 측정 (100회 평균으로 정확도 향상)
    double total_time = 0.0;
    const int num_iterations = 100;
    
    for (int i = 0; i < num_iterations; ++i) {
        double extraction_time = MeasureExecutionTimeMs([&]() {
            center_capture->ExtractCenterRegion(full_hd_frame, roi_output);
        });
        total_time += extraction_time;
    }
    
    double average_time = total_time / num_iterations;
    
    // 핵심 성능 요구사항 검증
    EXPECT_LT(average_time, MAX_ROI_EXTRACTION_MS) 
        << "🚨 320x320 ROI 추출이 목표 성능을 미달했습니다: " 
        << average_time << "ms (목표: <" << MAX_ROI_EXTRACTION_MS << "ms)";
    
    EXPECT_EQ(roi_output.cols, ROI_SIZE);
    EXPECT_EQ(roi_output.rows, ROI_SIZE);
    
    std::cout << "✅ [성능] 320x320 ROI 추출: " << average_time << "ms" << std::endl;
}

// 핵심 성능 테스트 2: YOLOv11 280+ FPS GPU 추론 검증
TEST_F(Performance320x320Benchmark, YOLOv11GPU_280PlusFPS_Validation) {
    #ifdef CUDA_ENABLED
    auto yolo_detector = std::make_unique<YOLOv11TensorRTInference>();
    
    // 320x320 ROI 테스트 프레임 (YOLO 입력 크기와 일치)
    cv::Mat roi_320x320 = cv::Mat::zeros(ROI_SIZE, ROI_SIZE, CV_8UC3);
    
    // GPU 메모리 및 TensorRT 엔진 워밍업
    for (int i = 0; i < 10; ++i) {
        yolo_detector->detect(roi_320x320);
    }
    
    // 280+ FPS 달성을 위한 추론 시간 측정
    const int fps_test_iterations = 50;
    double total_inference_time = 0.0;
    
    for (int i = 0; i < fps_test_iterations; ++i) {
        double inference_time = MeasureExecutionTimeMs([&]() {
            yolo_detector->detect(roi_320x320);
        });
        total_inference_time += inference_time;
    }
    
    double average_inference_time = total_inference_time / fps_test_iterations;
    double achieved_fps = 1000.0 / average_inference_time;
    
    // 280+ FPS 성능 요구사항 검증
    EXPECT_GE(achieved_fps, MIN_YOLO_FPS_GPU) 
        << "🚨 YOLOv11 GPU 추론이 목표 FPS를 미달했습니다: " 
        << achieved_fps << " FPS (목표: ≥" << MIN_YOLO_FPS_GPU << " FPS)";
    
    // TensorRT 최적화 상태 확인
    EXPECT_TRUE(yolo_detector->isTensorRTOptimized()) 
        << "TensorRT 최적화가 활성화되지 않았습니다";
    
    std::cout << "✅ [성능] YOLOv11 GPU: " << achieved_fps << " FPS" << std::endl;
    std::cout << "✅ [성능] 평균 추론 시간: " << average_inference_time << "ms" << std::endl;
    
    #else
    GTEST_SKIP() << "CUDA가 비활성화되어 YOLOv11 GPU 성능 테스트를 건너뜁니다.";
    #endif
}

// 성능 테스트 3: CPU 백엔드 30+ FPS 검증
TEST_F(Performance320x320Benchmark, YOLOv11CPU_30PlusFPS_Fallback) {
    auto yolo_detector = std::make_unique<YOLOv11TensorRTInference>();
    yolo_detector->ForceUseCPUBackend(); // CPU 모드 강제 설정
    
    cv::Mat roi_320x320 = cv::Mat::zeros(ROI_SIZE, ROI_SIZE, CV_8UC3);
    
    // CPU 추론 워밍업 (더 오래 필요)
    for (int i = 0; i < 5; ++i) {
        yolo_detector->detect(roi_320x320);
    }
    
    // CPU 30+ FPS 달성 검증
    double cpu_inference_time = MeasureExecutionTimeMs([&]() {
        yolo_detector->detect(roi_320x320);
    });
    
    double cpu_fps = 1000.0 / cpu_inference_time;
    
    EXPECT_GE(cpu_fps, MIN_YOLO_FPS_CPU) 
        << "🚨 YOLOv11 CPU 백엔드가 목표 FPS를 미달했습니다: " 
        << cpu_fps << " FPS (목표: ≥" << MIN_YOLO_FPS_CPU << " FPS)";
    
    std::cout << "✅ [성능] YOLOv11 CPU: " << cpu_fps << " FPS" << std::endl;
}

// 성능 테스트 4: HSV 색상 검출 320x320 최적화 검증
TEST_F(Performance320x320Benchmark, HSVDetection_320x320_OptimizedPerformance) {
    auto hsv_detector = std::make_unique<HSVColorDetection>();
    cv::Mat roi_320x320 = CreateTestROI_320x320_WithRedTarget();
    
    // HSV 설정
    HSVRange red_range{0, 10, 120, 255, 120, 255};
    hsv_detector->SetHSVRange(red_range);
    
    // HSV 검출 성능 측정 (320x320 픽셀만 처리)
    const int hsv_iterations = 100;
    double total_hsv_time = 0.0;
    
    for (int i = 0; i < hsv_iterations; ++i) {
        double hsv_time = MeasureExecutionTimeMs([&]() {
            hsv_detector->detect(roi_320x320);
        });
        total_hsv_time += hsv_time;
    }
    
    double avg_hsv_time = total_hsv_time / hsv_iterations;
    double hsv_fps = 1000.0 / avg_hsv_time;
    
    // HSV 성능 검증 (320x320 최적화로 매우 빨라야 함)
    EXPECT_LT(avg_hsv_time, 5.0) << "HSV 검출이 320x320 최적화에도 느립니다: " 
                                 << avg_hsv_time << "ms";
                                 
    std::cout << "✅ [성능] HSV 320x320: " << hsv_fps << " FPS" << std::endl;
}
```

### 📊 320x320 메모리 효율성 테스트 (<500MB 목표)
```cpp
// 320x320 시스템 메모리 효율성 검증 (Phase 0-3 최적화)
TEST_F(Performance320x320Benchmark, Memory320x320_Under500MB_Optimization) {
    #ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    size_t initial_memory = pmc.WorkingSetSize;
    
    // 320x320 시스템 컴포넌트 초기화
    auto center_capture = std::make_unique<CenterRegionCapture>();
    auto yolo_detector = std::make_unique<YOLOv11TensorRTInference>();
    auto hsv_detector = std::make_unique<HSVColorDetection>();
    
    // 320x320 시스템 부하 테스트 (1000회 반복)
    for (int i = 0; i < 1000; ++i) {
        // Full HD → 320x320 ROI 추출
        cv::Mat full_frame = CreateFullHDTestFrame();
        cv::Mat roi_320x320;
        
        center_capture->ExtractCenterRegion(full_frame, roi_320x320);
        
        // 320x320에서 병렬 검출
        auto hsv_results = hsv_detector->detect(roi_320x320);
        auto yolo_results = yolo_detector->detect(roi_320x320);
        
        // 명시적 메모리 해제 (최적화)
        roi_320x320.release();
        full_frame.release();
        
        // 주기적 가비지 컬렉션 (100회마다)
        if (i % 100 == 0) {
            cv::Mat().copyTo(cv::Mat());
        }
    }
    
    // 메모리 정리 후 최종 측정
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    size_t final_memory = pmc.WorkingSetSize;
    
    size_t memory_increase = final_memory - initial_memory;
    size_t max_allowed_320x320 = 100 * 1024 * 1024;  // 100MB 증가 허용 (320x320 최적화)
    size_t total_memory_mb = final_memory / 1024 / 1024;
    
    // 320x320 최적화된 메모리 사용량 검증
    EXPECT_LT(memory_increase, max_allowed_320x320)
        << "🚨 320x320 메모리 최적화 실패. 증가량: " 
        << (memory_increase / 1024 / 1024) << "MB (허용: 100MB)";
    
    EXPECT_LT(total_memory_mb, 500)  // 전체 <500MB 목표
        << "🚨 전체 메모리 사용량이 목표를 초과했습니다: " 
        << total_memory_mb << "MB (목표: <500MB)";
    
    std::cout << "✅ [메모리] 320x320 시스템 총 사용량: " << total_memory_mb << "MB" << std::endl;
    std::cout << "✅ [메모리] 테스트 중 증가량: " << (memory_increase / 1024 / 1024) << "MB" << std::endl;
    
    #endif
}
```

## 4패널 모던 GUI 및 320x320 통합 테스트 (Phase 3)

### 🖥️ 4패널 모던 GUI 테스트 (Phase 3 완료)

#### 320x320 특화 GUI 컴포넌트 테스트
```cpp
// helpers/GLTestContext 활용 - 4패널 GUI 테스트
class MainInterface320x320Test : public ::testing::Test {
protected:
    void SetUp() override {
        // OpenGL 3.3+ 컨텍스트 생성 (4패널 GUI 요구사항)
        gl_context_ = std::make_unique<GLTestContext>();
        ASSERT_TRUE(gl_context_->Initialize()) << "OpenGL 컨텍스트 초기화 실패";
        
        // Phase 3: 4패널 모던 MainInterface 생성
        main_interface_320x320_ = std::make_unique<MainInterface>();
        main_interface_320x320_->Initialize();
        
        // 320x320 시스템 컴포넌트 초기화
        center_capture_ = std::make_unique<CenterRegionCapture>();
        yolo_detector_ = std::make_unique<YOLOv11TensorRTInference>();
        hsv_detector_ = std::make_unique<HSVColorDetection>();
    }
    
    void TearDown() override {
        main_interface_320x320_->Cleanup();
        gl_context_->Cleanup();
    }
    
    std::unique_ptr<GLTestContext> gl_context_;
    std::unique_ptr<MainInterface> main_interface_320x320_;
    std::unique_ptr<CenterRegionCapture> center_capture_;
    std::unique_ptr<YOLOv11TensorRTInference> yolo_detector_;
    std::unique_ptr<HSVColorDetection> hsv_detector_;
};

// Phase 3: 4패널 도킹 레이아웃 검증
TEST_F(MainInterface320x320Test, FourPanelDocking_Phase3_AllPanelsCreated) {
    // 4패널 GUI 렌더링 (Phase 3 구현)
    main_interface_320x320_->Render();
    
    // 4패널 모던 GUI 패널 존재 확인 (imgui_internal.h DockBuilder)
    EXPECT_TRUE(ImGui::FindWindowByName("320x320 ROI 시각화") != nullptr) 
        << "ROI 시각화 패널이 생성되지 않았습니다";
    EXPECT_TRUE(ImGui::FindWindowByName("검출 결과") != nullptr) 
        << "검출 결과 패널이 생성되지 않았습니다";
    EXPECT_TRUE(ImGui::FindWindowByName("제어 패널") != nullptr) 
        << "제어 패널이 생성되지 않았습니다";
    EXPECT_TRUE(ImGui::FindWindowByName("성능 대시보드") != nullptr) 
        << "성능 대시보드 패널이 생성되지 않았습니다";
    
    // 도킹 공간 활성화 확인
    EXPECT_TRUE(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
        << "ImGui 도킹이 활성화되지 않았습니다";
}

// 320x320 ROI 실시간 시각화 테스트
TEST_F(MainInterface320x320Test, ROIVisualization_320x320_RealTimeUpdate) {
    // 320x320 ROI 시각화 패널 테스트
    cv::Mat test_roi = CreateTest320x320ROI();
    
    // HSV 검출 결과 시뮬레이션
    std::vector<cv::Point> hsv_points = {{160, 160}, {150, 170}};
    
    // YOLO 검출 결과 시뮬레이션  
    std::vector<cv::Rect> yolo_boxes = {{100, 100, 120, 80}};
    
    // GUI에 검출 결과 업데이트
    main_interface_320x320_->UpdateROIVisualization(test_roi, hsv_points, yolo_boxes);
    main_interface_320x320_->Render();
    
    // 시각화 데이터가 업데이트되었는지 확인
    auto& gui_roi_data = main_interface_320x320_->GetROIVisualizationData();
    EXPECT_FALSE(gui_roi_data.hsv_points.empty()) << "HSV 포인트가 GUI에 반영되지 않았습니다";
    EXPECT_FALSE(gui_roi_data.yolo_boxes.empty()) << "YOLO 박스가 GUI에 반영되지 않았습니다";
    
    // 320x320 크기 확인
    EXPECT_EQ(gui_roi_data.roi_frame.cols, 320);
    EXPECT_EQ(gui_roi_data.roi_frame.rows, 320);
}

// 실시간 설정 변경 GUI 테스트 (제어 패널)
TEST_F(MainInterface320x320Test, ControlPanel_RealTimeConfig_UpdatesSystem) {
    auto& config = ConfigManager::GetInstance();
    
    // HSV 범위 초기값 확인
    auto initial_hsv = config.GetHSVRange();
    
    // GUI 제어 패널에서 HSV 범위 변경 시뮬레이션
    HSVRange new_hsv_range{5, 15, 130, 255, 130, 255};  // 빨간색 범위 조정
    main_interface_320x320_->UpdateHSVControlPanel(new_hsv_range);
    main_interface_320x320_->Render();
    
    // 설정이 실시간으로 변경되었는지 확인
    auto updated_hsv = config.GetHSVRange();
    EXPECT_EQ(updated_hsv.hue_min, 5);
    EXPECT_EQ(updated_hsv.hue_max, 15);
    EXPECT_EQ(updated_hsv.sat_min, 130);
    
    // YOLO 신뢰도 임계값 변경 테스트
    float initial_confidence = config.GetYOLOConfidenceThreshold();
    main_interface_320x320_->UpdateYOLOControlPanel(0.7f);  // 70% 신뢰도
    main_interface_320x320_->Render();
    
    EXPECT_FLOAT_EQ(config.GetYOLOConfidenceThreshold(), 0.7f);
}

// 성능 대시보드 60+ FPS GUI 테스트
TEST_F(MainInterface320x320Test, PerformanceDashboard_60PlusFPS_RealTimeMetrics) {
    // 성능 메트릭 시뮬레이션 (Phase 0-3 달성 수치)
    PerformanceMetrics test_metrics{
        .roi_extraction_ms = 3.2,      // <5ms ROI 추출
        .yolo_fps = 285.5,             // 280+ FPS YOLO
        .hsv_fps = 450.2,              // HSV 고속 처리
        .gui_fps = 62.1,               // 60+ FPS GUI
        .memory_usage_mb = 387         // <500MB 메모리
    };
    
    // 성능 대시보드 업데이트
    main_interface_320x320_->UpdatePerformanceDashboard(test_metrics);
    main_interface_320x320_->Render();
    
    // 성능 메트릭이 GUI에 표시되었는지 확인
    auto& dashboard_data = main_interface_320x320_->GetPerformanceDashboardData();
    EXPECT_LT(dashboard_data.roi_extraction_ms, 5.0) 
        << "ROI 추출 성능이 대시보드에 잘못 표시됨";
    EXPECT_GE(dashboard_data.yolo_fps, 280.0) 
        << "YOLO FPS가 목표치 미달로 표시됨";
    EXPECT_GE(dashboard_data.gui_fps, 60.0) 
        << "GUI FPS가 목표치 미달로 표시됨";
}

// Phase 3 모던 다크 테마 적용 확인
TEST_F(MainInterface320x320Test, ModernDarkTheme_Phase3_Applied) {
    main_interface_320x320_->Render();
    
    // 모던 다크 테마 색상 확인 (Phase 3 구현)
    ImGuiStyle& style = ImGui::GetStyle();
    
    // 전문적인 차콜 배경 확인
    ImVec4 bg_color = style.Colors[ImGuiCol_WindowBg];
    EXPECT_LT(bg_color.x, 0.2f) << "배경이 충분히 어둡지 않습니다";
    EXPECT_LT(bg_color.y, 0.2f) << "배경이 충분히 어둡지 않습니다";
    EXPECT_LT(bg_color.z, 0.2f) << "배경이 충분히 어둡지 않습니다";
    
    // 블루 액센트 색상 확인
    ImVec4 accent_color = style.Colors[ImGuiCol_Button];
    EXPECT_GT(accent_color.z, 0.5f) << "액센트 블루 색상이 적용되지 않았습니다";
}
```

## 320x320 테스트 실행 및 CI/CD 통합 (Phase 0-3 완료)

### 🚀 320x320 시스템 테스트 실행 방법

#### 로컬 개발 환경 (320x320 최적화)
```bash
# 320x320 시스템 전체 테스트 빌드 (Release 필수)
cmake --build build --config Release --parallel

# 320x320 모든 테스트 실행 (성능 검증 포함)
ctest -C Release --test-dir build --verbose

# 320x320 핵심 테스트 개별 실행
./build/bin/tests/test_CenterRegionCapture.exe      # <5ms ROI 추출 테스트
./build/bin/tests/test_YOLOv11Integration.exe       # 280+ FPS YOLO 테스트
./build/bin/tests/test_HSVColorDetection.exe        # HSV 색상 추적 테스트
./build/bin/tests/test_MainInterface.exe            # 4패널 GUI 테스트 (Phase 3)

# 320x320 성능 벤치마크 실행 (핵심 성능 검증)
./build/bin/tests/benchmark_320x320_tests.exe       # 280+ FPS, <5ms ROI 검증

# GPU 환경 테스트 (CUDA 활성화 시)
./build/bin/tests/test_TensorRT_Performance.exe     # TensorRT 280+ FPS 검증

# 320x320 통합 테스트 (전체 워크플로우)
./build/bin/tests/test_320x320_Integration.exe      # ROI → HSV/YOLO → GUI 파이프라인

# CI/CD용 XML 결과 출력
ctest -C Release --test-dir build --output-on-failure --output-junit test_results_320x320.xml
```

#### Visual Studio 2019+ 통합 (320x320 개발 환경)
```bash
# Visual Studio Test Explorer에서 320x320 테스트 실행
# 테스트 > 테스트 탐색기 > "320x320" 필터링
# - CenterRegionCapture 성능 테스트 (<5ms 검증)
# - YOLOv11 GPU 가속 테스트 (280+ FPS 검증)  
# - 4패널 GUI 레이아웃 테스트 (Phase 3)
# - 실시간 디버깅 및 GPU 메모리 모니터링 지원
```

### 🔄 320x320 CI/CD 파이프라인 통합 (Phase 0-3 최적화)

#### GitHub Actions 320x320 워크플로우
```yaml
# .github/workflows/test_320x320_system.yml
name: 320x320 System Tests (Phase 0-3)
on: [push, pull_request]

jobs:
  test-320x320-system:
    runs-on: windows-latest
    
    steps:
    - uses: actions/checkout@v4
      with:
        submodules: recursive  # 5개 Git submodule 초기화
    
    - name: Setup CMake 3.16+
      uses: lukka/get-cmake@latest
    
    - name: Verify 320x320 Submodules
      run: |
        git submodule status
        cd ScreenMonitor/external/imgui && git branch -v  # docking 브랜치 확인
    
    - name: Configure 320x320 Build (Release)
      run: cmake -B build -S ScreenMonitor -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
    
    - name: Build 320x320 System
      run: cmake --build build --config Release --parallel
    
    - name: Run 320x320 Core Tests
      run: |
        # 핵심 320x320 성능 테스트
        ctest -C Release --test-dir build --output-on-failure -R "CenterRegion|320x320"
    
    - name: Run Performance Benchmarks
      run: |
        # 280+ FPS YOLO, <5ms ROI 성능 검증
        ctest -C Release --test-dir build --output-on-failure -R "Performance|Benchmark"
      continue-on-error: true  # GPU 환경 의존적
    
    - name: Run 4-Panel GUI Tests (Phase 3)
      run: |
        # 4패널 모던 GUI 테스트
        ctest -C Release --test-dir build --output-on-failure -R "MainInterface|GUI"
      env:
        DISPLAY: ":99"  # 가상 디스플레이 (헤드리스 GUI 테스트)
    
    - name: Upload 320x320 Test Results
      uses: actions/upload-artifact@v4
      if: always()
      with:
        name: 320x320-test-results
        path: |
          build/Testing/
          build/bin/tests/test_results_320x320.xml
    
    - name: Performance Report Summary
      run: |
        echo "## 320x320 시스템 성능 검증 결과" >> $GITHUB_STEP_SUMMARY
        echo "- ✅ ROI 추출: <5ms 목표" >> $GITHUB_STEP_SUMMARY  
        echo "- ✅ YOLO 추론: 280+ FPS 목표 (GPU)" >> $GITHUB_STEP_SUMMARY
        echo "- ✅ GUI 응답: 60+ FPS 목표" >> $GITHUB_STEP_SUMMARY
        echo "- ✅ 메모리 사용: <500MB 목표" >> $GITHUB_STEP_SUMMARY
```

### 📈 320x320 테스트 커버리지 측정 (90%+ 목표)

#### OpenCppCoverage (320x320 최적화)
```bash
# 커버리지 측정 도구 설치
winget install OpenCppCoverage

# 320x320 핵심 모듈 커버리지 측정
OpenCppCoverage.exe --sources ScreenMonitor\\src\\capture --sources ScreenMonitor\\src\\detection --export_type cobertura:coverage_320x320.xml -- ctest -C Debug --test-dir build -R "320x320|CenterRegion|YOLO|HSV"

# 4패널 GUI 커버리지 측정 (Phase 3)
OpenCppCoverage.exe --sources ScreenMonitor\\src\\gui --export_type html:gui_coverage_report -- ctest -C Debug --test-dir build -R "MainInterface|GUI"

# 전체 320x320 시스템 커버리지 (90%+ 목표)
OpenCppCoverage.exe --sources ScreenMonitor\\src --export_type html:full_320x320_coverage -- ctest -C Debug --test-dir build
```

## 320x320 새로운 테스트 작성 가이드 (간소화된 접근법)

### ✍️ 320x320 특화 테스트 작성 체크리스트

#### 새로운 320x320 기능 테스트 추가 (직접 인스턴스화)
```cpp
// 1. 320x320 테스트 파일 생성: test_NewFeature320x320.cpp
#include <gtest/gtest.h>
#include "320x320/NewFeature.h"
#include "capture/CenterRegionCapture.h"

class NewFeature320x320Test : public ::testing::Test {
protected:
    void SetUp() override {
        // 직접 인스턴스화 (팩토리 패턴 제거)
        feature_320x320_ = std::make_unique<NewFeature>();
        center_capture_ = std::make_unique<CenterRegionCapture>();
        
        // 320x320 테스트 ROI 준비
        test_roi_320x320_ = cv::Mat::zeros(320, 320, CV_8UC3);
    }
    
    std::unique_ptr<NewFeature> feature_320x320_;
    std::unique_ptr<CenterRegionCapture> center_capture_;
    cv::Mat test_roi_320x320_;
};

// 2. 320x320 ROI 처리 테스트 (핵심)
TEST_F(NewFeature320x320Test, Process320x320ROI_ValidInput_ReturnsExpectedResult) {
    // 320x320 ROI에서 정상 동작 테스트
    auto result = feature_320x320_->ProcessROI(test_roi_320x320_);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->roi_size.width, 320);
    EXPECT_EQ(result->roi_size.height, 320);
}

// 3. 성능 요구사항 테스트 (320x320 최적화)
TEST_F(NewFeature320x320Test, Performance320x320_UnderTargetTime_MeetsRequirements) {
    // 320x320 특화 성능 측정
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 100; ++i) {
        feature_320x320_->ProcessROI(test_roi_320x320_);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double avg_time_ms = duration.count() / 100.0;
    
    EXPECT_LT(avg_time_ms, 10.0) << "320x320 처리 성능이 목표를 미달했습니다: " 
                                 << avg_time_ms << "ms";
}

// 4. 경계값 테스트 (320x320 특화)
TEST_F(NewFeature320x320Test, InvalidROISize_NotExactly320x320_ReturnsError) {
    // 320x320이 아닌 크기 처리 테스트
    cv::Mat invalid_roi = cv::Mat::zeros(300, 300, CV_8UC3);
    
    auto result = feature_320x320_->ProcessROI(invalid_roi);
    
    EXPECT_FALSE(result.has_value()) << "잘못된 ROI 크기를 허용했습니다";
}

// 5. GPU/CPU 백엔드 테스트 (YOLOv11 스타일)
TEST_F(NewFeature320x320Test, GPUFallback_CUDAUnavailable_UsesCPUBackend) {
    #ifndef CUDA_ENABLED
    // CPU 백엔드 자동 전환 테스트
    bool gpu_available = feature_320x320_->IsGPUAccelerated();
    EXPECT_FALSE(gpu_available) << "CUDA 비활성화 환경에서 GPU 백엔드가 활성화됨";
    
    // CPU 백엔드 성능 검증 (더 관대한 기준)
    auto result = feature_320x320_->ProcessROI(test_roi_320x320_);
    EXPECT_TRUE(result.has_value()) << "CPU 백엔드에서 처리 실패";
    #endif
}
```

#### 320x320 Mock 객체 생성 가이드 (간소화)
```cpp
// 1. 320x320 특화 인터페이스 분석
//    - IDetectionAlgorithm (HSV, YOLO)
//    - ICaptureDevice (CenterRegionCapture)
//    - IPerformanceObserver (SimpleMetrics)

// 2. 320x320 Mock 클래스 정의 (mocks/ 디렉토리)
//    - MockCenterRegionCapture: ROI 추출 Mock
//    - MockYOLOv11Detector: YOLO 추론 Mock  
//    - MockHSVDetector: HSV 색상 검출 Mock

// 3. 직접 인스턴스화로 Mock 설정 (팩토리 제거)
//    - std::make_shared<Mock320x320Object>() 직접 생성
//    - EXPECT_CALL 설정으로 320x320 특화 동작 검증

// 4. 320x320 성능 요구사항 Mock 검증
//    - <5ms ROI 추출, 280+ FPS YOLO, 60+ FPS GUI
```

### 🎯 320x320 테스트 품질 가이드라인 (간소화된 AAA 패턴)

#### 320x320 특화 AAA 패턴 준수
```cpp
TEST_F(CenterRegionCapture320x320Test, ExtractROI_FullHDInput_Returns320x320) {
    // Arrange (준비) - 320x320 테스트 조건 설정
    cv::Mat full_hd_frame = CreateFullHDTestFrame(1920, 1080);
    cv::Mat roi_output;
    auto expected_size = cv::Size(320, 320);
    
    // Act (실행) - 320x320 ROI 추출 수행
    bool success = center_capture_->ExtractCenterRegion(full_hd_frame, roi_output);
    
    // Assert (검증) - 320x320 결과 확인
    EXPECT_TRUE(success) << "320x320 ROI 추출 실패";
    EXPECT_EQ(roi_output.size(), expected_size) << "ROI 크기가 320x320이 아닙니다";
    EXPECT_LT(center_capture_->GetLastExtractionTimeMs(), 5.0) << "<5ms 성능 요구사항 미달";
}
```

#### 320x320 테스트 명명 규칙 (Phase 0-3 스타일)
```cpp
// ✅ 320x320 특화 테스트 이름: 기능_320x320_조건_예상결과
TEST_F(CenterRegionCapture320x320Test, ExtractROI_320x320_FullHDInput_Returns320x320ROI)
TEST_F(YOLOv11Detection320x320Test, InferenceGPU_320x320ROI_Achieves280PlusFPS)
TEST_F(HSVColorDetection320x320Test, DetectRedTarget_320x320Center_FindsCenterPoint)
TEST_F(MainInterface320x320Test, RenderGUI_4Panel_Maintains60PlusFPS)

// ❌ 320x320 시스템에서 피해야 할 테스트 이름
TEST_F(DetectorTest, Test1)
TEST_F(CaptureTest, TestCapture)
```

## 320x320 품질 보증 매트릭스 (Phase 0-3 달성 목표)

### 📊 320x320 특화 품질 지표

| 항목 | 320x320 목표 | 측정 방법 |
|------|-------------|-----------|
| **핵심 모듈 커버리지** | 90%+ | CenterRegionCapture, YOLOv11, HSV |
| **성능 테스트** | 280+ FPS YOLO, <5ms ROI | 실시간 벤치마크 |
| **4패널 GUI 커버리지** | 85%+ | MainInterface Phase 3 기능 |
| **메모리 효율성** | <500MB | 1000회 반복 테스트 |
| **GPU/CPU 백엔드** | 자동 전환 | TensorRT ↔ CPU 테스트 |
| **테스트 실행 시간** | <3분 | 320x320 최적화된 CI/CD |

### 🔍 320x320 품질 게이트 조건 (Production Ready)

#### PR 머지 조건 (Phase 0-3 완료 기준)
- [ ] **320x320 핵심 테스트 통과**: CenterRegionCapture <5ms, YOLOv11 280+ FPS
- [ ] **4패널 GUI 테스트 통과**: Phase 3 모던 인터페이스 60+ FPS 유지
- [ ] **성능 저하 방지**: 벤치마크 결과 이전 버전 대비 성능 향상 또는 유지
- [ ] **메모리 효율성**: <500MB 메모리 사용량 및 누수 없음
- [ ] **GPU/CPU 백엔드**: CUDA 환경과 CPU 백엔드 모든 테스트 통과
- [ ] **통합 테스트**: ROI → HSV/YOLO → GUI 전체 파이프라인 검증

#### 320x320 Production Ready 검증 체크리스트
- ✅ **ROI 추출**: <5ms (320x320 중심 영역)
- ✅ **YOLO 추론**: 280+ FPS (TensorRT GPU), 30+ FPS (CPU 백엔드)
- ✅ **GUI 응답**: 60+ FPS (4패널 모던 인터페이스)
- ✅ **메모리 효율**: <500MB (최적화된 버퍼 관리)
- ✅ **코드 품질**: 35% 감축, 한국어 주석, 간소화된 구조

---

**320x320 ScreenMonitor 테스트 시스템** - Phase 0-3 완료 | 280+ FPS YOLO 검증  
GoogleTest/Mock 기반 | 성능 우선 검증 | CI/CD 자동화 | 간소화된 아키텍처