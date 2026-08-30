#pragma once

#include <windows.h>
#include <functional>

enum class HotkeyId {
    START_STOP = 1001,
    PANIC_STOP = 1002,
    PICK_POS   = 1003
};

class HotkeyManager {
public:
    using HotkeyCallback = std::function<void(HotkeyId id)>;

    HotkeyManager();
    ~HotkeyManager();

    bool init(HWND hwnd, UINT vkStartStop = VK_F6, UINT vkPanic = VK_F7, UINT vkPick = VK_F8);
    void unregister();
    bool updateHotkeys(UINT vkStartStop, UINT vkPanic, UINT vkPick);

    static POINT getCurrentMousePos();

private:
    HWND m_hwnd = nullptr;
    UINT m_vkStartStop = VK_F6;
    UINT m_vkPanic = VK_F7;
    UINT m_vkPick = VK_F8;
    bool m_registered = false;
};
