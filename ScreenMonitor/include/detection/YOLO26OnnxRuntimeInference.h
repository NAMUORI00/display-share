#pragma once

#include "../interfaces/IDetectionAlgorithm.h"
#include <onnxruntime_cxx_api.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class YOLO26OnnxRuntimeInference : public IDetectionAlgorithm {
public:
    struct ProviderInfo {
        int device_id = -1;
        std::string name;
    };

    struct Settings : public AlgorithmSettings {
        std::string onnx_model_path = "models/yolo26n.onnx";
        std::string class_names_path = "models/coco_classes.txt";
        int input_width = 640;
        int input_height = 640;
        int max_detections = 100;
        int selected_gpu_id = 0;
        std::vector<std::string> execution_providers = {"cuda", "cpu"};
    };

    YOLO26OnnxRuntimeInference();
    ~YOLO26OnnxRuntimeInference() override;

    bool Initialize(const AlgorithmSettings& settings) override;
    DetectionResult DetectSingle(const cv::Mat& image) override;
    std::vector<DetectionResult> DetectMultiple(const cv::Mat& image) override;
    std::vector<DetectionResult> DetectInROI(const cv::Mat& image, const cv::Rect& roi) override;
    bool UpdateSettings(const AlgorithmSettings& settings) override;
    std::string GetAlgorithmName() const override;
    std::string GetVersion() const override;
    std::vector<std::string> GetSupportedClasses() const override;
    std::string GetPerformanceStats() const override;
    void Reset() override;
    void Cleanup() override;

    std::vector<ProviderInfo> GetAvailableProviders() const;
    std::string GetActiveProviderName() const;
    bool IsUsingCpuFallback() const;
    bool IsInitialized() const;
    int GetSelectedGpuId() const;
    std::string GetLastError() const;

private:
    struct LetterboxMetadata {
        float scale = 1.0f;
        float pad_x = 0.0f;
        float pad_y = 0.0f;
        int original_width = 0;
        int original_height = 0;
    };

    struct BindingNames {
        std::vector<std::string> input_storage;
        std::vector<std::string> output_storage;
        std::vector<const char*> input_names;
        std::vector<const char*> output_names;
    };

    Settings settings_;
    bool is_initialized_ = false;
    bool using_cpu_fallback_ = false;
    std::string active_provider_name_ = "disabled";
    std::string last_error_;
    std::vector<ProviderInfo> available_providers_;
    std::vector<std::string> class_names_;

    mutable std::mutex inference_mutex_;
    mutable std::mutex metrics_mutex_;
    mutable std::atomic<uint64_t> total_inferences_{0};
    mutable double total_inference_time_ms_ = 0.0;
    mutable double average_inference_time_ms_ = 0.0;

    Ort::Env env_;
    std::unique_ptr<Ort::Session> session_;
    BindingNames binding_names_;

    bool InitializeSession(bool prefer_cuda);
    bool ValidateSettings(const Settings& settings, std::string& error) const;
    bool LoadClassNames();
    bool RefreshAvailableProviders();
    bool SupportsProvider(const std::string& provider_name) const;
    std::vector<float> PreprocessImage(const cv::Mat& image, LetterboxMetadata& metadata) const;
    std::vector<DetectionResult> RunSession(const cv::Mat& image);
    std::vector<DetectionResult> ParseOutputTensor(Ort::Value& output_tensor, const LetterboxMetadata& metadata) const;
    DetectionResult MakeDetectionResult(const float* row, size_t row_width, const LetterboxMetadata& metadata) const;
    void UpdatePerformanceMetrics(double inference_time_ms) const;
};
