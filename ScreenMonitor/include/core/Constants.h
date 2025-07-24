#pragma once

#include <cstdint>
#include <string>

/**
 * @brief 시스템 전체에서 사용되는 상수들을 정의하는 네임스페이스
 */
namespace Constants {

    // ==============================================
    // 성능 관련 상수
    // ==============================================
    namespace Performance {
        constexpr float DEFAULT_TARGET_FPS = 60.0f;
        constexpr float MIN_TARGET_FPS = 1.0f;
        constexpr float MAX_TARGET_FPS = 240.0f;
        
        constexpr double FRAME_TIME_SMOOTHING_FACTOR = 0.1;
        constexpr size_t FPS_HISTORY_SIZE = 120;
        constexpr size_t PERFORMANCE_UPDATE_INTERVAL_MS = 100;
        
        constexpr double FIXED_TIME_STEP = 1.0 / 60.0;  // 60 FPS 기준
        constexpr int MAX_FRAME_SKIP = 5;
        
        // 메모리 관련
        constexpr size_t MAX_MEMORY_USAGE_MB = 1024;    // 1GB 제한
        constexpr size_t MEMORY_WARNING_THRESHOLD_MB = 512;  // 512MB 경고
        constexpr size_t FRAME_BUFFER_SIZE = 3;         // 프레임 버퍼 크기
    }

    // ==============================================
    // 화면 캡처 관련 상수
    // ==============================================
    namespace Capture {
        constexpr int DEFAULT_MONITOR_INDEX = 0;
        constexpr int MAX_MONITORS = 16;
        
        // 캡처 영역 제한
        constexpr int MIN_CAPTURE_WIDTH = 64;
        constexpr int MIN_CAPTURE_HEIGHT = 64;
        constexpr int MAX_CAPTURE_WIDTH = 7680;   // 8K width
        constexpr int MAX_CAPTURE_HEIGHT = 4320;  // 8K height
        
        // 캡처 품질 설정
        constexpr bool DEFAULT_ENABLE_CURSOR = false;
        constexpr bool DEFAULT_ENABLE_BORDER = false;
        constexpr int CAPTURE_RETRY_COUNT = 3;
        constexpr int CAPTURE_TIMEOUT_MS = 5000;
    }

    // ==============================================
    // 컴퓨터 비전 관련 상수
    // ==============================================
    namespace Vision {
        // HSV 색상 감지
        namespace HSV {
            constexpr int DEFAULT_LOWER_H = 140;
            constexpr int DEFAULT_LOWER_S = 120;
            constexpr int DEFAULT_LOWER_V = 180;
            constexpr int DEFAULT_UPPER_H = 160;
            constexpr int DEFAULT_UPPER_S = 200;
            constexpr int DEFAULT_UPPER_V = 255;
            
            constexpr int MIN_AREA = 100;
            constexpr int MAX_AREA = 50000;
            constexpr int DEFAULT_MORPHOLOGY_KERNEL_SIZE = 5;
            constexpr int DEFAULT_GAUSSIAN_KERNEL_SIZE = 5;
            constexpr double DEFAULT_GAUSSIAN_SIGMA = 1.0;
            constexpr double DEFAULT_CONFIDENCE_THRESHOLD = 0.7;
        }
        
        // YOLO 객체 감지
        namespace YOLO {
            constexpr float DEFAULT_CONFIDENCE_THRESHOLD = 0.25f;
            constexpr float DEFAULT_NMS_THRESHOLD = 0.45f;
            constexpr int DEFAULT_INPUT_WIDTH = 640;
            constexpr int DEFAULT_INPUT_HEIGHT = 640;
            constexpr int MAX_DETECTIONS = 100;
            
            // 모델 파일 경로
            const std::string DEFAULT_MODEL_PATH = "models/yolo11n.onnx";
            const std::string DEFAULT_CLASSES_PATH = "models/coco_classes.txt";
            const std::string DEFAULT_CONFIG_PATH = "models/yolo.cfg";
        }
        
        // 일반 감지 설정
        constexpr double MIN_CONFIDENCE = 0.0;
        constexpr double MAX_CONFIDENCE = 1.0;
        constexpr int MAX_DETECTION_RESULTS = 1000;
    }

    // ==============================================
    // GUI 관련 상수
    // ==============================================
    namespace GUI {
        constexpr int DEFAULT_WINDOW_WIDTH = 1280;
        constexpr int DEFAULT_WINDOW_HEIGHT = 720;
        constexpr int MIN_WINDOW_WIDTH = 800;
        constexpr int MIN_WINDOW_HEIGHT = 600;
        
        const std::string WINDOW_TITLE = "Professional Screen Capture & Computer Vision System";
        const std::string VERSION_STRING = "v1.0.0";
        
        // ImGui 설정
        constexpr float PANEL_ROUNDING = 5.0f;
        constexpr float BUTTON_ROUNDING = 3.0f;
        constexpr float FRAME_ROUNDING = 2.0f;
        constexpr float SCROLLBAR_ROUNDING = 9.0f;
        
        // 색상 (RGBA)
        constexpr float ACCENT_COLOR[4] = {0.26f, 0.59f, 0.98f, 1.00f};     // Blue
        constexpr float SUCCESS_COLOR[4] = {0.20f, 0.78f, 0.32f, 1.00f};    // Green
        constexpr float WARNING_COLOR[4] = {1.00f, 0.76f, 0.03f, 1.00f};    // Amber
        constexpr float ERROR_COLOR[4] = {0.96f, 0.26f, 0.21f, 1.00f};      // Red
    }

    // ==============================================
    // 파일 시스템 관련 상수
    // ==============================================
    namespace FileSystem {
        const std::string CONFIG_DIRECTORY = "config";
        const std::string MODELS_DIRECTORY = "models";
        const std::string LOGS_DIRECTORY = "logs";
        const std::string SCREENSHOTS_DIRECTORY = "screenshots";
        
        const std::string CONFIG_FILE = "config.json";
        const std::string SCHEMA_FILE = "config_schema.json";
        const std::string LOG_FILE = "application.log";
        
        // 파일 확장자
        const std::string JSON_EXTENSION = ".json";
        const std::string IMAGE_EXTENSION = ".png";
        const std::string MODEL_EXTENSION = ".onnx";
        const std::string CONFIG_EXTENSION = ".cfg";
        const std::string WEIGHTS_EXTENSION = ".weights";
        const std::string CLASSES_EXTENSION = ".txt";
        
        constexpr size_t MAX_LOG_FILE_SIZE_MB = 10;
        constexpr int MAX_LOG_FILES = 5;
    }

    // ==============================================
    // 네트워킹 관련 상수
    // ==============================================
    namespace Network {
        constexpr int DEFAULT_TIMEOUT_MS = 5000;
        constexpr int MAX_RETRY_COUNT = 3;
        constexpr int CONNECTION_POOL_SIZE = 10;
        
        // HTTP 상태 코드
        constexpr int HTTP_OK = 200;
        constexpr int HTTP_NOT_FOUND = 404;
        constexpr int HTTP_INTERNAL_ERROR = 500;
    }

    // ==============================================
    // 오류 코드 및 메시지
    // ==============================================
    namespace Errors {
        // 일반 오류 코드
        constexpr int SUCCESS = 0;
        constexpr int GENERIC_ERROR = -1;
        constexpr int INITIALIZATION_FAILED = -2;
        constexpr int INVALID_PARAMETER = -3;
        constexpr int OUT_OF_MEMORY = -4;
        constexpr int FILE_NOT_FOUND = -5;
        constexpr int PERMISSION_DENIED = -6;
        constexpr int TIMEOUT = -7;
        
        // 캡처 관련 오류
        constexpr int CAPTURE_DEVICE_NOT_FOUND = -100;
        constexpr int CAPTURE_INITIALIZATION_FAILED = -101;
        constexpr int CAPTURE_START_FAILED = -102;
        constexpr int FRAME_CAPTURE_FAILED = -103;
        
        // 감지 관련 오류
        constexpr int DETECTION_MODEL_LOAD_FAILED = -200;
        constexpr int DETECTION_INITIALIZATION_FAILED = -201;
        constexpr int DETECTION_PROCESSING_FAILED = -202;
        
        // GUI 관련 오류
        constexpr int GUI_INITIALIZATION_FAILED = -300;
        constexpr int OPENGL_ERROR = -301;
        constexpr int IMGUI_ERROR = -302;
        
        // 에러 메시지
        const std::string INITIALIZATION_ERROR_MSG = "Failed to initialize component";
        const std::string INVALID_PARAMETER_MSG = "Invalid parameter provided";
        const std::string OUT_OF_MEMORY_MSG = "Insufficient memory available";
        const std::string FILE_NOT_FOUND_MSG = "Required file not found";
        const std::string CAPTURE_ERROR_MSG = "Screen capture failed";
        const std::string DETECTION_ERROR_MSG = "Object detection failed";
        const std::string GUI_ERROR_MSG = "GUI operation failed";
    }

    // ==============================================
    // 버전 정보
    // ==============================================
    namespace Version {
        constexpr int MAJOR = 1;
        constexpr int MINOR = 0;
        constexpr int PATCH = 0;
        const std::string VERSION_STRING = "1.0.0";
        const std::string BUILD_DATE = __DATE__;
        const std::string BUILD_TIME = __TIME__;
        
        // 컴포넌트 버전
        const std::string OPENCV_MIN_VERSION = "4.5.0";
        const std::string IMGUI_MIN_VERSION = "1.80.0";
        const std::string GLFW_MIN_VERSION = "3.3.0";
    }

    // ==============================================
    // 디버그 및 로깅
    // ==============================================
    namespace Debug {
        constexpr bool ENABLE_VERBOSE_LOGGING = false;
        constexpr bool ENABLE_PERFORMANCE_LOGGING = true;
        constexpr bool ENABLE_DEBUG_VISUALIZATION = false;
        constexpr bool ENABLE_MEMORY_TRACKING = true;
        
        constexpr int LOG_BUFFER_SIZE = 1024;
        constexpr int MAX_DEBUG_MESSAGES = 1000;
    }

} // namespace Constants