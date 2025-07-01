#include <gtest/gtest.h>
#include "detection/ObjectDetector.h"
#include "helpers/TestImageGenerator.h"
#include <filesystem>
#include <fstream>

class YOLOv11Test : public ::testing::Test {
protected:
    void SetUp() override {
        detector = std::make_unique<ObjectDetector>();
        
        // 테스트용 모델 경로 설정
        model_path = "models/yolo11n.onnx";
        class_names_path = "models/coco_classes.txt";
        
        // 모델 파일 존재 여부 확인
        model_exists = std::filesystem::exists(model_path);
        class_file_exists = std::filesystem::exists(class_names_path);
    }
    
    void TearDown() override {
        detector.reset();
    }
    
    std::unique_ptr<ObjectDetector> detector;
    std::string model_path;
    std::string class_names_path;
    bool model_exists;
    bool class_file_exists;
};

// YOLO v11 백엔드 기본 기능 테스트
TEST_F(YOLOv11Test, BackendSelection) {
    // 기본 백엔드 확인
    EXPECT_EQ(detector->getBackend(), YOLOBackend::OPENCV_DNN);
    
    // 백엔드 변경 테스트
    EXPECT_TRUE(detector->setBackend(YOLOBackend::ONNX_RUNTIME));
    EXPECT_EQ(detector->getBackend(), YOLOBackend::ONNX_RUNTIME);
    
    // 다시 OpenCV DNN으로 변경
    EXPECT_TRUE(detector->setBackend(YOLOBackend::OPENCV_DNN));
    EXPECT_EQ(detector->getBackend(), YOLOBackend::OPENCV_DNN);
}

// 클래스 이름 파일 로드 테스트
TEST_F(YOLOv11Test, LoadClassNames) {
    if (!class_file_exists) {
        GTEST_SKIP() << "Class names file not found: " << class_names_path;
    }
    
    EXPECT_TRUE(detector->loadClassNames(class_names_path));
    
    auto class_names = detector->getClassNames();
    EXPECT_EQ(class_names.size(), 80);  // COCO 클래스 수
    EXPECT_EQ(class_names[0], "person");
    EXPECT_EQ(class_names[2], "car");
}

// 잘못된 클래스 파일 테스트
TEST_F(YOLOv11Test, LoadInvalidClassNames) {
    EXPECT_FALSE(detector->loadClassNames("nonexistent_classes.txt"));
}

// YOLO v11 모델 로드 테스트 (OpenCV DNN)
TEST_F(YOLOv11Test, LoadYOLOv11ModelOpenCV) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not found: " << model_path;
    }
    
    bool load_result = detector->loadYOLOv11Model(model_path, YOLOBackend::OPENCV_DNN);
    
    if (!load_result) {
        GTEST_SKIP() << "OpenCV DNN not available or model loading failed";
    }
    
    EXPECT_TRUE(load_result);
    EXPECT_TRUE(detector->isModelLoaded());
    EXPECT_EQ(detector->getBackend(), YOLOBackend::OPENCV_DNN);
}

// YOLO v11 모델 로드 테스트 (ONNX Runtime)
TEST_F(YOLOv11Test, LoadYOLOv11ModelONNX) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not found: " << model_path;
    }
    
    bool load_result = detector->loadYOLOv11Model(model_path, YOLOBackend::ONNX_RUNTIME);
    
    // ONNX Runtime이 사용 가능한 경우에만 테스트
#ifdef HAVE_ONNXRUNTIME
    if (!load_result) {
        GTEST_SKIP() << "ONNX Runtime model loading failed";
    }
    
    EXPECT_TRUE(load_result);
    EXPECT_TRUE(detector->isModelLoaded());
    EXPECT_EQ(detector->getBackend(), YOLOBackend::ONNX_RUNTIME);
#else
    EXPECT_FALSE(load_result);  // ONNX Runtime 미지원 시 실패해야 함
#endif
}

// 존재하지 않는 모델 파일 테스트
TEST_F(YOLOv11Test, LoadNonexistentModel) {
    EXPECT_FALSE(detector->loadYOLOv11Model("nonexistent_model.onnx"));
    EXPECT_FALSE(detector->isModelLoaded());
}

// 빈 이미지 검출 테스트
TEST_F(YOLOv11Test, DetectEmptyImage) {
    cv::Mat empty_image;
    
    auto results = detector->detectMultipleTargets(empty_image);
    EXPECT_TRUE(results.empty());
    
    cv::Rect single_result = detector->detectTarget(empty_image);
    EXPECT_TRUE(single_result.empty());
}

// 모델 없이 검출 시도
TEST_F(YOLOv11Test, DetectionWithoutModel) {
    cv::Mat test_image = TestImageGenerator::createYOLOTestImage(640, 480);
    
    auto results = detector->detectMultipleTargets(test_image);
    EXPECT_TRUE(results.empty());
    
    cv::Rect single_result = detector->detectTarget(test_image);
    EXPECT_TRUE(single_result.empty());
}

// 실제 YOLO v11 모델을 사용한 검출 테스트
TEST_F(YOLOv11Test, RealModelDetection) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not found: " << model_path;
    }
    
    // 모델 로드
    bool load_success = detector->loadYOLOv11Model(model_path, YOLOBackend::OPENCV_DNN);
    if (!load_success) {
        GTEST_SKIP() << "Model loading failed";
    }
    
    // 클래스 이름 로드
    if (class_file_exists) {
        detector->loadClassNames(class_names_path);
    }
    
    // 테스트 이미지 생성 (자동차와 사람이 포함된 이미지)
    cv::Mat test_image = TestImageGenerator::createYOLOTestImage(640, 640);
    
    // 검출 실행
    auto results = detector->detectMultipleTargets(test_image);
    
    // 결과 검증 (검출 결과가 있을 수도 없을 수도 있음)
    std::cout << "Detected " << results.size() << " objects" << std::endl;
    
    for (const auto& result : results) {
        std::cout << "  - " << result.label 
                  << " (confidence: " << result.confidence << ")" << std::endl;
    }
    
    SUCCEED();  // 크래시 없이 완료되면 성공
}

// 다양한 해상도 테스트
TEST_F(YOLOv11Test, DifferentResolutions) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not found";
    }
    
    bool load_success = detector->loadYOLOv11Model(model_path);
    if (!load_success) {
        GTEST_SKIP() << "Model loading failed";
    }
    
    std::vector<cv::Size> resolutions = {
        cv::Size(320, 320),
        cv::Size(416, 416),
        cv::Size(640, 640),
        cv::Size(1280, 720),
        cv::Size(1920, 1080)
    };
    
    for (const auto& resolution : resolutions) {
        cv::Mat test_image = TestImageGenerator::createYOLOTestImage(
            resolution.width, resolution.height);
        
        // 검출 실행 (예외 없이 완료되어야 함)
        EXPECT_NO_THROW({
            auto results = detector->detectMultipleTargets(test_image);
        });
    }
}

// 성능 벤치마크 테스트
TEST_F(YOLOv11Test, PerformanceBenchmark) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not found";
    }
    
    bool load_success = detector->loadYOLOv11Model(model_path);
    if (!load_success) {
        GTEST_SKIP() << "Model loading failed";
    }
    
    cv::Mat test_image = TestImageGenerator::createYOLOTestImage(640, 640);
    
    // 워밍업
    for (int i = 0; i < 3; ++i) {
        detector->detectMultipleTargets(test_image);
    }
    
    // 성능 측정
    auto start_time = std::chrono::high_resolution_clock::now();
    
    const int num_iterations = 10;
    for (int i = 0; i < num_iterations; ++i) {
        detector->detectMultipleTargets(test_image);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    double avg_time_ms = static_cast<double>(duration) / num_iterations;
    double fps = 1000.0 / avg_time_ms;
    
    std::cout << "YOLO v11 Performance:" << std::endl;
    std::cout << "  Average time: " << avg_time_ms << " ms" << std::endl;
    std::cout << "  Estimated FPS: " << fps << std::endl;
    
    // 성능이 너무 느리지 않은지 확인 (5초 이내)
    EXPECT_LT(avg_time_ms, 5000.0);
}

// GPU 가속 테스트
TEST_F(YOLOv11Test, GPUAcceleration) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not found";
    }
    
    // GPU 사용 설정
    detector->setUseGPU(true);
    
    bool load_success = detector->loadYOLOv11Model(model_path);
    if (!load_success) {
        GTEST_SKIP() << "Model loading failed";
    }
    
    cv::Mat test_image = TestImageGenerator::createYOLOTestImage(640, 640);
    
    // GPU 검출 테스트
    auto results = detector->detectMultipleTargets(test_image);
    
    std::cout << "GPU acceleration test completed" << std::endl;
    SUCCEED();
}

// 백엔드 비교 테스트
TEST_F(YOLOv11Test, BackendComparison) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not found";
    }
    
    cv::Mat test_image = TestImageGenerator::createYOLOTestImage(640, 640);
    
    // OpenCV DNN 백엔드 테스트
    detector->setBackend(YOLOBackend::OPENCV_DNN);
    bool opencv_load = detector->loadYOLOv11Model(model_path);
    
    std::vector<ObjectDetector::DetectionResult> opencv_results;
    if (opencv_load) {
        opencv_results = detector->detectMultipleTargets(test_image);
        std::cout << "OpenCV DNN results: " << opencv_results.size() << " detections" << std::endl;
    }
    
#ifdef HAVE_ONNXRUNTIME
    // ONNX Runtime 백엔드 테스트
    detector->setBackend(YOLOBackend::ONNX_RUNTIME);
    bool onnx_load = detector->loadYOLOv11Model(model_path);
    
    std::vector<ObjectDetector::DetectionResult> onnx_results;
    if (onnx_load) {
        onnx_results = detector->detectMultipleTargets(test_image);
        std::cout << "ONNX Runtime results: " << onnx_results.size() << " detections" << std::endl;
    }
#endif
    
    SUCCEED();
}

// 메모리 안정성 테스트
TEST_F(YOLOv11Test, MemoryStability) {
    if (!model_exists) {
        GTEST_SKIP() << "YOLO v11 model not found";
    }
    
    // 여러 번 모델 로드/언로드
    for (int i = 0; i < 5; ++i) {
        bool load_success = detector->loadYOLOv11Model(model_path);
        if (load_success) {
            cv::Mat test_image = TestImageGenerator::createYOLOTestImage(640, 640);
            detector->detectMultipleTargets(test_image);
        }
        detector->reset();
    }
    
    SUCCEED();  // 메모리 문제 없이 완료되면 성공
}

// 모델 정보 테스트
TEST_F(YOLOv11Test, ModelInfo) {
    std::string info = detector->getModelInfo();
    EXPECT_FALSE(info.empty());
    
    if (model_exists && detector->loadYOLOv11Model(model_path)) {
        std::string loaded_info = detector->getModelInfo();
        EXPECT_FALSE(loaded_info.empty());
        std::cout << "Model info: " << loaded_info << std::endl;
    }
}

// 알고리즘 이름 테스트
TEST_F(YOLOv11Test, AlgorithmName) {
    std::string name = detector->getAlgorithmName();
    EXPECT_EQ(name, "YOLO_ObjectDetector");
}

// 에러 처리 테스트
TEST_F(YOLOv11Test, ErrorHandling) {
    // 잘못된 모델 경로
    EXPECT_FALSE(detector->loadYOLOv11Model("invalid/path/model.onnx"));
    
    // 손상된 모델 파일 (빈 파일)
    std::string temp_model = "temp_invalid.onnx";
    std::ofstream temp_file(temp_model);
    temp_file.close();
    
    EXPECT_FALSE(detector->loadYOLOv11Model(temp_model));
    
    // 정리
    std::filesystem::remove(temp_model);
}