#pragma once

#include "models.hpp"
#include <windows.h>
#include <string>

class RegistryManager {
public:
    static const wchar_t* REG_SUBKEY;

    static bool saveConfig(const AppConfig& config);
    static bool loadConfig(AppConfig& config);
    static bool resetConfig();
};
