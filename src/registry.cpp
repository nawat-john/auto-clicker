#include "registry.hpp"
#include "config.hpp"
#include <vector>

const wchar_t* RegistryManager::REG_SUBKEY = L"Software\\UltraAutoClicker";

static bool setDword(HKEY hKey, const wchar_t* valueName, DWORD value) {
    return RegSetValueExW(hKey, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD)) == ERROR_SUCCESS;
}

static bool getDword(HKEY hKey, const wchar_t* valueName, DWORD& value) {
    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    return RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size) == ERROR_SUCCESS && type == REG_DWORD;
}

static bool setString(HKEY hKey, const wchar_t* valueName, const std::string& str) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return false;
    std::vector<wchar_t> wstr(wlen);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wstr.data(), wlen);
    return RegSetValueExW(hKey, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(wstr.data()), static_cast<DWORD>(wstr.size() * sizeof(wchar_t))) == ERROR_SUCCESS;
}

static bool getString(HKEY hKey, const wchar_t* valueName, std::string& str) {
    DWORD size = 0;
    DWORD type = 0;
    if (RegQueryValueExW(hKey, valueName, nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_SZ) {
        return false;
    }
    std::vector<wchar_t> wbuf(size / sizeof(wchar_t) + 1, 0);
    if (RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<BYTE*>(wbuf.data()), &size) != ERROR_SUCCESS) {
        return false;
    }
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, nullptr, 0, nullptr, nullptr);
    if (ulen <= 0) return false;
    std::vector<char> ubuf(ulen);
    WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, ubuf.data(), ulen, nullptr, nullptr);
    str = ubuf.data();
    return true;
}

bool RegistryManager::saveConfig(const AppConfig& config) {
    HKEY hKey = nullptr;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, &disposition) != ERROR_SUCCESS) {
        return false;
    }

    setDword(hKey, L"ExecutionMode", static_cast<DWORD>(config.mode));
    setDword(hKey, L"GlobalCPM", config.globalCpm);
    setDword(hKey, L"GlobalIntervalMs", config.globalIntervalMs);
    setDword(hKey, L"RandomJitterMs", config.randomJitterMs);
    setDword(hKey, L"RepeatCount", config.repeatCount);
    setDword(hKey, L"SoundFeedback", config.soundFeedback ? 1 : 0);
    setDword(hKey, L"HotkeyStartStop", config.hotkeyStartStop);
    setDword(hKey, L"HotkeyPick", config.hotkeyPick);
    setDword(hKey, L"HotkeyPanic", config.hotkeyPanic);

    std::string json = ConfigManager::serializeToJson(config);
    setString(hKey, L"ConfigJson", json);

    RegCloseKey(hKey);
    return true;
}

bool RegistryManager::loadConfig(AppConfig& config) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SUBKEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    std::string json;
    if (getString(hKey, L"ConfigJson", json) && !json.empty()) {
        if (ConfigManager::deserializeFromJson(config, json)) {
            RegCloseKey(hKey);
            return true;
        }
    }

    // Fallback to individual keys if Json is absent
    DWORD dval = 0;
    if (getDword(hKey, L"ExecutionMode", dval)) config.mode = static_cast<ExecutionMode>(dval);
    if (getDword(hKey, L"GlobalCPM", dval)) config.globalCpm = dval;
    if (getDword(hKey, L"GlobalIntervalMs", dval)) config.globalIntervalMs = dval;
    if (getDword(hKey, L"RandomJitterMs", dval)) config.randomJitterMs = dval;
    if (getDword(hKey, L"RepeatCount", dval)) config.repeatCount = dval;
    if (getDword(hKey, L"SoundFeedback", dval)) config.soundFeedback = (dval != 0);
    if (getDword(hKey, L"HotkeyStartStop", dval)) config.hotkeyStartStop = dval;
    if (getDword(hKey, L"HotkeyPick", dval)) config.hotkeyPick = dval;
    if (getDword(hKey, L"HotkeyPanic", dval)) config.hotkeyPanic = dval;

    RegCloseKey(hKey);
    return true;
}

bool RegistryManager::resetConfig() {
    return RegDeleteKeyW(HKEY_CURRENT_USER, REG_SUBKEY) == ERROR_SUCCESS;
}
