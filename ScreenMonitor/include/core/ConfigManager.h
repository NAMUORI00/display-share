#pragma once

#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <optional>
#include <stdexcept>

using json = nlohmann::json;

/**
 * @brief Configuration management class for handling JSON-based settings
 * 
 * This class provides a centralized way to manage application configuration
 * with Python compatibility, schema validation, and type safety.
 */
class ConfigManager {
public:
    /**
     * @brief Exception thrown when configuration operations fail
     */
    class ConfigException : public std::runtime_error {
    public:
        explicit ConfigException(const std::string& message)
            : std::runtime_error("ConfigManager: " + message) {}
    };

    /**
     * @brief Configuration sections enum for type safety
     */
    enum class Section {
        PRODUCTION_SYSTEM,
        VISION_ALGORITHMS,
        ANALYTICS,
        GUI,
        PERFORMANCE
    };

private:
    json config_data;                           ///< Current configuration data
    json schema;                               ///< Configuration schema for validation
    std::filesystem::path config_file_path;    ///< Path to current config file
    bool is_loaded;                           ///< Whether configuration is loaded
    
    /**
     * @brief Load and validate schema from file
     * @param schema_path Path to schema file
     * @return true if schema loaded successfully
     */
    bool loadSchema(const std::filesystem::path& schema_path);
    
    /**
     * @brief Get default schema if no schema file exists
     * @return Default JSON schema
     */
    json getDefaultSchema() const;
    
    /**
     * @brief Get default configuration values
     * @return Default JSON configuration
     */
    json getDefaultConfig() const;
    
    /**
     * @brief Validate configuration against schema
     * @param config Configuration to validate
     * @return true if valid
     */
    bool validateConfig(const json& config) const;
    
    /**
     * @brief Merge configuration with defaults
     * @param config Configuration to merge
     * @return Merged configuration
     */
    json mergeWithDefaults(const json& config) const;

public:
    /**
     * @brief Constructor
     */
    ConfigManager();
    
    /**
     * @brief Destructor
     */
    ~ConfigManager() = default;
    
    // Disable copy constructor and assignment
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // Enable move constructor and assignment
    ConfigManager(ConfigManager&&) = default;
    ConfigManager& operator=(ConfigManager&&) = default;
    
    /**
     * @brief Load configuration from file
     * @param file_path Path to configuration file
     * @return true if loaded successfully
     */
    bool loadConfig(const std::string& file_path);
    
    /**
     * @brief Load configuration from file with schema validation
     * @param file_path Path to configuration file
     * @param schema_path Path to schema file (optional)
     * @return true if loaded successfully
     */
    bool loadConfig(const std::string& file_path, const std::string& schema_path);
    
    /**
     * @brief Save configuration to file
     * @param file_path Path to save file (uses loaded path if empty)
     * @return true if saved successfully
     */
    bool saveConfig(const std::string& file_path = "");
    
    /**
     * @brief Get configuration value by key path
     * @tparam T Type of value to retrieve
     * @param key_path JSON pointer path (e.g., "/hsv/lower/0")
     * @param default_value Default value if key not found
     * @return Configuration value or default
     */
    template<typename T>
    T getValue(const std::string& key_path, const T& default_value) const;
    
    /**
     * @brief Set configuration value by key path
     * @tparam T Type of value to set
     * @param key_path JSON pointer path
     * @param value Value to set
     * @return true if set successfully
     */
    template<typename T>
    bool setValue(const std::string& key_path, const T& value);
    
    /**
     * @brief Get entire section of configuration
     * @param section Section to retrieve
     * @return JSON object for the section
     */
    json getSection(Section section) const;
    
    /**
     * @brief Set entire section of configuration
     * @param section Section to set
     * @param data JSON data for the section
     * @return true if set successfully
     */
    bool setSection(Section section, const json& data);
    
    /**
     * @brief Check if configuration is loaded
     * @return true if configuration is loaded
     */
    bool isLoaded() const { return is_loaded; }
    
    /**
     * @brief Get current configuration file path
     * @return File path
     */
    std::string getConfigPath() const { return config_file_path.string(); }
    
    /**
     * @brief Validate current configuration against schema
     * @return true if valid
     */
    bool validateSchema() const;
    
    /**
     * @brief Reset configuration to defaults
     */
    void resetToDefaults();
    
    /**
     * @brief Get all configuration data
     * @return Complete configuration JSON
     */
    const json& getAllConfig() const { return config_data; }
    
    /**
     * @brief Create a new configuration file with default values
     * @param file_path Path for new configuration file
     * @return true if created successfully
     */
    bool createDefaultConfig(const std::string& file_path);
    
    /**
     * @brief Convert Section enum to string
     * @param section Section enum value
     * @return Section name as string
     */
    static std::string sectionToString(Section section);
    
    /**
     * @brief Convert string to Section enum
     * @param section_name Section name as string
     * @return Section enum value
     */
    static std::optional<Section> stringToSection(const std::string& section_name);
};

// Template implementations
template<typename T>
T ConfigManager::getValue(const std::string& key_path, const T& default_value) const {
    try {
        auto pointer = json::json_pointer(key_path);
        if (config_data.contains(pointer)) {
            return config_data[pointer].get<T>();
        }
    } catch (const std::exception&) {
        // Invalid key path or type conversion failed
    }
    
    return default_value;
}

template<typename T>
bool ConfigManager::setValue(const std::string& key_path, const T& value) {
    if (!is_loaded) {
        return false;
    }
    
    try {
        auto pointer = json::json_pointer(key_path);
        config_data[pointer] = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

