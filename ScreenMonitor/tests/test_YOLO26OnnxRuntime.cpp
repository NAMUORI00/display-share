#include <gtest/gtest.h>

#include "detection/YOLO26OnnxRuntimeInference.h"

namespace {
YOLO26OnnxRuntimeInference::Settings CreateMissingModelSettings() {
    YOLO26OnnxRuntimeInference::Settings settings;
    settings.onnx_model_path = "models/does_not_exist.onnx";
    settings.class_names_path = "models/coco_classes.txt";
    settings.max_detections = 25;
    settings.input_width = 640;
    settings.input_height = 640;
    settings.selected_gpu_id = 0;
    settings.execution_providers = {"cuda", "cpu"};
    return settings;
}
}  // namespace

TEST(YOLO26OnnxRuntimeInferenceTest, ReportsAlgorithmMetadata) {
    YOLO26OnnxRuntimeInference detector;
    EXPECT_EQ(detector.GetAlgorithmName(), "YOLO26 ONNX Runtime Inference");
    EXPECT_EQ(detector.GetActiveProviderName(), "disabled");
    EXPECT_FALSE(detector.IsInitialized());
    EXPECT_FALSE(detector.IsUsingCpuFallback());
    EXPECT_EQ(detector.GetSelectedGpuId(), 0);
}

TEST(YOLO26OnnxRuntimeInferenceTest, RejectsMissingModelPath) {
    YOLO26OnnxRuntimeInference detector;
    auto settings = CreateMissingModelSettings();

    EXPECT_FALSE(detector.Initialize(settings));
    EXPECT_FALSE(detector.IsInitialized());
    EXPECT_FALSE(detector.GetLastError().empty());
}

TEST(YOLO26OnnxRuntimeInferenceTest, EnumeratesGpuDevicesWithoutThrowing) {
    YOLO26OnnxRuntimeInference detector;
    EXPECT_NO_THROW({
        const auto devices = detector.GetAvailableProviders();
        for (const auto& device : devices) {
            EXPECT_GE(device.device_id, 0);
            EXPECT_FALSE(device.name.empty());
        }
    });
}
