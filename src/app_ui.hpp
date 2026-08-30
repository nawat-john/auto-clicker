#pragma once

#include "models.hpp"
#include "engine.hpp"
#include "hotkeys.hpp"
#include "registry.hpp"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <functional>

class AppUI {
public:
    AppUI(HINSTANCE hInstance);
    ~AppUI();

    bool create(int nCmdShow);
    int run();

    static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK DialogWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK PickerOverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK CustomButtonProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    void startPositionPicker(std::function<void(POINT, bool)> onFinished);

private:
    LRESULT handleMainMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleDialogMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handlePickerMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void setupDPI();
    void createMainControls();
    void initListView();
    void updateListView();
    void updateUIFromConfig();
    void updateConfigFromUI();
    void autoSaveToRegistry();
    void toggleStartStop();
    void onEmergencyStop();
    void onPickPosition();
    void openItemDialog(int itemIndex = -1, bool defaultToKey = false);
    void deleteSelectedItem();
    void moveSelectedItem(bool up);
    void saveProfilePrompt();
    void loadProfilePrompt();
    void loadPreset(int presetIndex);

    void applyDarkTheme(HWND hwnd);

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwndMain = nullptr;
    HWND m_hDlgCurrent = nullptr;
    HWND m_hPickerOverlay = nullptr;

    // Main UI Controls
    HWND m_hStatusLabel = nullptr;
    HWND m_hCountLabel = nullptr;
    HWND m_hBtnStart = nullptr;
    HWND m_hBtnPick = nullptr;
    HWND m_hRadioSync = nullptr;
    HWND m_hRadioAsync = nullptr;
    HWND m_hEditCpm = nullptr;
    HWND m_hEditInterval = nullptr;
    HWND m_hEditJitter = nullptr;
    HWND m_hEditRepeat = nullptr;
    HWND m_hChkSound = nullptr;
    HWND m_hListView = nullptr;
    HWND m_hPresetCombo = nullptr;

    // Brushes and Fonts
    HFONT m_hFontMain = nullptr;
    HFONT m_hFontBold = nullptr;
    HFONT m_hFontTitle = nullptr;
    HFONT m_hFontSmall = nullptr;
    HFONT m_hFontPicker = nullptr;
    HBRUSH m_hBrBg = nullptr;
    HBRUSH m_hBrCard = nullptr;
    HBRUSH m_hBrInput = nullptr;

    AppConfig m_config;
    ClickerEngine m_engine;
    HotkeyManager m_hotkeys;

    // Temporary storage for item editing dialog
    ActionItem m_tempItem;
    int m_tempItemIndex = -1;
    bool m_tempIsNew = false;
    bool m_isUpdatingListView = false;

    // Position picker state
    std::function<void(POINT, bool)> m_pickerCallback;
    POINT m_pickerCurPos{0, 0};

    static AppUI* s_instance;
};
