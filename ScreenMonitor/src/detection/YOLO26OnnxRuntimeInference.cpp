#include "detection/YOLO26OnnxRuntimeInference.h"

#include "core/Constants.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <onnxruntime_c_api.h>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

using CuInitFn = int (*)(unsigned int);
using CuDeviceGetCountFn = int (*)(int*);
using CuDeviceGetFn = int (*)(int*, int);
using CuDeviceGetNameFn = int (*)(char*, int, int);

std::vector<YOLO26OnnxRuntimeInference::ProviderInfo> DiscoverCudaDevices() {
    std::vector<YOLO26OnnxRuntimeInference::ProviderInfo> devices;
    HMODULE cuda_driver = LoadLibraryA("nvcuda.dll");
    if (!cuda_driver) {
        return devices;
    }

    const auto cu_init = reinterpret_cast<CuInitFn>(GetProcAddress(cuda_driver, "cuInit"));
    const auto cu_device_get_count = reinterpret_cast<CuDeviceGetCountFn>(GetProcAddress(cuda_driver, "cuDeviceGetCount"));
    const auto cu_device_get = reinterpret_cast<CuDeviceGetFn>(GetProcAddress(cuda_driver, "cuDeviceGet"));
    const auto cu_device_get_name = reinterpret_cast<CuDeviceGetNameFn>(GetProcAddress(cuda_driver, "cuDeviceGetName"));

    if (!cu_init || !cu_device_get_count || !cu_device_get || !cu_device_get_name || cu_init(0) != 0) {
        FreeLibrary(cuda_driver);
        return devices;
    }

    int device_count = 0;
    if (cu_device_get_count(&device_count) != 0) {
        FreeLibrary(cuda_driver);
        return devices;
    }

    for (int index = 0; index < device_count; ++index) {
        int device = 0;
        if (cu_device_get(&device, index) != 0) {
            continue;
        }

        char device_name[128] = {};
        if (cu_device_get_name(device_name, static_cast<int>(sizeof(device_name)), device) != 0) {
            std::snprintf(device_name, sizeof(device_name), "CUDA Device %d", index);
        }

        YOLO26OnnxRuntimeInference::ProviderInfo info;
        info.device_id = index;
        info.name = device_name;
        devices.push_back(info);
    }

    FreeLibrary(cuda_driver);
    return devices;
}

std::string TrimLine(std::string line) {
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
    return line;
}

} // namespace

YOLO26OnnxRuntimeInference::YOLO26OnnxRuntimeInference()
    : env_(ORT_LOGGING_LEVEL_WARNING, "SmartScreenCapture-YOLO26") {
    RefreshAvailableProviders();
}

YOLO26OnnxRuntimeInference::~YOLO26OnnxRuntimeInference() {
    Cleanup();
}

bool YOLO26OnnxRuntimeInference::Initialize(const AlgorithmSettings& settings) {
    const auto* typed_settings = dynamic_cast<const Settings*>(&settings);
    if (!typed_settings) {
        last_error_ = "Invalid settings type for YOLO26OnnxRuntimeInference";
        return false;
    }

    std::string validation_error;
    if (!ValidateSettings(*typed_settings, validation_error)) {
        last_error_ = validation_error;
        return false;
    }

    Cleanup();
    settings_ = *typed_settings;
    RefreshAvailableProviders();

    if (!LoadClassNames()) {
        return false;
    }

    const bool prefer_cuda = SupportsProvider("cuda") && !available_providers_.empty();
    if (prefer_cuda && InitializeSession(true)) {
        is_initialized_ = true;
        return true;
    }

    using_cpu_fallback_ = true;
    if (!InitializeSession(false)) {
        return false;
    }

    is_initialized_ = true;
    return true;
}

IDetectionAlgorithm::DetectionResult YOLO26OnnxRuntimeInference::DetectSingle(const cv::Mat& image) {
    auto detections = DetectMultiple(image);
    if (detections.empty()) {
        return DetectionResult();
    }

    return *std::max_element(
        detections.begin(), detections.end(),
        [](const DetectionResult& lhs, const DetectionResult& rhs) {
            return lhs.confidence < rhs.confidence;
        });
}

std::vector<IDetectionAlgorithm::DetectionResult> YOLO26OnnxRuntimeInference::DetectMultiple(const cv::Mat& image) {
    if (!is_initialized_ || image.empty()) {
        return {};
    }

    std::lock_guard<std::mutex> lock(inference_mutex_);
    const auto start = std::chrono::high_resolution_clock::now();

    try {
        auto detections = RunSession(image);
        const auto end = std::chrono::high_resolution_clock::now();
        UpdatePerformanceMetrics(std::chrono::duration<double, std::milli>(end - start).count());
        return detections;
    } catch (const Ort::Exception& ex) {
        last_error_ = ex.what();
        if (!using_cpu_fallback_ && InitializeSession(false)) {
            using_cpu_fallback_ = true;
            try {
                auto detections = RunSession(image);
                const auto end = std::chrono::high_resolution_clock::now();
                UpdatePerformanceMetrics(std::chrono::duration<double, std::milli>(end - start).count());
                return detections;
            } catch (const Ort::Exception& retry_ex) {
                last_error_ = retry_ex.what();
            }
        }
    }

    return {};
}

std::vector<IDetectionAlgorithm::DetectionResult> YOLO26OnnxRuntimeInference::DetectInROI(
    const cv::Mat& image,
    const cv::Rect& roi) {
    if (!is_initialized_ || image.empty()) {
        return {};
    }

    const cv::Rect safe_roi = roi & cv::Rect(0, 0, image.cols, image.rows);
    if (safe_roi.width <= 0 || safe_roi.height <= 0) {
        return {};
    }

    auto detections = DetectMultiple(image(safe_roi));
    for (auto& detection : detections) {
        detection.bounding_box.x += safe_roi.x;
        detection.bounding_box.y += safe_roi.y;
        detection.center.x += static_cast<float>(safe_roi.x);
        detection.center.y += static_cast<float>(safe_roi.y);
    }

    return detections;
}

bool YOLO26OnnxRuntimeInference::UpdateSettings(const AlgorithmSettings& settings) {
    return Initialize(settings);
}

std::string YOLO26OnnxRuntimeInference::GetAlgorithmName() const {
    return "YOLO26 ONNX Runtime Inference";
}

std::string YOLO26OnnxRuntimeInference::GetVersion() const {
    return Constants::Version::VERSION_STRING;
}

std::vector<std::string> YOLO26OnnxRuntimeInference::GetSupportedClasses() const {
    return class_names_;
}

std::string YOLO26OnnxRuntimeInference::GetPerformanceStats() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    std::ostringstream oss;
    oss << "Provider: " << active_provider_name_
        << ", Inferences: " << total_inferences_.load()
        << ", Avg: " << average_inference_time_ms_ << " ms";
    return oss.str();
}

void YOLO26OnnxRuntimeInference::Reset() {
    total_inferences_ = 0;
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    total_inference_time_ms_ = 0.0;
    average_inference_time_ms_ = 0.0;
}

void YOLO26OnnxRuntimeInference::Cleanup() {
    session_.reset();
    binding_names_ = {};
    is_initialized_ = false;
    using_cpu_fallback_ = false;
    active_provider_name_ = "disabled";
    Reset();
}

std::vector<YOLO26OnnxRuntimeInference::ProviderInfo> YOLO26OnnxRuntimeInference::GetAvailableProviders() const {
    return available_providers_;
}

std::string YOLO26OnnxRuntimeInference::GetActiveProviderName() const {
    return active_provider_name_;
}

bool YOLO26OnnxRuntimeInference::IsUsingCpuFallback() const {
    return using_cpu_fallback_;
}

bool YOLO26OnnxRuntimeInference::IsInitialized() const {
    return is_initialized_;
}

int YOLO26OnnxRuntimeInference::GetSelectedGpuId() const {
    return settings_.selected_gpu_id;
}

std::string YOLO26OnnxRuntimeInference::GetLastError() const {
    return last_error_;
}

bool YOLO26OnnxRuntimeInference::InitializeSession(bool prefer_cuda) {
    try {
        Ort::SessionOptions options;
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
        options.SetIntraOpNumThreads(1);
        options.SetInterOpNumThreads(1);

        if (prefer_cuda) {
            Ort::ThrowOnError(
                OrtSessionOptionsAppendExecutionProvider_CUDA(options, settings_.selected_gpu_id));
        }

        session_ = std::make_unique<Ort::Session>(
            env_,
            std::filesystem::path(settings_.onnx_model_path).wstring().c_str(),
            options);

        binding_names_ = {};
        Ort::AllocatorWithDefaultOptions allocator;
        const size_t input_count = session_->GetInputCount();
        const size_t output_count = session_->GetOutputCount();
        if (input_count != 1 || output_count != 1) {
            last_error_ = "YOLO26 detect models must expose one input and one output";
            session_.reset();
            return false;
        }

        auto input_name = session_->GetInputNameAllocated(0, allocator);
        auto output_name = session_->GetOutputNameAllocated(0, allocator);
        binding_names_.input_storage.push_back(input_name.get());
        binding_names_.output_storage.push_back(output_name.get());
        binding_names_.input_names.push_back(binding_names_.input_storage.front().c_str());
        binding_names_.output_names.push_back(binding_names_.output_storage.front().c_str());

        active_provider_name_ = prefer_cuda ? "CUDA" : "CPU";
        return true;
    } catch (const Ort::Exception& ex) {
        last_error_ = ex.what();
        session_.reset();
        binding_names_ = {};
        return false;
    }
}

bool YOLO26OnnxRuntimeInference::ValidateSettings(const Settings& settings, std::string& error) const {
    if (settings.onnx_model_path.empty()) {
        error = "YOLO26 ONNX model path is empty";
        return false;
    }
    if (settings.class_names_path.empty()) {
        error = "YOLO26 class names path is empty";
        return false;
    }
    if (settings.input_width <= 0 || settings.input_height <= 0) {
        error = "YOLO26 input size must be positive";
        return false;
    }
    if (settings.max_detections <= 0) {
        error = "YOLO26 max detections must be positive";
        return false;
    }
    if (settings.selected_gpu_id < 0) {
        error = "YOLO26 selected GPU id must be non-negative";
        return false;
    }
    if (settings.confidence_threshold < 0.0 || settings.confidence_threshold > 1.0) {
        error = "YOLO26 confidence threshold must be between 0 and 1";
        return false;
    }
    if (!std::filesystem::exists(settings.onnx_model_path)) {
        error = "YOLO26 ONNX model not found: " + settings.onnx_model_path;
        return false;
    }
    if (!std::filesystem::exists(settings.class_names_path)) {
        error = "YOLO26 class names file not found: " + settings.class_names_path;
        return false;
    }
    return true;
}

bool YOLO26OnnxRuntimeInference::LoadClassNames() {
    std::ifstream file(settings_.class_names_path);
    if (!file.is_open()) {
        last_error_ = "Unable to open class names file: " + settings_.class_names_path;
        return false;
    }

    class_names_.clear();
    std::string line;
    while (std::getline(file, line)) {
        line = TrimLine(line);
        if (!line.empty()) {
            class_names_.push_back(line);
        }
    }

    if (class_names_.empty()) {
        last_error_ = "Class names file is empty: " + settings_.class_names_path;
        return false;
    }

    return true;
}

bool YOLO26OnnxRuntimeInference::RefreshAvailableProviders() {
    available_providers_ = DiscoverCudaDevices();
    return !available_providers_.empty();
}

bool YOLO26OnnxRuntimeInference::SupportsProvider(const std::string& provider_name) const {
    return std::find(
               settings_.execution_providers.begin(),
               settings_.execution_providers.end(),
               provider_name) != settings_.execution_providers.end();
}

std::vector<float> YOLO26OnnxRuntimeInference::PreprocessImage(
    const cv::Mat& image,
    LetterboxMetadata& metadata) const {
    metadata.original_width = image.cols;
    metadata.original_height = image.rows;

    const float scale = std::min(
        static_cast<float>(settings_.input_width) / static_cast<float>(image.cols),
        static_cast<float>(settings_.input_height) / static_cast<float>(image.rows));

    const int resized_width = std::max(1, static_cast<int>(std::round(image.cols * scale)));
    const int resized_height = std::max(1, static_cast<int>(std::round(image.rows * scale)));
    metadata.scale = scale;
    metadata.pad_x = static_cast<float>(settings_.input_width - resized_width) * 0.5f;
    metadata.pad_y = static_cast<float>(settings_.input_height - resized_height) * 0.5f;

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(resized_width, resized_height));

    cv::Mat letterboxed(settings_.input_height, settings_.input_width, CV_8UC3, cv::Scalar(114, 114, 114));
    const int offset_x = static_cast<int>(std::round(metadata.pad_x));
    const int offset_y = static_cast<int>(std::round(metadata.pad_y));
    resized.copyTo(letterboxed(cv::Rect(offset_x, offset_y, resized.cols, resized.rows)));

    cv::Mat rgb_image;
    cv::cvtColor(letterboxed, rgb_image, cv::COLOR_BGR2RGB);
    rgb_image.convertTo(rgb_image, CV_32F, 1.0 / 255.0);

    std::vector<float> input_tensor(static_cast<size_t>(3 * settings_.input_width * settings_.input_height));
    std::vector<cv::Mat> channels;
    channels.reserve(3);
    for (int channel = 0; channel < 3; ++channel) {
        channels.emplace_back(
            settings_.input_height,
            settings_.input_width,
            CV_32F,
            input_tensor.data() + static_cast<size_t>(channel * settings_.input_width * settings_.input_height));
    }
    cv::split(rgb_image, channels);

    return input_tensor;
}

std::vector<IDetectionAlgorithm::DetectionResult> YOLO26OnnxRuntimeInference::RunSession(const cv::Mat& image) {
    LetterboxMetadata metadata;
    auto input_values = PreprocessImage(image, metadata);

    const std::array<int64_t, 4> input_shape = {
        1, 3, static_cast<int64_t>(settings_.input_height), static_cast<int64_t>(settings_.input_width)};
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto input_tensor = Ort::Value::CreateTensor<float>(
        memory_info,
        input_values.data(),
        input_values.size(),
        input_shape.data(),
        input_shape.size());

    auto output_tensors = session_->Run(
        Ort::RunOptions{nullptr},
        binding_names_.input_names.data(),
        &input_tensor,
        1,
        binding_names_.output_names.data(),
        binding_names_.output_names.size());

    if (output_tensors.size() != 1 || !output_tensors[0].IsTensor()) {
        throw Ort::Exception("YOLO26 detect models must return one tensor output", ORT_RUNTIME_EXCEPTION);
    }

    return ParseOutputTensor(output_tensors[0], metadata);
}

std::vector<IDetectionAlgorithm::DetectionResult> YOLO26OnnxRuntimeInference::ParseOutputTensor(
    Ort::Value& output_tensor,
    const LetterboxMetadata& metadata) const {
    auto tensor_info = output_tensor.GetTensorTypeAndShapeInfo();
    const auto shape = tensor_info.GetShape();
    if (shape.size() < 2) {
        throw Ort::Exception("Unsupported YOLO26 output rank", ORT_RUNTIME_EXCEPTION);
    }
    if (tensor_info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
        throw Ort::Exception("Only float YOLO26 outputs are supported", ORT_RUNTIME_EXCEPTION);
    }

    size_t row_count = 0;
    size_t row_width = 0;
    if (shape.size() == 3) {
        row_count = static_cast<size_t>(shape[1]);
        row_width = static_cast<size_t>(shape[2]);
    } else if (shape.size() == 2) {
        row_count = static_cast<size_t>(shape[0]);
        row_width = static_cast<size_t>(shape[1]);
    } else {
        throw Ort::Exception("Unsupported YOLO26 detect output shape", ORT_RUNTIME_EXCEPTION);
    }

    if (row_width < 6) {
        throw Ort::Exception("YOLO26 detect output must expose at least 6 columns", ORT_RUNTIME_EXCEPTION);
    }

    const float* output = output_tensor.GetTensorData<float>();
    std::vector<DetectionResult> detections;
    detections.reserve(row_count);

    for (size_t row = 0; row < row_count; ++row) {
        DetectionResult detection = MakeDetectionResult(output + row * row_width, row_width, metadata);
        if (detection.confidence >= settings_.confidence_threshold && detection.bounding_box.area() > 0) {
            detections.push_back(detection);
        }
    }

    std::sort(
        detections.begin(),
        detections.end(),
        [](const DetectionResult& lhs, const DetectionResult& rhs) {
            return lhs.confidence > rhs.confidence;
        });
    if (detections.size() > static_cast<size_t>(settings_.max_detections)) {
        detections.resize(static_cast<size_t>(settings_.max_detections));
    }

    return detections;
}

IDetectionAlgorithm::DetectionResult YOLO26OnnxRuntimeInference::MakeDetectionResult(
    const float* row,
    size_t row_width,
    const LetterboxMetadata& metadata) const {
    DetectionResult result;
    float x1 = row[0];
    float y1 = row[1];
    float x2 = row[2];
    float y2 = row[3];

    if (x2 <= x1 || y2 <= y1) {
        const float center_x = row[0];
        const float center_y = row[1];
        const float width = row[2];
        const float height = row[3];
        x1 = center_x - width * 0.5f;
        y1 = center_y - height * 0.5f;
        x2 = center_x + width * 0.5f;
        y2 = center_y + height * 0.5f;
    }

    x1 = (x1 - metadata.pad_x) / metadata.scale;
    y1 = (y1 - metadata.pad_y) / metadata.scale;
    x2 = (x2 - metadata.pad_x) / metadata.scale;
    y2 = (y2 - metadata.pad_y) / metadata.scale;

    x1 = std::clamp(x1, 0.0f, static_cast<float>(metadata.original_width - 1));
    y1 = std::clamp(y1, 0.0f, static_cast<float>(metadata.original_height - 1));
    x2 = std::clamp(x2, 0.0f, static_cast<float>(metadata.original_width - 1));
    y2 = std::clamp(y2, 0.0f, static_cast<float>(metadata.original_height - 1));

    result.confidence = row[4];
    result.class_id = static_cast<int>(std::round(row[5]));
    result.bounding_box = cv::Rect(
        static_cast<int>(std::round(x1)),
        static_cast<int>(std::round(y1)),
        std::max(0, static_cast<int>(std::round(x2 - x1))),
        std::max(0, static_cast<int>(std::round(y2 - y1))));
    result.label = (result.class_id >= 0 && result.class_id < static_cast<int>(class_names_.size()))
        ? class_names_[result.class_id]
        : "class_" + std::to_string(result.class_id);
    result.center = cv::Point2f(
        result.bounding_box.x + result.bounding_box.width / 2.0f,
        result.bounding_box.y + result.bounding_box.height / 2.0f);

    if (row_width > 6 && !std::isfinite(row[6])) {
        result.bounding_box = cv::Rect();
    }

    return result;
}

void YOLO26OnnxRuntimeInference::UpdatePerformanceMetrics(double inference_time_ms) const {
    const auto new_count = total_inferences_.fetch_add(1) + 1;
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    total_inference_time_ms_ += inference_time_ms;
    average_inference_time_ms_ = total_inference_time_ms_ / static_cast<double>(new_count);
}
