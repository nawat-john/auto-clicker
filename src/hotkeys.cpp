#include "hotkeys.hpp"

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

HotkeyManager::HotkeyManager() {
}

HotkeyManager::~HotkeyManager() {
    unregister();
}

bool HotkeyManager::init(HWND hwnd, UINT vkStartStop, UINT vkPanic, UINT vkPick) {
    m_hwnd = hwnd;
    m_vkStartStop = vkStartStop;
    m_vkPanic = vkPanic;
    m_vkPick = vkPick;

    unregister();

    bool s1 = RegisterHotKey(m_hwnd, static_cast<int>(HotkeyId::START_STOP), MOD_NOREPEAT, m_vkStartStop);
    bool s2 = RegisterHotKey(m_hwnd, static_cast<int>(HotkeyId::PANIC_STOP), MOD_NOREPEAT, m_vkPanic);
    bool s3 = RegisterHotKey(m_hwnd, static_cast<int>(HotkeyId::PICK_POS),   MOD_NOREPEAT, m_vkPick);

    m_registered = s1 && s2 && s3;
    return m_registered;
}

void HotkeyManager::unregister() {
    if (m_hwnd) {
        UnregisterHotKey(m_hwnd, static_cast<int>(HotkeyId::START_STOP));
        UnregisterHotKey(m_hwnd, static_cast<int>(HotkeyId::PANIC_STOP));
        UnregisterHotKey(m_hwnd, static_cast<int>(HotkeyId::PICK_POS));
        m_registered = false;
    }
}

bool HotkeyManager::updateHotkeys(UINT vkStartStop, UINT vkPanic, UINT vkPick) {
    if (!m_hwnd) return false;
    return init(m_hwnd, vkStartStop, vkPanic, vkPick);
}

POINT HotkeyManager::getCurrentMousePos() {
    POINT pt = {0, 0};
    GetCursorPos(&pt);
    return pt;
}
