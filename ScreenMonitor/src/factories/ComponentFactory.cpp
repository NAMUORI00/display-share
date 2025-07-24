#include "../../include/factories/ComponentFactory.h"
#include "../../include/capture/ScreenCaptureLiteDevice.h"
#include "../../include/detection/HSVColorDetection.h"
#include "../../include/core/Constants.h"
#include <iostream>
#include <stdexcept>

// 플랫폼별 헤더 (미래 확장용)
#ifdef _WIN32
    // Windows 전용 캡처 디바이스들
#elif __linux__
    // Linux 전용 캡처 디바이스들
#elif __APPLE__
    // macOS 전용 캡처 디바이스들
#endif

ComponentFactory& ComponentFactory::GetInstance() {
    static ComponentFactory instance;
    static bool initialized = false;
    
    if (!initialized) {
        instance.RegisterDefaultCreators();
        initialized = true;
    }
    
    return instance;
}

std::unique_ptr<ICaptureDevice> ComponentFactory::CreateCaptureDevice(CaptureDeviceType type) {
    std::string type_name = CaptureDeviceTypeToString(type);
    return CreateCaptureDevice(type_name);
}

std::unique_ptr<IDetectionAlgorithm> ComponentFactory::CreateDetectionAlgorithm(DetectionAlgorithmType type) {
    std::string type_name = DetectionAlgorithmTypeToString(type);
    return CreateDetectionAlgorithm(type_name);
}

std::unique_ptr<ICaptureDevice> ComponentFactory::CreateCaptureDevice(const std::string& type_name) {
    auto it = capture_device_creators_.find(type_name);
    if (it != capture_device_creators_.end()) {
        try {
            auto device = it->second();
            if (device) {
                std::cout << "[ComponentFactory] Created capture device: " << type_name << std::endl;
                return device;
            }
        } catch (const std::exception& e) {
            std::cerr << "[ComponentFactory] Failed to create capture device '" << type_name 
                      << "': " << e.what() << std::endl;
        }
    }
    
    std::cerr << "[ComponentFactory] Unknown capture device type: " << type_name << std::endl;
    return nullptr;
}

std::unique_ptr<IDetectionAlgorithm> ComponentFactory::CreateDetectionAlgorithm(const std::string& type_name) {
    auto it = detection_algorithm_creators_.find(type_name);
    if (it != detection_algorithm_creators_.end()) {
        try {
            auto algorithm = it->second();
            if (algorithm) {
                std::cout << "[ComponentFactory] Created detection algorithm: " << type_name << std::endl;
                return algorithm;
            }
        } catch (const std::exception& e) {
            std::cerr << "[ComponentFactory] Failed to create detection algorithm '" << type_name 
                      << "': " << e.what() << std::endl;
        }
    }
    
    std::cerr << "[ComponentFactory] Unknown detection algorithm type: " << type_name << std::endl;
    return nullptr;
}

std::vector<std::string> ComponentFactory::GetAvailableCaptureDeviceTypes() const {
    std::vector<std::string> types;
    types.reserve(capture_device_creators_.size());
    
    for (const auto& pair : capture_device_creators_) {
        types.push_back(pair.first);
    }
    
    return types;
}

std::vector<std::string> ComponentFactory::GetAvailableDetectionAlgorithmTypes() const {
    std::vector<std::string> types;
    types.reserve(detection_algorithm_creators_.size());
    
    for (const auto& pair : detection_algorithm_creators_) {
        types.push_back(pair.first);
    }
    
    return types;
}

ComponentFactory::CaptureDeviceType ComponentFactory::GetRecommendedCaptureDeviceType() const {
    // 플랫폼별 권장 캡처 디바이스 결정
#ifdef _WIN32
    // Windows: DirectX 캡처가 사용 가능하면 우선, 아니면 screen_capture_lite
    return CaptureDeviceType::SCREEN_CAPTURE_LITE;  // 현재는 screen_capture_lite만 구현됨
#elif __linux__
    // Linux: XLib 캡처 우선
    return CaptureDeviceType::SCREEN_CAPTURE_LITE;
#elif __APPLE__
    // macOS: Core Graphics 캡처 우선
    return CaptureDeviceType::SCREEN_CAPTURE_LITE;
#else
    // 기타 플랫폼: screen_capture_lite (크로스 플랫폼)
    return CaptureDeviceType::SCREEN_CAPTURE_LITE;
#endif
}

void ComponentFactory::RegisterDefaultCreators() {
    // 캡처 디바이스 생성자 등록
    capture_device_creators_["screen_capture_lite"] = []() -> std::unique_ptr<ICaptureDevice> {
        return std::make_unique<ScreenCaptureLiteDevice>();
    };
    
    capture_device_creators_["default"] = capture_device_creators_["screen_capture_lite"];
    
    // 플랫폼별 캡처 디바이스 (미래 확장용)
#ifdef _WIN32
    // Windows 전용 캡처 디바이스들을 여기에 등록
    // capture_device_creators_["directx_capture"] = []() { return std::make_unique<DirectXCaptureDevice>(); };
#endif

#ifdef __linux__
    // Linux 전용 캡처 디바이스들을 여기에 등록
    // capture_device_creators_["xlib_capture"] = []() { return std::make_unique<XLibCaptureDevice>(); };
#endif

#ifdef __APPLE__
    // macOS 전용 캡처 디바이스들을 여기에 등록
    // capture_device_creators_["core_graphics_capture"] = []() { return std::make_unique<CoreGraphicsCaptureDevice>(); };
#endif

    // 감지 알고리즘 생성자 등록
    detection_algorithm_creators_["hsv_color_detection"] = []() -> std::unique_ptr<IDetectionAlgorithm> {
        return std::make_unique<HSVColorDetection>();
    };
    
    detection_algorithm_creators_["default"] = detection_algorithm_creators_["hsv_color_detection"];
    
    // 추가 감지 알고리즘들 (미래 구현용)
    /*
    detection_algorithm_creators_["yolo_object_detection"] = []() {
        return std::make_unique<YOLOObjectDetection>();
    };
    
    detection_algorithm_creators_["template_matching"] = []() {
        return std::make_unique<TemplateMatching>();
    };
    
    detection_algorithm_creators_["feature_matching"] = []() {
        return std::make_unique<FeatureMatching>();
    };
    */
    
    std::cout << "[ComponentFactory] Registered " << capture_device_creators_.size() 
              << " capture device types and " << detection_algorithm_creators_.size() 
              << " detection algorithm types" << std::endl;
}

std::string ComponentFactory::CaptureDeviceTypeToString(CaptureDeviceType type) const {
    switch (type) {
        case CaptureDeviceType::SCREEN_CAPTURE_LITE:
            return "screen_capture_lite";
        case CaptureDeviceType::DIRECTX_CAPTURE:
            return "directx_capture";
        case CaptureDeviceType::XLIB_CAPTURE:
            return "xlib_capture";
        case CaptureDeviceType::CORE_GRAPHICS_CAPTURE:
            return "core_graphics_capture";
        default:
            return "unknown";
    }
}

std::string ComponentFactory::DetectionAlgorithmTypeToString(DetectionAlgorithmType type) const {
    switch (type) {
        case DetectionAlgorithmType::HSV_COLOR_DETECTION:
            return "hsv_color_detection";
        case DetectionAlgorithmType::YOLO_OBJECT_DETECTION:
            return "yolo_object_detection";
        case DetectionAlgorithmType::TEMPLATE_MATCHING:
            return "template_matching";
        case DetectionAlgorithmType::FEATURE_MATCHING:
            return "feature_matching";
        default:
            return "unknown";
    }
}