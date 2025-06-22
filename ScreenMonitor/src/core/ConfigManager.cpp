#include "ConfigManager.h"
#include <fstream>
#include <iostream>

ConfigManager::ConfigManager() 
    : is_loaded(false) {  // Keep as false to distinguish between default and loaded state
    // Initialize with default configuration
    config_data = getDefaultConfig();
    schema = getDefaultSchema();
}

bool ConfigManager::loadSchema(const std::filesystem::path& schema_path) {
    try {
        if (!std::filesystem::exists(schema_path)) {
            schema = getDefaultSchema();
            return true;
        }
        
        std::ifstream file(schema_path);
        if (!file.is_open()) {
            throw ConfigException("Cannot open schema file: " + schema_path.string());
        }
        
        file >> schema;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading schema: " << e.what() << std::endl;
        schema = getDefaultSchema();
        return false;
    }
}

json ConfigManager::getDefaultSchema() const {
    return json{
        {"type", "object"},
        {"required", json::array({"educational_framework", "vision_algorithms", "simulation", "analytics", "gui", "performance", "data_export", "educational_content"})},
        {"properties", {
            {"educational_framework", {
                {"type", "object"},
                {"required", json::array({"name", "version", "purpose", "mode"})},
                {"properties", {
                    {"name", {"type", "string"}},
                    {"version", {"type", "string"}},
                    {"purpose", {"type", "string"}},
                    {"mode", {"type", "string", "enum", json::array({"educational_only"})}}
                }}
            }},
            {"vision_algorithms", {
                {"type", "object"},
                {"required", json::array({"selected_algorithm", "hsv_tracking", "yolo_detection"})},
                {"properties", {
                    {"selected_algorithm", {"type", "string", "enum", json::array({"hsv", "yolo", "template_matching", "optical_flow"})}},
                    {"hsv_tracking", {
                        {"type", "object"},
                        {"properties", {
                            {"enabled", {"type", "boolean"}},
                            {"lower_bound", {
                                {"type", "array"},
                                {"items", {"type", "integer", "minimum", 0, "maximum", 255}},
                                {"minItems", 3},
                                {"maxItems", 3}
                            }},
                            {"upper_bound", {
                                {"type", "array"},
                                {"items", {"type", "integer", "minimum", 0, "maximum", 255}},
                                {"minItems", 3},
                                {"maxItems", 3}
                            }},
                            {"morphology_kernel_size", {"type", "integer", "minimum", 1, "maximum", 15}},
                            {"min_contour_area", {"type", "integer", "minimum", 1}}
                        }}
                    }},
                    {"yolo_detection", {
                        {"type", "object"},
                        {"properties", {
                            {"enabled", {"type", "boolean"}},
                            {"model_path", {"type", "string"}},
                            {"config_path", {"type", "string"}},
                            {"confidence_threshold", {"type", "number", "minimum", 0, "maximum", 1}},
                            {"nms_threshold", {"type", "number", "minimum", 0, "maximum", 1}}
                        }}
                    }}
                }}
            }},
            {"simulation", {
                {"type", "object"},
                {"required", json::array({"enabled", "reaction_time_ms", "movement_smoothness"})},
                {"properties", {
                    {"enabled", {"type", "boolean"}},
                    {"reaction_time_ms", {"type", "number", "minimum", 0}},
                    {"movement_smoothness", {"type", "number", "minimum", 0, "maximum", 1}},
                    {"enable_prediction", {"type", "boolean"}},
                    {"enable_noise_simulation", {"type", "boolean"}},
                    {"auto_tracking_demo", {"type", "boolean"}},
                    {"trigger_simulation", {"type", "boolean"}}
                }}
            }},
            {"analytics", {
                {"type", "object"},
                {"required", json::array({"enabled", "max_history_size"})},
                {"properties", {
                    {"enabled", {"type", "boolean"}},
                    {"max_history_size", {"type", "integer", "minimum", 100, "maximum", 10000}},
                    {"fps_calculation_window_sec", {"type", "number", "minimum", 1, "maximum", 60}},
                    {"enable_real_time_analysis", {"type", "boolean"}},
                    {"confidence_threshold", {"type", "number", "minimum", 0, "maximum", 1}}
                }}
            }},
            {"gui", {
                {"type", "object"},
                {"properties", {
                    {"educational_mode", {"type", "boolean"}},
                    {"show_performance_metrics", {"type", "boolean"}},
                    {"show_algorithm_comparison", {"type", "boolean"}},
                    {"ui_scale", {"type", "number", "minimum", 0.5, "maximum", 3.0}}
                }}
            }},
            {"performance", {
                {"type", "object"},
                {"properties", {
                    {"target_fps", {"type", "integer", "minimum", 1, "maximum", 240}},
                    {"enable_multithreading", {"type", "boolean"}},
                    {"max_processing_threads", {"type", "integer", "minimum", 1, "maximum", 16}},
                    {"enable_gpu_acceleration", {"type", "boolean"}}
                }}
            }},
            {"data_export", {
                {"type", "object"},
                {"properties", {
                    {"default_format", {"type", "string", "enum", json::array({"csv", "json", "txt"})}},
                    {"auto_timestamp", {"type", "boolean"}},
                    {"include_frame_data", {"type", "boolean"}},
                    {"include_intermediate_steps", {"type", "boolean"}},
                    {"compression", {"type", "boolean"}}
                }}
            }},
            {"educational_content", {
                {"type", "object"},
                {"properties", {
                    {"enable_tutorials", {"type", "boolean"}},
                    {"show_algorithm_explanations", {"type", "boolean"}},
                    {"interactive_demos", {"type", "boolean"}},
                    {"step_by_step_guides", {"type", "boolean"}},
                    {"performance_comparisons", {"type", "boolean"}},
                    {"code_examples", {"type", "boolean"}}
                }}
            }}
        }}
    };
}

json ConfigManager::getDefaultConfig() const {
    return json{
        {"educational_framework", {
            {"name", "Educational Computer Vision Framework"},
            {"version", "1.0.0"},
            {"purpose", "Learning computer vision and tracking algorithms"},
            {"mode", "educational_only"}
        }},
        {"vision_algorithms", {
            {"selected_algorithm", "hsv"},
            {"hsv_tracking", {
                {"enabled", true},
                {"lower_bound", json::array({140, 120, 180})},
                {"upper_bound", json::array({160, 200, 255})},
                {"morphology_kernel_size", 3},
                {"min_contour_area", 100}
            }},
            {"yolo_detection", {
                {"enabled", false},
                {"model_path", "models/yolo.weights"},
                {"config_path", "models/yolo.cfg"},
                {"confidence_threshold", 0.5},
                {"nms_threshold", 0.4}
            }},
            {"template_matching", {
                {"enabled", true},
                {"method", "TM_CCOEFF_NORMED"},
                {"threshold", 0.8},
                {"multi_scale", true}
            }},
            {"optical_flow", {
                {"enabled", true},
                {"max_corners", 100},
                {"quality_level", 0.01},
                {"min_distance", 10.0},
                {"block_size", 3},
                {"use_harris_detector", false}
            }},
            {"kalman_filter", {
                {"enabled", true},
                {"process_noise_cov", 0.01},
                {"measurement_noise_cov", 0.1},
                {"error_cov_post", 0.1}
            }}
        }},
        {"simulation", {
            {"enabled", true},
            {"reaction_time_ms", 150.0},
            {"movement_smoothness", 0.7},
            {"enable_prediction", true},
            {"enable_noise_simulation", false},
            {"auto_tracking_demo", false},
            {"trigger_simulation", false}
        }},
        {"analytics", {
            {"enabled", true},
            {"max_history_size", 1000},
            {"fps_calculation_window_sec", 5.0},
            {"enable_real_time_analysis", true},
            {"confidence_threshold", 0.5}
        }},
        {"gui", {
            {"educational_mode", true},
            {"show_performance_metrics", true},
            {"show_algorithm_comparison", false},
            {"ui_scale", 1.0}
        }},
        {"performance", {
            {"target_fps", 30},
            {"enable_multithreading", true},
            {"max_processing_threads", 4},
            {"enable_gpu_acceleration", false}
        }}
    };
}

bool ConfigManager::validateConfig(const json& config) const {
    // Basic validation - check if all required sections exist
    try {
        const std::vector<std::string> required_sections = {"educational_framework", "vision_algorithms", "simulation", "analytics", "gui", "performance"};
        
        for (const auto& section : required_sections) {
            if (!config.contains(section)) {
                return false;
            }
        }
        
        // Validate educational_framework section
        if (!config["educational_framework"].contains("mode")) {
            return false;
        }
        
        std::string mode = config["educational_framework"]["mode"].get<std::string>();
        if (mode != "educational_only") {
            return false;
        }
        
        // Validate vision_algorithms section
        if (!config["vision_algorithms"].contains("selected_algorithm")) {
            return false;
        }
        
        std::string selected_algo = config["vision_algorithms"]["selected_algorithm"].get<std::string>();
        std::vector<std::string> valid_algorithms = {"hsv", "yolo", "template_matching", "optical_flow"};
        if (std::find(valid_algorithms.begin(), valid_algorithms.end(), selected_algo) == valid_algorithms.end()) {
            return false;
        }
        
        // Validate HSV tracking section if present
        if (config["vision_algorithms"].contains("hsv_tracking")) {
            auto hsv_section = config["vision_algorithms"]["hsv_tracking"];
            if (hsv_section.contains("lower_bound") && hsv_section.contains("upper_bound")) {
                auto hsv_lower = hsv_section["lower_bound"];
                auto hsv_upper = hsv_section["upper_bound"];
                
                if (hsv_lower.is_array() && hsv_upper.is_array() && 
                    hsv_lower.size() == 3 && hsv_upper.size() == 3) {
                    
                    for (size_t i = 0; i < 3; ++i) {
                        if (!hsv_lower[i].is_number_integer() || !hsv_upper[i].is_number_integer()) {
                            return false;
                        }
                        int lower_val = hsv_lower[i].get<int>();
                        int upper_val = hsv_upper[i].get<int>();
                        if (lower_val < 0 || lower_val > 255 || upper_val < 0 || upper_val > 255) {
                            return false;
                        }
                    }
                }
            }
        }
        
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

json ConfigManager::mergeWithDefaults(const json& config) const {
    json result = getDefaultConfig();
    
    // Deep merge with provided config
    if (config.is_object()) {
        for (auto& [key, value] : config.items()) {
            if (result.contains(key)) {
                if (value.is_object() && result[key].is_object()) {
                    // Recursively merge objects
                    for (auto& [subkey, subvalue] : value.items()) {
                        result[key][subkey] = subvalue;
                    }
                } else {
                    result[key] = value;
                }
            } else {
                result[key] = value;
            }
        }
    }
    
    return result;
}

bool ConfigManager::loadConfig(const std::string& file_path) {
    return loadConfig(file_path, "");
}

bool ConfigManager::loadConfig(const std::string& file_path, const std::string& schema_path) {
    try {
        config_file_path = std::filesystem::path(file_path);
        
        // Validate the parent directory path is writable/accessible
        auto parent_dir = config_file_path.parent_path();
        if (!parent_dir.empty() && !std::filesystem::exists(parent_dir)) {
            // Try to create parent directory, but fail if it's invalid (like /invalid/path)
            std::error_code ec;
            std::filesystem::create_directories(parent_dir, ec);
            if (ec) {
                is_loaded = false;
                return false;
            }
        }
        
        // Load schema if provided
        if (!schema_path.empty()) {
            loadSchema(std::filesystem::path(schema_path));
        }
        
        // Check if config file exists
        if (!std::filesystem::exists(config_file_path)) {
            // Create default config file
            if (!createDefaultConfig(file_path)) {
                is_loaded = false;
                return false;
            }
        }
        
        // Load configuration
        std::ifstream file(config_file_path);
        if (!file.is_open()) {
            throw ConfigException("Cannot open config file: " + file_path);
        }
        
        json loaded_config;
        file >> loaded_config;
        
        // Merge with defaults to ensure all required fields exist
        config_data = mergeWithDefaults(loaded_config);
        
        // Validate configuration
        if (!validateConfig(config_data)) {
            std::cerr << "Warning: Configuration validation failed, using defaults" << std::endl;
            config_data = getDefaultConfig();
        }
        
        is_loaded = true;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        config_data = getDefaultConfig();
        is_loaded = false;
        return false;
    }
}

bool ConfigManager::saveConfig(const std::string& file_path) {
    try {
        std::filesystem::path save_path = file_path.empty() ? config_file_path : std::filesystem::path(file_path);
        
        if (save_path.empty()) {
            throw ConfigException("No file path specified for saving");
        }
        
        // Create directory if it doesn't exist
        std::filesystem::create_directories(save_path.parent_path());
        
        // Save configuration with pretty formatting
        std::ofstream file(save_path);
        if (!file.is_open()) {
            throw ConfigException("Cannot open file for writing: " + save_path.string());
        }
        
        file << config_data.dump(4) << std::endl;
        
        if (!file_path.empty()) {
            config_file_path = save_path;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving config: " << e.what() << std::endl;
        return false;
    }
}

json ConfigManager::getSection(Section section) const {
    if (!is_loaded) {
        return json{};
    }
    
    std::string section_name = sectionToString(section);
    if (config_data.contains(section_name)) {
        return config_data[section_name];
    }
    
    return json{};
}

bool ConfigManager::setSection(Section section, const json& data) {
    if (!is_loaded) {
        return false;
    }
    
    try {
        std::string section_name = sectionToString(section);
        config_data[section_name] = data;
        
        // Validate after setting
        return validateConfig(config_data);
        
    } catch (const std::exception&) {
        return false;
    }
}

bool ConfigManager::validateSchema() const {
    return validateConfig(config_data);
}

void ConfigManager::resetToDefaults() {
    config_data = getDefaultConfig();
    is_loaded = true;
}

bool ConfigManager::createDefaultConfig(const std::string& file_path) {
    try {
        std::filesystem::path path(file_path);
        std::filesystem::create_directories(path.parent_path());
        
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        
        json default_config = getDefaultConfig();
        file << default_config.dump(4) << std::endl;
        
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::string ConfigManager::sectionToString(Section section) {
    switch (section) {
        case Section::EDUCATIONAL_FRAMEWORK: return "educational_framework";
        case Section::VISION_ALGORITHMS: return "vision_algorithms";
        case Section::SIMULATION: return "simulation";
        case Section::ANALYTICS: return "analytics";
        case Section::GUI: return "gui";
        case Section::PERFORMANCE: return "performance";
        default: return "";
    }
}

std::optional<ConfigManager::Section> ConfigManager::stringToSection(const std::string& section_name) {
    if (section_name == "educational_framework") return Section::EDUCATIONAL_FRAMEWORK;
    if (section_name == "vision_algorithms") return Section::VISION_ALGORITHMS;
    if (section_name == "simulation") return Section::SIMULATION;
    if (section_name == "analytics") return Section::ANALYTICS;
    if (section_name == "gui") return Section::GUI;
    if (section_name == "performance") return Section::PERFORMANCE;
    return std::nullopt;
}

