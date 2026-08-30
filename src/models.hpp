#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdint>

enum class ActionType {
    MOUSE_LEFT_CLICK = 0,
    MOUSE_RIGHT_CLICK,
    MOUSE_MIDDLE_CLICK,
    MOUSE_DOUBLE_LEFT,
    KEY_PRESS
};

enum class CoordType {
    FOLLOW_MOUSE = 0,
    FIXED_COORDS
};

enum class ExecutionMode {
    SYNC = 0,  // Sequential order
    ASYNC      // Independent multi-threading per item
};

struct ActionItem {
    int id = 0;
    std::string name = "Target";
    ActionType actionType = ActionType::MOUSE_LEFT_CLICK;
    CoordType coordType = CoordType::FOLLOW_MOUSE;
    int x = 0;
    int y = 0;
    bool restoreMouse = false;
    WORD keyVk = 'E';
    std::string keyName = "E";
    bool useCustomRate = false;
    uint32_t cpm = 600;         // Clicks Per Minute
    uint32_t intervalMs = 100;  // Interval in milliseconds
    uint32_t delayBeforeMs = 0; // Sync mode delay before execution
    uint32_t delayAfterMs = 0;  // Sync mode delay after execution
    bool enabled = true;

    void updateIntervalFromCpm() {
        if (cpm > 0) {
            intervalMs = static_cast<uint32_t>((60000.0 / cpm) + 0.5);
            if (intervalMs == 0) intervalMs = 1;
        }
    }

    void updateCpmFromInterval() {
        if (intervalMs > 0) {
            cpm = static_cast<uint32_t>((60000.0 / intervalMs) + 0.5);
        }
    }
};

struct AppConfig {
    ExecutionMode mode = ExecutionMode::SYNC;
    uint32_t globalCpm = 600;
    uint32_t globalIntervalMs = 100;
    uint32_t randomJitterMs = 0;
    uint32_t repeatCount = 0; // 0 = Infinite
    bool soundFeedback = true;

    UINT hotkeyStartStop = VK_F6;
    UINT hotkeyPick = VK_F8;
    UINT hotkeyPanic = VK_F7;

    std::vector<ActionItem> items;

    void updateIntervalFromCpm() {
        if (globalCpm > 0) {
            globalIntervalMs = static_cast<uint32_t>((60000.0 / globalCpm) + 0.5);
            if (globalIntervalMs == 0) globalIntervalMs = 1;
        }
    }

    void updateCpmFromInterval() {
        if (globalIntervalMs > 0) {
            globalCpm = static_cast<uint32_t>((60000.0 / globalIntervalMs) + 0.5);
        }
    }

    static std::string actionTypeToString(ActionType type) {
        switch (type) {
            case ActionType::MOUSE_LEFT_CLICK: return "Left Click";
            case ActionType::MOUSE_RIGHT_CLICK: return "Right Click";
            case ActionType::MOUSE_MIDDLE_CLICK: return "Middle Click";
            case ActionType::MOUSE_DOUBLE_LEFT: return "Double Click";
            case ActionType::KEY_PRESS: return "Key Press";
            default: return "Unknown";
        }
    }

    static ActionType stringToActionType(const std::string& str) {
        if (str == "Right Click" || str == "MOUSE_RIGHT_CLICK") return ActionType::MOUSE_RIGHT_CLICK;
        if (str == "Middle Click" || str == "MOUSE_MIDDLE_CLICK") return ActionType::MOUSE_MIDDLE_CLICK;
        if (str == "Double Click" || str == "MOUSE_DOUBLE_LEFT") return ActionType::MOUSE_DOUBLE_LEFT;
        if (str == "Key Press" || str == "KEY_PRESS") return ActionType::KEY_PRESS;
        return ActionType::MOUSE_LEFT_CLICK;
    }

    static std::string vkToString(WORD vk) {
        if (vk >= 'A' && vk <= 'Z') return std::string(1, static_cast<char>(vk));
        if (vk >= '0' && vk <= '9') return std::string(1, static_cast<char>(vk));
        if (vk >= VK_F1 && vk <= VK_F24) return "F" + std::to_string(vk - VK_F1 + 1);
        if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return "Num " + std::to_string(vk - VK_NUMPAD0);
        switch (vk) {
            case VK_SPACE: return "SPACE";
            case VK_RETURN: return "ENTER";
            case VK_TAB: return "TAB";
            case VK_SHIFT: return "SHIFT";
            case VK_CONTROL: return "CTRL";
            case VK_MENU: return "ALT";
            case VK_ESCAPE: return "ESC";
            case VK_BACK: return "BACKSPACE";
            case VK_LEFT: return "LEFT";
            case VK_RIGHT: return "RIGHT";
            case VK_UP: return "UP";
            case VK_DOWN: return "DOWN";
            case VK_DELETE: return "DELETE";
            case VK_INSERT: return "INSERT";
            case VK_HOME: return "HOME";
            case VK_END: return "END";
            case VK_PRIOR: return "PAGE UP";
            case VK_NEXT: return "PAGE DOWN";
            case VK_OEM_1: return ";";
            case VK_OEM_PLUS: return "=";
            case VK_OEM_COMMA: return ",";
            case VK_OEM_MINUS: return "-";
            case VK_OEM_PERIOD: return ".";
            case VK_OEM_2: return "/";
            case VK_OEM_3: return "`";
            case VK_OEM_4: return "[";
            case VK_OEM_5: return "\\";
            case VK_OEM_6: return "]";
            case VK_OEM_7: return "'";
            default: return "0x" + std::to_string(vk);
        }
    }

    static WORD stringToVk(const std::string& str) {
        std::string s = str;
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        if (s.length() == 1) {
            char c = s[0];
            if (c >= 'A' && c <= 'Z') return static_cast<WORD>(c);
            if (c >= '0' && c <= '9') return static_cast<WORD>(c);
            if (c == ';') return VK_OEM_1;
            if (c == '=') return VK_OEM_PLUS;
            if (c == ',') return VK_OEM_COMMA;
            if (c == '-') return VK_OEM_MINUS;
            if (c == '.') return VK_OEM_PERIOD;
            if (c == '/') return VK_OEM_2;
            if (c == '`') return VK_OEM_3;
            if (c == '[') return VK_OEM_4;
            if (c == '\\') return VK_OEM_5;
            if (c == ']') return VK_OEM_6;
            if (c == '\'') return VK_OEM_7;
        }
        if (s.rfind("F", 0) == 0 && s.length() <= 3) {
            try {
                int fNum = std::stoi(s.substr(1));
                if (fNum >= 1 && fNum <= 24) return static_cast<WORD>(VK_F1 + fNum - 1);
            } catch (...) {}
        }
        if (s == "SPACE") return VK_SPACE;
        if (s == "ENTER" || s == "RETURN") return VK_RETURN;
        if (s == "TAB") return VK_TAB;
        if (s == "SHIFT") return VK_SHIFT;
        if (s == "CTRL" || s == "CONTROL") return VK_CONTROL;
        if (s == "ALT") return VK_MENU;
        if (s == "ESC" || s == "ESCAPE") return VK_ESCAPE;
        if (s == "BACKSPACE" || s == "BACK") return VK_BACK;
        if (s == "LEFT") return VK_LEFT;
        if (s == "RIGHT") return VK_RIGHT;
        if (s == "UP") return VK_UP;
        if (s == "DOWN") return VK_DOWN;
        if (s == "DELETE" || s == "DEL") return VK_DELETE;
        if (s == "INSERT" || s == "INS") return VK_INSERT;
        if (s == "HOME") return VK_HOME;
        if (s == "END") return VK_END;
        if (s == "PAGE UP" || s == "PAGEUP") return VK_PRIOR;
        if (s == "PAGE DOWN" || s == "PAGEDOWN") return VK_NEXT;
        return 'E';
    }
};
