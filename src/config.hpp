#pragma once

#include "models.hpp"
#include <string>

class ConfigManager {
public:
    static bool saveToFile(const AppConfig& config, const std::string& filePath);
    static bool loadFromFile(AppConfig& config, const std::string& filePath);
    static std::string serializeToJson(const AppConfig& config);
    static bool deserializeFromJson(AppConfig& config, const std::string& json);
    static AppConfig getDefaultConfig();
    static uint32_t parseUnsigned(const std::string& value);
};
