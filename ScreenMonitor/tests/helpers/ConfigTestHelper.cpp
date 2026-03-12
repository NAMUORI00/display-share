#include "ConfigTestHelper.h"
#include <fstream>
#include <random>
#include <chrono>
#include <iostream>

std::vector<std::string> ConfigTestHelper::temp_files_;

json ConfigTestHelper::createDefaultTestConfig() {
    return json{
        {"production_system", {
            {"name", "Professional Screen Capture & Computer Vision System"},
            {"version", "1.0.0"},
            {"purpose", "High-performance real-time screen capture and computer vision processing"},
            {"mode", "production_mode"}
        }},
        {"vision_algorithms", {
            {"selected_algorithm", "yolo26"},
            {"hsv_tracking", {
                {"enabled", true},
                {"lower_bound", json::array({140, 120, 180})},
                {"upper_bound", json::array({160, 200, 255})},
                {"morphology_kernel_size", 3},
                {"min_contour_area", 100}
            }},
            {"yolo26_detection", {
                {"enabled", true},
                {"onnx_model_path", "models/yolo26n.onnx"},
                {"class_names_path", "models/coco_classes.txt"},
                {"confidence_threshold", 0.25},
                {"max_detections", 100},
                {"input_size", json::array({640, 640})},
                {"execution_providers", json::array({"cuda", "cpu"})},
                {"selected_gpu_id", 0}
            }}
        }},
        {"analytics", {
            {"enabled", true},
            {"max_history_size", 1000},
            {"fps_calculation_window_sec", 5.0}
        }},
        {"gui", {
            {"show_performance_metrics", true},
            {"ui_scale", 1.0},
            {"show_metrics_overlay", true}
        }},
        {"performance", {
            {"target_fps", 30},
            {"enable_multithreading", true},
            {"max_processing_threads", 4},
            {"frame_buffer_size", 5},
            {"enable_gpu_acceleration", true}
        }}
    };
}

json ConfigTestHelper::createMinimalValidConfig() {
    return json{
        {"educational_framework", {
            {"name", "Test Framework"},
            {"version", "1.0.0"},
            {"purpose", "Testing"},
            {"mode", "educational_only"}
        }},
        {"vision_algorithms", {
            {"selected_algorithm", "yolo26"},
            {"hsv_tracking", {
                {"enabled", true},
                {"lower_bound", json::array({0, 0, 0})},
                {"upper_bound", json::array({179, 255, 255})}
            }},
            {"yolo26_detection", {
                {"enabled", true},
                {"onnx_model_path", "models/yolo26n.onnx"},
                {"class_names_path", "models/coco_classes.txt"},
                {"confidence_threshold", 0.25},
                {"max_detections", 10},
                {"input_size", json::array({640, 640})},
                {"execution_providers", json::array({"cuda", "cpu"})},
                {"selected_gpu_id", 0}
            }}
        }},
        {"analytics", {
            {"enabled", true},
            {"max_history_size", 100},
            {"fps_calculation_window_sec", 1.0}
        }},
        {"gui", {
            {"show_performance_metrics", true},
            {"ui_scale", 1.0}
        }},
        {"performance", {
            {"target_fps", 30},
            {"enable_multithreading", true},
            {"max_processing_threads", 1}
        }}
    };
}

json ConfigTestHelper::createInvalidConfig() {
    return json{
        {"educational_framework", {
            {"mode", "invalid_mode"}  // 잘못된 모드
        }},
        {"vision_algorithms", {
            {"selected_algorithm", "invalid_algorithm"},  // 잘못된 알고리즘
            {"hsv_tracking", {
                {"lower_bound", json::array({-10, 300, 400})}  // 범위 초과
            }},
            {"yolo26_detection", {
                {"enabled", true},
                {"onnx_model_path", ""},
                {"class_names_path", ""},
                {"confidence_threshold", 1.5},
                {"max_detections", 0},
                {"input_size", json::array({0, 0})},
                {"execution_providers", json::array()},
                {"selected_gpu_id", -1}
            }}
        }}
    };
}

json ConfigTestHelper::createHSVTestConfig(int h_min, int h_max, int s_min, int s_max, int v_min, int v_max) {
    json config = createDefaultTestConfig();
    config["vision_algorithms"]["hsv_tracking"]["lower_bound"] = json::array({h_min, s_min, v_min});
    config["vision_algorithms"]["hsv_tracking"]["upper_bound"] = json::array({h_max, s_max, v_max});
    return config;
}

json ConfigTestHelper::createYOLOTestConfig(const std::string& model_path, 
                                           const std::string& config_path, 
                                           double confidence) {
    json config = createDefaultTestConfig();
    config["vision_algorithms"]["yolo26_detection"]["enabled"] = true;
    config["vision_algorithms"]["yolo26_detection"]["onnx_model_path"] = model_path;
    config["vision_algorithms"]["yolo26_detection"]["class_names_path"] = config_path;
    config["vision_algorithms"]["yolo26_detection"]["confidence_threshold"] = confidence;
    return config;
}

json ConfigTestHelper::createPerformanceTestConfig(int target_fps, bool multithreading, int max_threads) {
    json config = createDefaultTestConfig();
    config["performance"]["target_fps"] = target_fps;
    config["performance"]["enable_multithreading"] = multithreading;
    config["performance"]["max_processing_threads"] = max_threads;
    return config;
}

std::string ConfigTestHelper::createTempConfigFile(const json& config) {
    std::string filename = generateTempFileName();
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create temp file: " + filename);
    }
    
    file << config.dump(4);
    file.close();
    
    temp_files_.push_back(filename);
    return filename;
}

std::string ConfigTestHelper::createEmptyConfigFile() {
    std::string filename = generateTempFileName();
    
    std::ofstream file(filename);
    file.close();
    
    temp_files_.push_back(filename);
    return filename;
}

std::string ConfigTestHelper::createInvalidJSONFile() {
    std::string filename = generateTempFileName();
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create temp file: " + filename);
    }
    
    file << "{ invalid json content without closing brace";
    file.close();
    
    temp_files_.push_back(filename);
    return filename;
}

std::string ConfigTestHelper::createInaccessibleFilePath() {
    // 존재하지 않는 디렉토리의 파일 경로
    return "/nonexistent/directory/config.json";
}

json ConfigTestHelper::createConfigWithMissingSection(const std::string& missing_section) {
    json config = createDefaultTestConfig();
    
    if (config.contains(missing_section)) {
        config.erase(missing_section);
    }
    
    return config;
}

json ConfigTestHelper::createConfigWithWrongTypes() {
    return json{
        {"educational_framework", {
            {"name", 123},  // 문자열이어야 하는데 숫자
            {"mode", "educational_only"}
        }},
        {"vision_algorithms", {
            {"selected_algorithm", "hsv"},
            {"hsv_tracking", {
                {"enabled", "true"},  // 불린이어야 하는데 문자열
                {"lower_bound", "not_an_array"}  // 배열이어야 하는데 문자열
            }}
        }},
        {"performance", {
            {"target_fps", "thirty"}  // 숫자여야 하는데 문자열
        }}
    };
}

json ConfigTestHelper::createConfigWithOutOfRangeValues() {
    return json{
        {"educational_framework", {
            {"name", "Test Framework"},
            {"version", "1.0.0"},
            {"purpose", "Testing"},
            {"mode", "educational_only"}
        }},
        {"vision_algorithms", {
            {"selected_algorithm", "hsv"},
            {"hsv_tracking", {
                {"enabled", true},
                {"lower_bound", json::array({-50, 300, 400})},  // H: -50 (범위: 0-179), S,V: 300,400 (범위: 0-255)
                {"upper_bound", json::array({200, 500, 600})}   // 모든 값이 범위 초과
            }},
            {"yolo26_detection", {
                {"enabled", true},
                {"onnx_model_path", ""},
                {"class_names_path", ""},
                {"confidence_threshold", 5.0},
                {"max_detections", -1},
                {"input_size", json::array({-1, -1})},
                {"execution_providers", json::array()},
                {"selected_gpu_id", -3}
            }}
        }},
        {"performance", {
            {"target_fps", -10},  // 음수 FPS
            {"max_processing_threads", 0}  // 0개 스레드
        }}
    };
}

json ConfigTestHelper::createDefaultSchema() {
    return json{
        {"type", "object"},
        {"required", json::array({"production_system", "vision_algorithms", "analytics", "gui", "performance"})},
        {"properties", {
            {"production_system", {
                {"type", "object"},
                {"required", json::array({"name", "version", "purpose", "mode"})},
                {"properties", {
                    {"name", {"type", "string"}},
                    {"version", {"type", "string"}},
                    {"purpose", {"type", "string"}},
                    {"mode", {"type", "string", "enum", json::array({"production_mode"})}}
                }}
            }}
        }}
    };
}

bool ConfigTestHelper::compareConfigs(const json& config1, const json& config2, 
                                     const std::vector<std::string>& ignore_keys) {
    json c1 = config1;
    json c2 = config2;
    
    // 무시할 키들 제거
    for (const auto& key : ignore_keys) {
        if (c1.contains(key)) c1.erase(key);
        if (c2.contains(key)) c2.erase(key);
    }
    
    return c1 == c2;
}

void ConfigTestHelper::cleanupTempFile(const std::string& file_path) {
    try {
        std::filesystem::remove(file_path);
        auto it = std::find(temp_files_.begin(), temp_files_.end(), file_path);
        if (it != temp_files_.end()) {
            temp_files_.erase(it);
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to cleanup temp file " << file_path << ": " << e.what() << std::endl;
    }
}

void ConfigTestHelper::cleanupAllTempFiles() {
    for (const auto& file : temp_files_) {
        try {
            std::filesystem::remove(file);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to cleanup temp file " << file << ": " << e.what() << std::endl;
        }
    }
    temp_files_.clear();
}

std::string ConfigTestHelper::generateTempFileName() {
    auto now = std::chrono::high_resolution_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    return "test_config_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen)) + ".json";
}
