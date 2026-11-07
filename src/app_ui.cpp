#include "app_ui.hpp"
#include "config.hpp"
#include "registry.hpp"
#include <dwmapi.h>
#include <uxtheme.h>
#include <commdlg.h>
#include <sstream>
#include <iomanip>
#include <vector>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#define IDI_APP_ICON 101
#define IDB_APP_LOGO 102

// Main Window Control IDs
enum ControlIds {
    ID_BTN_START = 2001,
    ID_BTN_PICK,
    ID_RADIO_SYNC,
    ID_RADIO_ASYNC,
    ID_EDIT_CPM,
    ID_EDIT_INTERVAL,
    ID_EDIT_JITTER,
    ID_EDIT_REPEAT,
    ID_CHK_SOUND,
    ID_LISTVIEW_ITEMS,
    ID_BTN_ADD_CLICK,
    ID_BTN_ADD_KEY,
    ID_BTN_EDIT_ITEM,
    ID_BTN_DEL_ITEM,
    ID_BTN_MOVE_UP,
    ID_BTN_MOVE_DOWN,
    ID_BTN_SAVE_PROFILE,
    ID_BTN_LOAD_PROFILE,
    ID_COMBO_PRESETS,
    ID_RADIO_RECORDED,
    ID_BTN_ADD_RECORDING,
    ID_TIMER_UPDATE = 3001
};

// Item Dialog Control IDs
enum DialogControlIds {
    ID_DLG_EDIT_NAME = 4001,
    ID_DLG_COMBO_TYPE,
    ID_DLG_RADIO_FOLLOW,
    ID_DLG_RADIO_FIXED,
    ID_DLG_EDIT_X,
    ID_DLG_EDIT_Y,
    ID_DLG_BTN_PICK,
    ID_DLG_CHK_RESTORE,
    ID_DLG_EDIT_KEY,
    ID_DLG_CHK_CUSTOM_RATE,
    ID_DLG_EDIT_CPM,
    ID_DLG_EDIT_DELAY_BEFORE,
    ID_DLG_EDIT_DELAY_AFTER,
    ID_DLG_CHK_ENABLED,
    ID_DLG_BTN_OK,
    ID_DLG_BTN_CANCEL
};

AppUI* AppUI::s_instance = nullptr;

static std::wstring utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return L"";
    std::vector<wchar_t> wstr(wlen);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wstr.data(), wlen);
    return std::wstring(wstr.data());
}

static std::string wideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (ulen <= 0) return "";
    std::vector<char> ustr(ulen);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, ustr.data(), ulen, nullptr, nullptr);
    return std::string(ustr.data());
}

AppUI::AppUI(HINSTANCE hInstance) : m_hInstance(hInstance) {
    s_instance = this;

    // Load from Windows Registry first, fallback to default config
    if (!RegistryManager::loadConfig(m_config) ||
        (m_config.items.empty() && m_config.mode != ExecutionMode::RECORDED_KEYS)) {
        m_config = ConfigManager::getDefaultConfig();
        RegistryManager::saveConfig(m_config);
    }
    m_engine.setConfig(m_config);

    // Color Palette: Clean White Light Theme
    m_hBrBg = CreateSolidBrush(RGB(255, 255, 255));       // Main window pure white bg
    m_hBrCard = CreateSolidBrush(RGB(248, 250, 252));     // Group / Card bg
    m_hBrInput = CreateSolidBrush(RGB(255, 255, 255));    // Edit inputs white bg

    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
}

AppUI::~AppUI() {
    m_hotkeys.unregister();
    m_macro.disable();
    m_engine.stop();

    if (m_hFontMain) DeleteObject(m_hFontMain);
    if (m_hFontBold) DeleteObject(m_hFontBold);
    if (m_hFontTitle) DeleteObject(m_hFontTitle);
    if (m_hFontSmall) DeleteObject(m_hFontSmall);
    if (m_hFontPicker) DeleteObject(m_hFontPicker);
    if (m_hBrBg) DeleteObject(m_hBrBg);
    if (m_hBrCard) DeleteObject(m_hBrCard);
    if (m_hBrInput) DeleteObject(m_hBrInput);
}

void AppUI::setupDPI() {
    typedef BOOL(WINAPI *SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        auto proc = reinterpret_cast<SetProcessDpiAwarenessContextProc>(
            reinterpret_cast<void*>(GetProcAddress(hUser32, "SetProcessDpiAwarenessContext"))
        );
        if (proc) {
            proc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }
}

void AppUI::applyDarkTheme(HWND hwnd) {
    // Light theme (Standard clean titlebar)
    BOOL useDarkMode = FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
}

void AppUI::autoSaveToRegistry() {
    RegistryManager::saveConfig(m_config);
}

// Custom button painting procedure for crisp modern light theme buttons
LRESULT CALLBACK AppUI::CustomButtonProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    (void)uIdSubclass;
    DWORD btnType = static_cast<DWORD>(dwRefData); // 0=Default, 1=Start/Stop, 2=Action/Primary, 3=Secondary

    switch (msg) {
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, AppUI::CustomButtonProc, uIdSubclass);
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);

            bool isPressed = (SendMessage(hwnd, BM_GETSTATE, 0, 0) & BST_PUSHED) != 0;
            bool isHover = false;
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            if (PtInRect(&rc, pt)) isHover = true;

            // Determine Colors
            COLORREF bgColor;
            COLORREF borderColor;
            COLORREF textColor;

            if (btnType == 1) { // Start/Stop Toggle Button
                bool isRunning = s_instance && s_instance->isRunning();
                if (isRunning) {
                    bgColor = isPressed ? RGB(185, 28, 28) : (isHover ? RGB(220, 38, 38) : RGB(239, 68, 68)); // Red
                    borderColor = bgColor;
                    textColor = RGB(255, 255, 255);
                } else {
                    bgColor = isPressed ? RGB(21, 128, 61) : (isHover ? RGB(22, 163, 74) : RGB(16, 185, 129)); // Green
                    borderColor = bgColor;
                    textColor = RGB(255, 255, 255);
                }
            } else if (btnType == 2) { // Primary / Action (Blue)
                bgColor = isPressed ? RGB(29, 78, 216) : (isHover ? RGB(37, 99, 235) : RGB(59, 130, 246));
                borderColor = bgColor;
                textColor = RGB(255, 255, 255);
            } else { // Secondary / Neutral (Light Slate with crisp dark text)
                bgColor = isPressed ? RGB(203, 213, 225) : (isHover ? RGB(226, 232, 240) : RGB(241, 245, 249));
                borderColor = isHover ? RGB(148, 163, 184) : RGB(203, 213, 225);
                textColor = RGB(15, 23, 42); // Deep Charcoal text
            }

            // Fill background with rounded rectangle
            HBRUSH hBrush = CreateSolidBrush(bgColor);
            HPEN hPen = CreatePen(PS_SOLID, 1, borderColor);
            HGDIOBJ oldBrush = SelectObject(hdc, hBrush);
            HGDIOBJ oldPen = SelectObject(hdc, hPen);

            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);

            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(hBrush);
            DeleteObject(hPen);

            // Draw Button Text
            wchar_t text[128];
            GetWindowTextW(hwnd, text, 128);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, textColor);

            HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
            HGDIOBJ oldFont = SelectObject(hdc, hFont);

            if (isPressed) {
                rc.left += 1;
                rc.top += 1;
            }

            DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, oldFont);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

bool AppUI::create(int nCmdShow) {
    setupDPI();

    // Load App Icon
    HICON hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!hIcon) {
        hIcon = (HICON)LoadImageW(m_hInstance, L"res/app_icon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    }

    // Register Main Window Class
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = AppUI::MainWndProc;
    wc.hInstance = m_hInstance;
    wc.hIcon = hIcon ? hIcon : LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = hIcon ? hIcon : LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = m_hBrBg;
    wc.lpszClassName = L"AutoClickerMainWindowClass";
    RegisterClassExW(&wc);

    // Register Dedicated Dialog Window Class
    WNDCLASSEXW wcd{};
    wcd.cbSize = sizeof(WNDCLASSEXW);
    wcd.style = CS_HREDRAW | CS_VREDRAW;
    wcd.lpfnWndProc = AppUI::DialogWndProc;
    wcd.hInstance = m_hInstance;
    wcd.hIcon = hIcon;
    wcd.hIconSm = hIcon;
    wcd.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcd.hbrBackground = m_hBrBg;
    wcd.lpszClassName = L"AutoClickerItemDialogClass";
    RegisterClassExW(&wcd);

    // Register Fullscreen Crosshair Picker Overlay Window Class
    WNDCLASSEXW wcp{};
    wcp.cbSize = sizeof(WNDCLASSEXW);
    wcp.style = CS_HREDRAW | CS_VREDRAW;
    wcp.lpfnWndProc = AppUI::PickerOverlayWndProc;
    wcp.hInstance = m_hInstance;
    wcp.hCursor = LoadCursor(nullptr, IDC_CROSS);
    wcp.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcp.lpszClassName = L"AutoClickerPickerOverlayClass";
    RegisterClassExW(&wcp);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = 860;
    int windowHeight = 700;
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;

    m_hwndMain = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"Ultra Auto Clicker & Key Spammer (C++20)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, windowWidth, windowHeight,
        nullptr, nullptr, m_hInstance, this
    );

    if (!m_hwndMain) return false;

    if (hIcon) {
        SendMessageW(m_hwndMain, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessageW(m_hwndMain, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    applyDarkTheme(m_hwndMain);

    m_hFontMain = CreateFontW(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontBold = CreateFontW(17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontTitle = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontSmall = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontPicker = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    createMainControls();
    initListView();
    bool legacyLoaded = m_macro.load(KeyboardMacro::recordingPath());
    if (m_config.version < 2) {
        if (legacyLoaded) addRecordedItem(m_macro.recording());
        m_config.version = 2;
        autoSaveToRegistry();
    }
    updateUIFromConfig();
    updateListView();

    SetTimer(m_hwndMain, ID_TIMER_UPDATE, 100, nullptr);

    ShowWindow(m_hwndMain, nCmdShow);
    UpdateWindow(m_hwndMain);
    return true;
}

void AppUI::createMainControls() {
    // Header Logo Icon
    HBITMAP hBmpLogo = (HBITMAP)LoadImageW(m_hInstance, MAKEINTRESOURCEW(IDB_APP_LOGO), IMAGE_BITMAP, 44, 44, LR_DEFAULTCOLOR);
    if (!hBmpLogo) {
        hBmpLogo = (HBITMAP)LoadImageW(nullptr, L"res/app_logo.bmp", IMAGE_BITMAP, 44, 44, LR_LOADFROMFILE | LR_DEFAULTCOLOR);
    }
    if (hBmpLogo) {
        HWND hLogoPic = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_REALSIZECONTROL, 25, 12, 44, 44, m_hwndMain, nullptr, m_hInstance, nullptr);
        SendMessageW(hLogoPic, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)hBmpLogo);
    }

    // Top Status Bar
    m_hStatusLabel = CreateWindowExW(0, L"STATIC", L"[ STOPPED ] Press F6 to Start",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 80, 18, 305, 30, m_hwndMain, nullptr, m_hInstance, nullptr);

    m_hCountLabel = CreateWindowExW(0, L"STATIC", L"Total Clicks: 0",
        WS_CHILD | WS_VISIBLE | SS_RIGHT, 390, 22, 140, 26, m_hwndMain, nullptr, m_hInstance, nullptr);

    m_hBtnStart = CreateWindowExW(0, L"BUTTON", L"START (F6)",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 545, 14, 135, 40, m_hwndMain, (HMENU)ID_BTN_START, m_hInstance, nullptr);
    SetWindowSubclass(m_hBtnStart, AppUI::CustomButtonProc, 1, 1); // 1 = Start/Stop theme

    m_hBtnPick = CreateWindowExW(0, L"BUTTON", L"Pick Pos (F8)",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 690, 14, 135, 40, m_hwndMain, (HMENU)ID_BTN_PICK, m_hInstance, nullptr);
    SetWindowSubclass(m_hBtnPick, AppUI::CustomButtonProc, 2, 2); // 2 = Primary Blue

    // Group 1: Global Settings & Timing Engine
    CreateWindowExW(0, L"BUTTON", L" Global Engine Settings ",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 25, 68, 800, 110, m_hwndMain, nullptr, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Mode:",
        WS_CHILD | WS_VISIBLE, 40, 96, 50, 20, m_hwndMain, nullptr, m_hInstance, nullptr);

    m_hRadioSync = CreateWindowExW(0, L"BUTTON", L"Sync (Sequential)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 95, 95, 145, 22, m_hwndMain, (HMENU)ID_RADIO_SYNC, m_hInstance, nullptr);

    m_hRadioAsync = CreateWindowExW(0, L"BUTTON", L"Async (Independent Threads)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 250, 95, 220, 22, m_hwndMain, (HMENU)ID_RADIO_ASYNC, m_hInstance, nullptr);

    m_hRadioRecorded = CreateWindowExW(0, L"BUTTON", L"Recorded Keys (hold F5)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 490, 95, 300, 22, m_hwndMain, (HMENU)ID_RADIO_RECORDED, m_hInstance, nullptr);

    // Rate Setting
    CreateWindowExW(0, L"STATIC", L"Global CPM:",
        WS_CHILD | WS_VISIBLE, 40, 135, 90, 20, m_hwndMain, nullptr, m_hInstance, nullptr);

    m_hEditCpm = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"600",
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, 135, 132, 75, 24, m_hwndMain, (HMENU)ID_EDIT_CPM, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Interval (ms):",
        WS_CHILD | WS_VISIBLE, 225, 135, 95, 20, m_hwndMain, nullptr, m_hInstance, nullptr);

    m_hEditInterval = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"100",
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, 325, 132, 70, 24, m_hwndMain, (HMENU)ID_EDIT_INTERVAL, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Jitter (+-ms):",
        WS_CHILD | WS_VISIBLE, 415, 135, 90, 20, m_hwndMain, nullptr, m_hInstance, nullptr);

    m_hEditJitter = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0",
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, 510, 132, 50, 24, m_hwndMain, (HMENU)ID_EDIT_JITTER, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Repeat:",
        WS_CHILD | WS_VISIBLE, 575, 135, 60, 20, m_hwndMain, nullptr, m_hInstance, nullptr);

    m_hEditRepeat = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0",
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, 640, 132, 50, 24, m_hwndMain, (HMENU)ID_EDIT_REPEAT, m_hInstance, nullptr);

    m_hChkSound = CreateWindowExW(0, L"BUTTON", L"Sound",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 715, 133, 75, 22, m_hwndMain, (HMENU)ID_CHK_SOUND, m_hInstance, nullptr);

    // Group 2: Action Items Queue
    CreateWindowExW(0, L"BUTTON", L" Target Action Items (Click Targets & Key Spammers) ",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 25, 190, 800, 415, m_hwndMain, nullptr, m_hInstance, nullptr);

    // Action buttons for ListView
    HWND hBtnAddClick = CreateWindowExW(0, L"BUTTON", L"+ Mouse Click",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 40, 218, 120, 32, m_hwndMain, (HMENU)ID_BTN_ADD_CLICK, m_hInstance, nullptr);
    SetWindowSubclass(hBtnAddClick, AppUI::CustomButtonProc, 3, 3);

    HWND hBtnAddKey = CreateWindowExW(0, L"BUTTON", L"+ Key Press",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 170, 218, 110, 32, m_hwndMain, (HMENU)ID_BTN_ADD_KEY, m_hInstance, nullptr);
    SetWindowSubclass(hBtnAddKey, AppUI::CustomButtonProc, 3, 3);

    HWND hBtnEdit = CreateWindowExW(0, L"BUTTON", L"Edit Item",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 290, 218, 90, 32, m_hwndMain, (HMENU)ID_BTN_EDIT_ITEM, m_hInstance, nullptr);
    SetWindowSubclass(hBtnEdit, AppUI::CustomButtonProc, 3, 3);

    HWND hBtnDel = CreateWindowExW(0, L"BUTTON", L"Delete",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 390, 218, 80, 32, m_hwndMain, (HMENU)ID_BTN_DEL_ITEM, m_hInstance, nullptr);
    SetWindowSubclass(hBtnDel, AppUI::CustomButtonProc, 3, 3);

    HWND hBtnUp = CreateWindowExW(0, L"BUTTON", L"Up",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 480, 218, 55, 32, m_hwndMain, (HMENU)ID_BTN_MOVE_UP, m_hInstance, nullptr);
    SetWindowSubclass(hBtnUp, AppUI::CustomButtonProc, 3, 3);

    HWND hBtnDown = CreateWindowExW(0, L"BUTTON", L"Down",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 545, 218, 65, 32, m_hwndMain, (HMENU)ID_BTN_MOVE_DOWN, m_hInstance, nullptr);
    SetWindowSubclass(hBtnDown, AppUI::CustomButtonProc, 3, 3);

    CreateWindowExW(0, L"STATIC", L"Preset:",
        WS_CHILD | WS_VISIBLE, 620, 224, 50, 20, m_hwndMain, nullptr, m_hInstance, nullptr);

    m_hPresetCombo = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 675, 221, 135, 150, m_hwndMain, (HMENU)ID_COMBO_PRESETS, m_hInstance, nullptr);

    SendMessageW(m_hPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"1. Fast Left Click");
    SendMessageW(m_hPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"2. Dual Point Sync");
    SendMessageW(m_hPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"3. Key [E] Spammer");
    SendMessageW(m_hPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"4. Multi Async Combo");
    SendMessageW(m_hPresetCombo, CB_SETCURSEL, 0, 0);

    // ListView Control
    m_hListView = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_VSCROLL,
        40, 260, 770, 330,
        m_hwndMain,
        (HMENU)ID_LISTVIEW_ITEMS,
        m_hInstance,
        nullptr
    );

    ListView_SetExtendedListViewStyle(m_hListView, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(m_hListView, RGB(255, 255, 255));
    ListView_SetTextBkColor(m_hListView, RGB(255, 255, 255));
    ListView_SetTextColor(m_hListView, RGB(15, 23, 42));

    // Bottom Bar (Save / Load Profile)
    HWND hBtnSave = CreateWindowExW(0, L"BUTTON", L"Export Profile...",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 25, 615, 140, 34, m_hwndMain, (HMENU)ID_BTN_SAVE_PROFILE, m_hInstance, nullptr);
    SetWindowSubclass(hBtnSave, AppUI::CustomButtonProc, 3, 3);

    HWND hBtnLoad = CreateWindowExW(0, L"BUTTON", L"Import Profile...",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 175, 615, 140, 34, m_hwndMain, (HMENU)ID_BTN_LOAD_PROFILE, m_hInstance, nullptr);
    SetWindowSubclass(hBtnLoad, AppUI::CustomButtonProc, 3, 3);

    HWND hBtnRecord = CreateWindowExW(0, L"BUTTON", L"+ Record Keys",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 325, 615, 140, 34, m_hwndMain, (HMENU)ID_BTN_ADD_RECORDING, m_hInstance, nullptr);
    SetWindowSubclass(hBtnRecord, AppUI::CustomButtonProc, 3, 3);
    m_hRecordHint = CreateWindowExW(0, L"STATIC", L"Hold F5: new item | F6: play | F7: stop",
        WS_CHILD | WS_VISIBLE | SS_RIGHT, 475, 622, 350, 24, m_hwndMain, nullptr, m_hInstance, nullptr);

    // Apply Fonts
    EnumChildWindows(m_hwndMain, [](HWND hwnd, LPARAM lParam) -> BOOL {
        SendMessageW(hwnd, WM_SETFONT, lParam, TRUE);
        return TRUE;
    }, (LPARAM)m_hFontMain);

    SendMessageW(m_hStatusLabel, WM_SETFONT, (WPARAM)m_hFontTitle, TRUE);
    SendMessageW(m_hCountLabel, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
    SendMessageW(m_hBtnStart, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
    SendMessageW(m_hBtnPick, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
}

void AppUI::initListView() {
    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvc.iSubItem = 0;
    lvc.cx = 50;
    lvc.pszText = const_cast<LPWSTR>(L"#");
    ListView_InsertColumn(m_hListView, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.cx = 130;
    lvc.pszText = const_cast<LPWSTR>(L"Action Type");
    ListView_InsertColumn(m_hListView, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.cx = 160;
    lvc.pszText = const_cast<LPWSTR>(L"Target / Key");
    ListView_InsertColumn(m_hListView, 2, &lvc);

    lvc.iSubItem = 3;
    lvc.cx = 135;
    lvc.pszText = const_cast<LPWSTR>(L"Rate (CPM / ms)");
    ListView_InsertColumn(m_hListView, 3, &lvc);

    lvc.iSubItem = 4;
    lvc.cx = 115;
    lvc.pszText = const_cast<LPWSTR>(L"Delays (B/A)");
    ListView_InsertColumn(m_hListView, 4, &lvc);

    lvc.iSubItem = 5;
    lvc.cx = 170;
    lvc.pszText = const_cast<LPWSTR>(L"Name / Label");
    ListView_InsertColumn(m_hListView, 5, &lvc);
}

void AppUI::updateListView() {
    m_isUpdatingListView = true;
    ListView_DeleteAllItems(m_hListView);

    for (size_t i = 0; i < m_config.items.size(); ++i) {
        const auto& item = m_config.items[i];

        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = static_cast<int>(i);
        lvi.iSubItem = 0;

        std::wstring idStr = std::to_wstring(i + 1);
        lvi.pszText = const_cast<LPWSTR>(idStr.c_str());
        ListView_InsertItem(m_hListView, &lvi);

        // Checkbox state
        ListView_SetCheckState(m_hListView, i, item.enabled ? TRUE : FALSE);

        // Column 1: Action Type
        std::wstring typeStr = utf8ToWide(AppConfig::actionTypeToString(item.actionType));
        ListView_SetItemText(m_hListView, i, 1, const_cast<LPWSTR>(typeStr.c_str()));

        // Column 2: Target / Key
        std::wstring targetStr;
        if (item.actionType == ActionType::RECORDED_KEYS) {
            targetStr = std::to_wstring(item.recording.events.size()) + L" events / " +
                std::to_wstring(item.recording.durationUs / 1000) + L" ms";
        } else if (item.actionType == ActionType::KEY_PRESS) {
            targetStr = L"Key: [" + utf8ToWide(item.keyName) + L"]";
        } else {
            if (item.coordType == CoordType::FOLLOW_MOUSE) {
                targetStr = L"Follow Mouse";
            } else {
                targetStr = L"(" + std::to_wstring(item.x) + L", " + std::to_wstring(item.y) + L")";
                if (item.restoreMouse) targetStr += L" [Restore]";
            }
        }
        ListView_SetItemText(m_hListView, i, 2, const_cast<LPWSTR>(targetStr.c_str()));

        // Column 3: Rate
        std::wstring rateStr;
        if (item.actionType == ActionType::RECORDED_KEYS) {
            rateStr = L"Recorded timing";
        } else if (item.useCustomRate) {
            rateStr = std::to_wstring(item.cpm) + L" CPM (" + std::to_wstring(item.intervalMs) + L"ms)";
        } else {
            rateStr = L"Global (" + std::to_wstring(m_config.globalCpm) + L" CPM)";
        }
        ListView_SetItemText(m_hListView, i, 3, const_cast<LPWSTR>(rateStr.c_str()));

        // Column 4: Delays
        std::wstring delayStr = std::to_wstring(item.delayBeforeMs) + L" / " + std::to_wstring(item.delayAfterMs) + L" ms";
        ListView_SetItemText(m_hListView, i, 4, const_cast<LPWSTR>(delayStr.c_str()));

        // Column 5: Name
        std::wstring nameStr = utf8ToWide(item.name);
        ListView_SetItemText(m_hListView, i, 5, const_cast<LPWSTR>(nameStr.c_str()));
    }
    m_isUpdatingListView = false;
}

void AppUI::updateUIFromConfig() {
    SendMessageW(m_hRadioSync, BM_SETCHECK, m_config.mode == ExecutionMode::SYNC ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_hRadioAsync, BM_SETCHECK, m_config.mode == ExecutionMode::ASYNC ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_hRadioRecorded, BM_SETCHECK, m_config.mode == ExecutionMode::RECORDED_KEYS ? BST_CHECKED : BST_UNCHECKED, 0);

    SetWindowTextW(m_hEditCpm, std::to_wstring(m_config.globalCpm).c_str());
    SetWindowTextW(m_hEditInterval, std::to_wstring(m_config.globalIntervalMs).c_str());
    SetWindowTextW(m_hEditJitter, std::to_wstring(m_config.randomJitterMs).c_str());
    SetWindowTextW(m_hEditRepeat, std::to_wstring(m_config.repeatCount).c_str());
    SendMessageW(m_hChkSound, BM_SETCHECK, m_config.soundFeedback ? BST_CHECKED : BST_UNCHECKED, 0);
    updateKeyboardMode();
}

void AppUI::updateKeyboardMode() {
    bool recorded = m_config.mode == ExecutionMode::RECORDED_KEYS;
    if (recorded != m_recordedMode) {
        m_macro.setRecordingBlocked(true);
        m_macro.disable();
        m_engine.stop();
        m_hotkeys.unregister();
        m_recordedMode = recorded;
    }
    bool needsHook = recorded || std::any_of(m_config.items.begin(), m_config.items.end(),
        [](const ActionItem& item) { return item.actionType == ActionType::RECORDED_KEYS; });
    if (needsHook) {
        m_hotkeys.unregister();
        if (!m_macro.enable(m_hwndMain, m_config.hotkeyStartStop, m_config.hotkeyPanic, m_config.hotkeyPick)) {
            SetWindowTextW(m_hRecordHint, L"Keyboard hook failed; switch mode and try again.");
        }
    } else {
        m_macro.disable();
        m_hotkeys.init(m_hwndMain, m_config.hotkeyStartStop, m_config.hotkeyPanic, m_config.hotkeyPick);
    }
    m_macro.setRecordingEnabled(recorded);
    m_macro.setRecordingBlocked(m_engine.isRunning());
    for (HWND control : {m_hEditCpm, m_hEditInterval, m_hEditJitter}) EnableWindow(control, !recorded);
}

bool AppUI::addRecordedItem(const KeyRecording& recording) {
    if (recording.events.empty()) return false;
    ActionItem item;
    item.id = m_config.nextItemId();
    if (item.id == 0) return false;
    item.name = "Recorded Keys #" + std::to_string(item.id);
    item.actionType = ActionType::RECORDED_KEYS;
    item.recording = recording;
    m_config.items.push_back(std::move(item));
    updateListView();
    m_engine.setConfig(m_config);
    return RegistryManager::saveConfig(m_config);
}

void AppUI::updateConfigFromUI() {
    if (m_isUpdatingListView) return;

    m_config.mode = SendMessageW(m_hRadioRecorded, BM_GETCHECK, 0, 0) == BST_CHECKED ? ExecutionMode::RECORDED_KEYS :
        (SendMessageW(m_hRadioSync, BM_GETCHECK, 0, 0) == BST_CHECKED ? ExecutionMode::SYNC : ExecutionMode::ASYNC);
    updateKeyboardMode();

    HWND hFocus = GetFocus();
    wchar_t buf[64];

    if (hFocus == m_hEditInterval) {
        GetWindowTextW(m_hEditInterval, buf, 64);
        try { m_config.globalIntervalMs = ConfigManager::parseUnsigned(wideToUtf8(buf)); } catch (...) { m_config.globalIntervalMs = 600; }
        if (m_config.globalIntervalMs == 0) m_config.globalIntervalMs = 1;
        m_config.updateCpmFromInterval();
        SetWindowTextW(m_hEditCpm, std::to_wstring(m_config.globalCpm).c_str());
    } else {
        GetWindowTextW(m_hEditCpm, buf, 64);
        try { m_config.globalCpm = ConfigManager::parseUnsigned(wideToUtf8(buf)); } catch (...) { m_config.globalCpm = 100; }
        if (m_config.globalCpm == 0) m_config.globalCpm = 1;
        m_config.updateIntervalFromCpm();
        SetWindowTextW(m_hEditInterval, std::to_wstring(m_config.globalIntervalMs).c_str());
    }

    GetWindowTextW(m_hEditJitter, buf, 64);
    try { m_config.randomJitterMs = ConfigManager::parseUnsigned(wideToUtf8(buf)); } catch (...) { m_config.randomJitterMs = 0; }

    GetWindowTextW(m_hEditRepeat, buf, 64);
    try { m_config.repeatCount = ConfigManager::parseUnsigned(wideToUtf8(buf)); } catch (...) { m_config.repeatCount = 0; }

    m_config.soundFeedback = (SendMessageW(m_hChkSound, BM_GETCHECK, 0, 0) == BST_CHECKED);

    // Sync enabled checkboxes from ListView only when listview is valid and populated
    int lvCount = ListView_GetItemCount(m_hListView);
    if (lvCount == static_cast<int>(m_config.items.size()) && lvCount > 0) {
        for (size_t i = 0; i < m_config.items.size(); ++i) {
            m_config.items[i].enabled = (ListView_GetCheckState(m_hListView, i) != 0);
        }
    }

    m_engine.setConfig(m_config);
    autoSaveToRegistry();
}

void AppUI::toggleStartStop() {
    updateConfigFromUI();
    if (m_macro.isRecording()) return;

    if (m_engine.isRunning()) {
        m_engine.stop();
        m_macro.setRecordingBlocked(false);
        SetWindowTextW(m_hStatusLabel, L"[ STOPPED ] Press F6 to Start");
        SetWindowTextW(m_hBtnStart, L"START (F6)");
        InvalidateRect(m_hBtnStart, nullptr, TRUE);
    } else {
        m_macro.setRecordingBlocked(true);
        if (m_engine.start()) {
            SetWindowTextW(m_hStatusLabel, L"[ RUNNING ] Press F6 to Stop");
            SetWindowTextW(m_hBtnStart, L"STOP (F6)");
            InvalidateRect(m_hBtnStart, nullptr, TRUE);
        } else {
            m_macro.setRecordingBlocked(false);
            MessageBoxW(m_hwndMain, L"Please enable at least one target action item to start!", L"Notice", MB_OK | MB_ICONINFORMATION);
        }
    }
}

void AppUI::onEmergencyStop() {
    m_macro.cancelRecording();
    m_macro.stop();
    if (m_engine.isRunning()) {
        m_engine.stop();
        SetWindowTextW(m_hStatusLabel, L"[ STOPPED ] Panic F7 Pressed");
        SetWindowTextW(m_hBtnStart, L"START (F6)");
        InvalidateRect(m_hBtnStart, nullptr, TRUE);
    }
    m_macro.setRecordingBlocked(false);
}

void AppUI::startPositionPicker(std::function<void(POINT, bool)> onFinished) {
    m_pickerCallback = onFinished;

    // Hide active window(s) so screen underneath is fully visible
    if (m_hDlgCurrent && IsWindow(m_hDlgCurrent)) {
        ShowWindow(m_hDlgCurrent, SW_HIDE);
    }
    if (m_hwndMain && IsWindow(m_hwndMain)) {
        ShowWindow(m_hwndMain, SW_HIDE);
    }

    // Small delay to ensure desktop repaint is completed
    Sleep(50);

    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    m_hPickerOverlay = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"AutoClickerPickerOverlayClass",
        L"PositionPickerOverlay",
        WS_POPUP | WS_VISIBLE,
        vx, vy, vw, vh,
        nullptr, nullptr, m_hInstance, nullptr
    );

    if (m_hPickerOverlay) {
        // Semi-transparent overlay with crosshair
        SetLayeredWindowAttributes(m_hPickerOverlay, 0, 40, LWA_ALPHA);
        SetCapture(m_hPickerOverlay);
        SetFocus(m_hPickerOverlay);
        SetCursor(LoadCursor(nullptr, IDC_CROSS));
        GetCursorPos(&m_pickerCurPos);
        ShowWindow(m_hPickerOverlay, SW_SHOW);
        UpdateWindow(m_hPickerOverlay);
    }
}

void AppUI::onPickPosition() {
    startPositionPicker([this](POINT pt, bool captured) {
        if (!captured) return;

        int sel = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
        if (sel >= 0 && static_cast<size_t>(sel) < m_config.items.size()) {
            m_config.items[sel].coordType = CoordType::FIXED_COORDS;
            m_config.items[sel].x = pt.x;
            m_config.items[sel].y = pt.y;
            updateListView();
            ListView_SetItemState(m_hListView, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

            std::wstring msg = L"Captured Target #" + std::to_wstring(sel + 1) + L" Coords: (" + std::to_wstring(pt.x) + L", " + std::to_wstring(pt.y) + L")";
            SetWindowTextW(m_hStatusLabel, msg.c_str());
        } else {
            ActionItem item;
            item.id = m_config.nextItemId();
            if (item.id == 0) return;
            item.name = "Target (" + std::to_string(pt.x) + "," + std::to_string(pt.y) + ")";
            item.actionType = ActionType::MOUSE_LEFT_CLICK;
            item.coordType = CoordType::FIXED_COORDS;
            item.x = pt.x;
            item.y = pt.y;
            item.enabled = true;
            m_config.items.push_back(item);
            updateListView();

            std::wstring msg = L"Captured Target at (" + std::to_wstring(pt.x) + L", " + std::to_wstring(pt.y) + L")";
            SetWindowTextW(m_hStatusLabel, msg.c_str());
        }

        autoSaveToRegistry();
        if (m_config.soundFeedback) Beep(1500, 50);
    });
}

void AppUI::deleteSelectedItem() {
    int sel = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (sel >= 0 && static_cast<size_t>(sel) < m_config.items.size()) {
        m_config.items.erase(m_config.items.begin() + sel);
        updateListView();
        autoSaveToRegistry();
    }
}

void AppUI::moveSelectedItem(bool up) {
    int sel = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (sel < 0) return;

    if (up && sel > 0) {
        std::swap(m_config.items[sel], m_config.items[sel - 1]);
        updateListView();
        ListView_SetItemState(m_hListView, sel - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        autoSaveToRegistry();
    } else if (!up && sel + 1 < static_cast<int>(m_config.items.size())) {
        std::swap(m_config.items[sel], m_config.items[sel + 1]);
        updateListView();
        ListView_SetItemState(m_hListView, sel + 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        autoSaveToRegistry();
    }
}

void AppUI::saveProfilePrompt() {
    updateConfigFromUI();

    wchar_t filename[MAX_PATH] = L"profile.json";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = L"JSON Profile (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"json";

    if (GetSaveFileNameW(&ofn)) {
        if (ConfigManager::saveToFile(m_config, wideToUtf8(filename))) {
            MessageBoxW(m_hwndMain, L"Profile exported successfully!", L"Success", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(m_hwndMain, L"Failed to export profile.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

void AppUI::loadProfilePrompt() {
    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = L"JSON Profile (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        AppConfig loaded;
        if (ConfigManager::loadFromFile(loaded, wideToUtf8(filename))) {
            m_config = loaded;
            updateUIFromConfig();
            updateListView();
            m_engine.setConfig(m_config);
            autoSaveToRegistry();
            MessageBoxW(m_hwndMain, L"Profile imported and saved to Registry!", L"Success", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(m_hwndMain, L"Failed to import profile.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

void AppUI::loadPreset(int presetIndex) {
    updateConfigFromUI();
    m_config.items.clear();

    switch (presetIndex) {
        case 0: { // Fast Left Clicker
            m_config.mode = ExecutionMode::SYNC;
            m_config.globalCpm = 600;
            m_config.globalIntervalMs = 100;
            ActionItem it;
            it.name = "Fast Mouse Left";
            it.actionType = ActionType::MOUSE_LEFT_CLICK;
            it.coordType = CoordType::FOLLOW_MOUSE;
            it.enabled = true;
            m_config.items.push_back(it);
            break;
        }
        case 1: { // Dual Point Sync
            m_config.mode = ExecutionMode::SYNC;
            m_config.globalCpm = 300;
            m_config.globalIntervalMs = 200;
            ActionItem it1;
            it1.name = "Point A (Top Left)";
            it1.actionType = ActionType::MOUSE_LEFT_CLICK;
            it1.coordType = CoordType::FIXED_COORDS;
            it1.x = 400; it1.y = 300;
            it1.delayAfterMs = 100;
            it1.enabled = true;

            ActionItem it2;
            it2.name = "Point B (Bottom Right)";
            it2.actionType = ActionType::MOUSE_LEFT_CLICK;
            it2.coordType = CoordType::FIXED_COORDS;
            it2.x = 800; it2.y = 600;
            it2.delayAfterMs = 100;
            it2.enabled = true;

            m_config.items.push_back(it1);
            m_config.items.push_back(it2);
            break;
        }
        case 2: { // Key 'E' Spammer
            m_config.mode = ExecutionMode::SYNC;
            m_config.globalCpm = 1200;
            m_config.globalIntervalMs = 50;
            ActionItem it;
            it.name = "Spam Key [E]";
            it.actionType = ActionType::KEY_PRESS;
            it.keyVk = 'E';
            it.keyName = "E";
            it.enabled = true;
            m_config.items.push_back(it);
            break;
        }
        case 3: { // Multi Async Combo
            m_config.mode = ExecutionMode::ASYNC;
            ActionItem it1;
            it1.name = "Async Left Click (Mouse)";
            it1.actionType = ActionType::MOUSE_LEFT_CLICK;
            it1.coordType = CoordType::FOLLOW_MOUSE;
            it1.useCustomRate = true;
            it1.cpm = 600; it1.intervalMs = 100;
            it1.enabled = true;

            ActionItem it2;
            it2.name = "Async Point (500, 500)";
            it2.actionType = ActionType::MOUSE_RIGHT_CLICK;
            it2.coordType = CoordType::FIXED_COORDS;
            it2.x = 500; it2.y = 500;
            it2.restoreMouse = true;
            it2.useCustomRate = true;
            it2.cpm = 120; it2.intervalMs = 500;
            it2.enabled = true;

            ActionItem it3;
            it3.name = "Async Key [Space]";
            it3.actionType = ActionType::KEY_PRESS;
            it3.keyVk = VK_SPACE;
            it3.keyName = "SPACE";
            it3.useCustomRate = true;
            it3.cpm = 300; it3.intervalMs = 200;
            it3.enabled = true;

            m_config.items.push_back(it1);
            m_config.items.push_back(it2);
            m_config.items.push_back(it3);
            break;
        }
    }

    updateUIFromConfig();
    updateListView();
    autoSaveToRegistry();
}

// Open dedicated modal dialog to Add or Edit Target Action Item
void AppUI::openItemDialog(int itemIndex, bool defaultToKey) {
    updateConfigFromUI();

    m_tempItemIndex = itemIndex;
    if (itemIndex >= 0 && static_cast<size_t>(itemIndex) < m_config.items.size()) {
        m_tempItem = m_config.items[itemIndex];
        m_tempIsNew = false;
    } else {
        m_tempItem = ActionItem();
        m_tempItem.id = m_config.nextItemId();
        if (m_tempItem.id == 0) return;
        m_tempItem.enabled = true;
        m_tempIsNew = true;

        if (defaultToKey) {
            m_tempItem.name = "Key Spam (E)";
            m_tempItem.actionType = ActionType::KEY_PRESS;
            m_tempItem.keyVk = 'E';
            m_tempItem.keyName = "E";
        } else {
            m_tempItem.name = "Target #" + std::to_string(m_tempItem.id);
            m_tempItem.actionType = ActionType::MOUSE_LEFT_CLICK;
            m_tempItem.coordType = CoordType::FOLLOW_MOUSE;
        }
    }

    int dlgW = 460;
    int dlgH = 530;
    RECT rcMain;
    GetWindowRect(m_hwndMain, &rcMain);
    int dlgX = rcMain.left + (rcMain.right - rcMain.left - dlgW) / 2;
    int dlgY = rcMain.top + (rcMain.bottom - rcMain.top - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"AutoClickerItemDialogClass",
        m_tempIsNew ? L"Add Target Action Item" : L"Edit Target Action Item",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        dlgX, dlgY, dlgW, dlgH,
        m_hwndMain, nullptr, m_hInstance, nullptr
    );

    if (!hDlg) return;

    m_hDlgCurrent = hDlg;
    applyDarkTheme(hDlg);

    // Target Name
    CreateWindowExW(0, L"STATIC", L"Target Name:", WS_CHILD | WS_VISIBLE, 25, 20, 100, 20, hDlg, nullptr, m_hInstance, nullptr);
    std::wstring wName = utf8ToWide(m_tempItem.name);
    HWND hName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", wName.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 130, 18, 290, 24, hDlg, (HMENU)ID_DLG_EDIT_NAME, m_hInstance, nullptr);

    // Action Type
    CreateWindowExW(0, L"STATIC", L"Action Type:", WS_CHILD | WS_VISIBLE, 25, 55, 100, 20, hDlg, nullptr, m_hInstance, nullptr);
    HWND hType = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 130, 52, 290, 160, hDlg, (HMENU)ID_DLG_COMBO_TYPE, m_hInstance, nullptr);
    SendMessageW(hType, CB_ADDSTRING, 0, (LPARAM)L"Mouse Left Click");
    SendMessageW(hType, CB_ADDSTRING, 0, (LPARAM)L"Mouse Right Click");
    SendMessageW(hType, CB_ADDSTRING, 0, (LPARAM)L"Mouse Middle Click");
    SendMessageW(hType, CB_ADDSTRING, 0, (LPARAM)L"Mouse Double Click");
    SendMessageW(hType, CB_ADDSTRING, 0, (LPARAM)L"Keyboard Key Press");
    SendMessageW(hType, CB_ADDSTRING, 0, (LPARAM)L"Recorded Keys");
    SendMessageW(hType, CB_SETCURSEL, static_cast<int>(m_tempItem.actionType), 0);

    // Mouse Coordinates Group
    CreateWindowExW(0, L"BUTTON", L" Mouse Position Target ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 25, 90, 395, 110, hDlg, nullptr, m_hInstance, nullptr);
    HWND hFollow = CreateWindowExW(0, L"BUTTON", L"Follow Current Mouse", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 40, 115, 170, 20, hDlg, (HMENU)ID_DLG_RADIO_FOLLOW, m_hInstance, nullptr);
    HWND hFixed = CreateWindowExW(0, L"BUTTON", L"Fixed Coords:", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 40, 140, 115, 20, hDlg, (HMENU)ID_DLG_RADIO_FIXED, m_hInstance, nullptr);

    SendMessageW(hFollow, BM_SETCHECK, m_tempItem.coordType == CoordType::FOLLOW_MOUSE ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(hFixed, BM_SETCHECK, m_tempItem.coordType == CoordType::FIXED_COORDS ? BST_CHECKED : BST_UNCHECKED, 0);

    CreateWindowExW(0, L"STATIC", L"X:", WS_CHILD | WS_VISIBLE, 160, 142, 18, 20, hDlg, nullptr, m_hInstance, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(m_tempItem.x).c_str(), WS_CHILD | WS_VISIBLE | ES_NUMBER, 180, 139, 50, 24, hDlg, (HMENU)ID_DLG_EDIT_X, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Y:", WS_CHILD | WS_VISIBLE, 240, 142, 18, 20, hDlg, nullptr, m_hInstance, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(m_tempItem.y).c_str(), WS_CHILD | WS_VISIBLE | ES_NUMBER, 260, 139, 50, 24, hDlg, (HMENU)ID_DLG_EDIT_Y, m_hInstance, nullptr);

    HWND hDlgBtnPick = CreateWindowExW(0, L"BUTTON", L"Pick Pos", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 325, 137, 80, 28, hDlg, (HMENU)ID_DLG_BTN_PICK, m_hInstance, nullptr);
    SetWindowSubclass(hDlgBtnPick, AppUI::CustomButtonProc, 2, 2);

    HWND hRestore = CreateWindowExW(0, L"BUTTON", L"Restore mouse cursor after clicking", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 40, 170, 310, 20, hDlg, (HMENU)ID_DLG_CHK_RESTORE, m_hInstance, nullptr);
    SendMessageW(hRestore, BM_SETCHECK, m_tempItem.restoreMouse ? BST_CHECKED : BST_UNCHECKED, 0);

    // Keyboard Key Group
    CreateWindowExW(0, L"BUTTON", L" Keyboard Key Settings ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 25, 210, 395, 65, hDlg, nullptr, m_hInstance, nullptr);
    CreateWindowExW(0, L"STATIC", L"Key (e.g. E, Space, F1, 1):", WS_CHILD | WS_VISIBLE, 40, 235, 200, 20, hDlg, nullptr, m_hInstance, nullptr);
    std::wstring wKey = utf8ToWide(m_tempItem.keyName);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", wKey.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 245, 232, 160, 24, hDlg, (HMENU)ID_DLG_EDIT_KEY, m_hInstance, nullptr);

    // Custom Rate & Delays
    CreateWindowExW(0, L"BUTTON", L" Custom Rate & Delays ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 25, 285, 395, 100, hDlg, nullptr, m_hInstance, nullptr);
    HWND hCustom = CreateWindowExW(0, L"BUTTON", L"Use Custom CPM (Async):", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 40, 310, 200, 20, hDlg, (HMENU)ID_DLG_CHK_CUSTOM_RATE, m_hInstance, nullptr);
    SendMessageW(hCustom, BM_SETCHECK, m_tempItem.useCustomRate ? BST_CHECKED : BST_UNCHECKED, 0);

    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(m_tempItem.cpm).c_str(), WS_CHILD | WS_VISIBLE | ES_NUMBER, 250, 308, 75, 24, hDlg, (HMENU)ID_DLG_EDIT_CPM, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Delay Before (ms):", WS_CHILD | WS_VISIBLE, 40, 350, 120, 20, hDlg, nullptr, m_hInstance, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(m_tempItem.delayBeforeMs).c_str(), WS_CHILD | WS_VISIBLE | ES_NUMBER, 165, 347, 50, 24, hDlg, (HMENU)ID_DLG_EDIT_DELAY_BEFORE, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Delay After (ms):", WS_CHILD | WS_VISIBLE, 230, 350, 110, 20, hDlg, nullptr, m_hInstance, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(m_tempItem.delayAfterMs).c_str(), WS_CHILD | WS_VISIBLE | ES_NUMBER, 345, 347, 50, 24, hDlg, (HMENU)ID_DLG_EDIT_DELAY_AFTER, m_hInstance, nullptr);

    // Enabled Checkbox
    HWND hEnabled = CreateWindowExW(0, L"BUTTON", L"Active / Enabled (Run this item)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 40, 395, 300, 22, hDlg, (HMENU)ID_DLG_CHK_ENABLED, m_hInstance, nullptr);
    SendMessageW(hEnabled, BM_SETCHECK, m_tempItem.enabled ? BST_CHECKED : BST_UNCHECKED, 0);

    // Save / Cancel Buttons
    HWND hBtnSave = CreateWindowExW(0, L"BUTTON", L"Save Item", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 195, 435, 110, 36, hDlg, (HMENU)ID_DLG_BTN_OK, m_hInstance, nullptr);
    SetWindowSubclass(hBtnSave, AppUI::CustomButtonProc, 1, 1); // Green Save button

    HWND hBtnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 315, 435, 105, 36, hDlg, (HMENU)ID_DLG_BTN_CANCEL, m_hInstance, nullptr);
    SetWindowSubclass(hBtnCancel, AppUI::CustomButtonProc, 3, 3); // Neutral Cancel button

    // Apply fonts to dialog
    EnumChildWindows(hDlg, [](HWND h, LPARAM lp) -> BOOL {
        SendMessageW(h, WM_SETFONT, lp, TRUE);
        return TRUE;
    }, (LPARAM)m_hFontMain);
    SendMessageW(hBtnSave, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
    SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
    updateItemDialogControls(hDlg);

    SetFocus(hName);

    // Enable Modal State
    EnableWindow(m_hwndMain, FALSE);
}

// Dialog Window Procedure
void AppUI::updateItemDialogControls(HWND hwnd) {
    bool recorded = SendMessageW(GetDlgItem(hwnd, ID_DLG_COMBO_TYPE), CB_GETCURSEL, 0, 0) ==
        static_cast<int>(ActionType::RECORDED_KEYS);
    for (int id : {ID_DLG_RADIO_FOLLOW, ID_DLG_RADIO_FIXED, ID_DLG_EDIT_X, ID_DLG_EDIT_Y,
        ID_DLG_BTN_PICK, ID_DLG_CHK_RESTORE, ID_DLG_EDIT_KEY, ID_DLG_CHK_CUSTOM_RATE, ID_DLG_EDIT_CPM})
        EnableWindow(GetDlgItem(hwnd, id), !recorded);
}

LRESULT CALLBACK AppUI::DialogWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (s_instance) {
        return s_instance->handleDialogMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT AppUI::handleDialogMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);

            if (id == ID_DLG_COMBO_TYPE && HIWORD(wParam) == CBN_SELCHANGE) {
                updateItemDialogControls(hwnd);
                return 0;
            }

            if (id == ID_DLG_BTN_PICK) {
                startPositionPicker([hwnd, this](POINT pt, bool captured) {
                    if (captured) {
                        SetDlgItemInt(hwnd, ID_DLG_EDIT_X, pt.x, FALSE);
                        SetDlgItemInt(hwnd, ID_DLG_EDIT_Y, pt.y, FALSE);
                        SendMessageW(GetDlgItem(hwnd, ID_DLG_RADIO_FIXED), BM_SETCHECK, BST_CHECKED, 0);
                        SendMessageW(GetDlgItem(hwnd, ID_DLG_RADIO_FOLLOW), BM_SETCHECK, BST_UNCHECKED, 0);
                        if (m_config.soundFeedback) Beep(1500, 50);
                    }
                });
                return 0;
            }

            if (id == ID_DLG_BTN_OK) {
                wchar_t buf[128];
                GetDlgItemTextW(hwnd, ID_DLG_EDIT_NAME, buf, 128);
                m_tempItem.name = wideToUtf8(buf);
                if (m_tempItem.name.empty()) m_tempItem.name = "Target";

                int typeIdx = (int)SendMessageW(GetDlgItem(hwnd, ID_DLG_COMBO_TYPE), CB_GETCURSEL, 0, 0);
                m_tempItem.actionType = static_cast<ActionType>(typeIdx);
                if (m_tempItem.actionType == ActionType::RECORDED_KEYS && m_tempItem.recording.events.empty()) {
                    MessageBoxW(hwnd, L"Use + Record Keys, then hold F5 to create a recorded item.", L"Record Keys", MB_OK | MB_ICONINFORMATION);
                    return 0;
                }

                m_tempItem.coordType = (SendMessageW(GetDlgItem(hwnd, ID_DLG_RADIO_FOLLOW), BM_GETCHECK, 0, 0) == BST_CHECKED)
                    ? CoordType::FOLLOW_MOUSE : CoordType::FIXED_COORDS;

                GetDlgItemTextW(hwnd, ID_DLG_EDIT_X, buf, 128);
                try { m_tempItem.x = std::stoi(buf); } catch (...) { m_tempItem.x = 0; }

                GetDlgItemTextW(hwnd, ID_DLG_EDIT_Y, buf, 128);
                try { m_tempItem.y = std::stoi(buf); } catch (...) { m_tempItem.y = 0; }

                m_tempItem.restoreMouse = (SendMessageW(GetDlgItem(hwnd, ID_DLG_CHK_RESTORE), BM_GETCHECK, 0, 0) == BST_CHECKED);

                GetDlgItemTextW(hwnd, ID_DLG_EDIT_KEY, buf, 128);
                m_tempItem.keyName = wideToUtf8(buf);
                if (m_tempItem.keyName.empty()) m_tempItem.keyName = "E";
                m_tempItem.keyVk = AppConfig::stringToVk(m_tempItem.keyName);

                m_tempItem.useCustomRate = (SendMessageW(GetDlgItem(hwnd, ID_DLG_CHK_CUSTOM_RATE), BM_GETCHECK, 0, 0) == BST_CHECKED);

                GetDlgItemTextW(hwnd, ID_DLG_EDIT_CPM, buf, 128);
                try { m_tempItem.cpm = ConfigManager::parseUnsigned(wideToUtf8(buf)); } catch (...) { m_tempItem.cpm = 600; }
                m_tempItem.updateIntervalFromCpm();

                GetDlgItemTextW(hwnd, ID_DLG_EDIT_DELAY_BEFORE, buf, 128);
                try { m_tempItem.delayBeforeMs = ConfigManager::parseUnsigned(wideToUtf8(buf)); } catch (...) { m_tempItem.delayBeforeMs = 0; }

                GetDlgItemTextW(hwnd, ID_DLG_EDIT_DELAY_AFTER, buf, 128);
                try { m_tempItem.delayAfterMs = ConfigManager::parseUnsigned(wideToUtf8(buf)); } catch (...) { m_tempItem.delayAfterMs = 0; }

                m_tempItem.enabled = (SendMessageW(GetDlgItem(hwnd, ID_DLG_CHK_ENABLED), BM_GETCHECK, 0, 0) == BST_CHECKED);

                // Save item into config
                if (m_tempIsNew) {
                    m_config.items.push_back(m_tempItem);
                } else if (m_tempItemIndex >= 0 && static_cast<size_t>(m_tempItemIndex) < m_config.items.size()) {
                    m_config.items[m_tempItemIndex] = m_tempItem;
                }

                updateListView();
                m_engine.setConfig(m_config);
                autoSaveToRegistry();

                EnableWindow(m_hwndMain, TRUE);
                SetForegroundWindow(m_hwndMain);
                DestroyWindow(hwnd);
                m_hDlgCurrent = nullptr;
                return 0;
            }

            if (id == ID_DLG_BTN_CANCEL) {
                EnableWindow(m_hwndMain, TRUE);
                SetForegroundWindow(m_hwndMain);
                DestroyWindow(hwnd);
                m_hDlgCurrent = nullptr;
                return 0;
            }
            break;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(15, 23, 42)); // Deep Charcoal text on white
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)m_hBrBg;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(15, 23, 42)); // Deep Charcoal input text
            SetBkColor(hdc, RGB(255, 255, 255)); // White input box
            return (LRESULT)m_hBrInput;
        }

        case WM_CLOSE: {
            EnableWindow(m_hwndMain, TRUE);
            SetForegroundWindow(m_hwndMain);
            DestroyWindow(hwnd);
            m_hDlgCurrent = nullptr;
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Main Window Procedure
LRESULT CALLBACK AppUI::MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (s_instance) {
        return s_instance->handleMainMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT AppUI::handleMainMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case KeyboardMacro::WM_RECORDED: {
            bool saved = true;
            for (const auto& recording : m_macro.takeRecordings()) saved = addRecordedItem(recording) && saved;
            m_macro.save(KeyboardMacro::recordingPath());
            SetWindowTextW(m_hRecordHint, saved ? L"Added recorded item | F5: add another" :
                L"Save failed; export profile to keep items.");
            return 0;
        }
        case KeyboardMacro::WM_PLAYBACK_ERROR:
            SetWindowTextW(m_hRecordHint, L"Replay blocked by Windows; check the target app's permissions.");
            return 0;
        case KeyboardMacro::WM_RECORDING_LIMIT:
            SetWindowTextW(m_hRecordHint, wParam ? L"Recording queue full; release F5 and retry." :
                L"Recording stopped at the event limit.");
            return 0;
        case WM_HOTKEY: {
            HotkeyId id = static_cast<HotkeyId>(wParam);
            if (id == HotkeyId::START_STOP) {
                toggleStartStop();
            } else if (id == HotkeyId::PANIC_STOP) {
                onEmergencyStop();
            } else if (id == HotkeyId::PICK_POS) {
                onPickPosition();
            }
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);

            if (id == ID_BTN_START) {
                toggleStartStop();
            } else if (id == ID_BTN_PICK) {
                onPickPosition();
            } else if (id == ID_BTN_ADD_CLICK) {
                openItemDialog(-1, false);
            } else if (id == ID_BTN_ADD_KEY) {
                openItemDialog(-1, true);
            } else if (id == ID_BTN_ADD_RECORDING) {
                onEmergencyStop();
                m_config.mode = ExecutionMode::RECORDED_KEYS;
                updateUIFromConfig();
                autoSaveToRegistry();
                SetWindowTextW(m_hRecordHint, L"Hold F5 to record a new action item.");
            } else if (id == ID_BTN_EDIT_ITEM) {
                int sel = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
                if (sel >= 0) openItemDialog(sel, false);
            } else if (id == ID_BTN_DEL_ITEM) {
                deleteSelectedItem();
            } else if (id == ID_BTN_MOVE_UP) {
                moveSelectedItem(true);
            } else if (id == ID_BTN_MOVE_DOWN) {
                moveSelectedItem(false);
            } else if (id == ID_BTN_SAVE_PROFILE) {
                saveProfilePrompt();
            } else if (id == ID_BTN_LOAD_PROFILE) {
                loadProfilePrompt();
            } else if (id == ID_COMBO_PRESETS && code == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(m_hPresetCombo, CB_GETCURSEL, 0, 0);
                loadPreset(sel);
            } else if (id == ID_RADIO_SYNC || id == ID_RADIO_ASYNC || id == ID_RADIO_RECORDED || id == ID_CHK_SOUND) {
                updateConfigFromUI();
            } else if (id == ID_EDIT_CPM && code == EN_KILLFOCUS) {
                wchar_t buf[32];
                GetWindowTextW(m_hEditCpm, buf, 32);
                try {
                    uint32_t cpm = ConfigManager::parseUnsigned(wideToUtf8(buf));
                    if (cpm > 0) {
                        uint32_t interval = static_cast<uint32_t>((60000.0 / cpm) + 0.5);
                        if (interval == 0) interval = 1;
                        SetWindowTextW(m_hEditInterval, std::to_wstring(interval).c_str());
                    }
                } catch (...) {}
                updateConfigFromUI();
            } else if (id == ID_EDIT_INTERVAL && code == EN_KILLFOCUS) {
                wchar_t buf[32];
                GetWindowTextW(m_hEditInterval, buf, 32);
                try {
                    uint32_t interval = ConfigManager::parseUnsigned(wideToUtf8(buf));
                    if (interval > 0) {
                        uint32_t cpm = static_cast<uint32_t>((60000.0 / interval) + 0.5);
                        SetWindowTextW(m_hEditCpm, std::to_wstring(cpm).c_str());
                    }
                } catch (...) {}
                updateConfigFromUI();
            } else if ((id == ID_EDIT_JITTER || id == ID_EDIT_REPEAT) && code == EN_KILLFOCUS) {
                updateConfigFromUI();
            }
            break;
        }

        case WM_NOTIFY: {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->idFrom == ID_LISTVIEW_ITEMS) {
                if (pnmh->code == NM_DBLCLK) {
                    LPNMITEMACTIVATE pia = (LPNMITEMACTIVATE)lParam;
                    if (pia->iItem >= 0) {
                        openItemDialog(pia->iItem, false);
                    }
                } else if (pnmh->code == LVN_ITEMCHANGED) {
                    if (!m_isUpdatingListView) {
                        NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
                        if ((pnmv->uChanged & LVIF_STATE) && (pnmv->uNewState & LVIS_STATEIMAGEMASK) != (pnmv->uOldState & LVIS_STATEIMAGEMASK)) {
                            UINT oldCheck = (pnmv->uOldState & LVIS_STATEIMAGEMASK) >> 12;
                            UINT newCheck = (pnmv->uNewState & LVIS_STATEIMAGEMASK) >> 12;
                            if (oldCheck != newCheck && oldCheck != 0 && newCheck != 0) {
                                int itemIdx = pnmv->iItem;
                                if (itemIdx >= 0 && static_cast<size_t>(itemIdx) < m_config.items.size()) {
                                    m_config.items[itemIdx].enabled = (newCheck == 2);
                                    m_engine.setConfig(m_config);
                                    autoSaveToRegistry();
                                }
                            }
                        }
                    }
                }
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == ID_TIMER_UPDATE) {
                uint64_t count = m_engine.getTotalCount();
                std::wstring countStr = L"Actions: " + std::to_wstring(count);
                SetWindowTextW(m_hCountLabel, countStr.c_str());

                bool isRunning = this->isRunning();
                m_macro.setRecordingBlocked(isRunning);
                if (m_engine.inputFailed()) SetWindowTextW(m_hRecordHint, L"Replay failed; check target permissions.");
                if (m_macro.isRecording()) SetWindowTextW(m_hStatusLabel, L"[ RECORDING ] Release F5");
                else if (m_recordedMode && !isRunning) SetWindowTextW(m_hStatusLabel, L"[ READY ] Hold F5 / Replay F6");
                wchar_t curText[64];
                GetWindowTextW(m_hBtnStart, curText, 64);
                if (isRunning && std::wstring(curText).find(L"START") != std::wstring::npos) {
                    SetWindowTextW(m_hStatusLabel, L"[ RUNNING ] Press F6 to Stop");
                    SetWindowTextW(m_hBtnStart, L"STOP (F6)");
                    InvalidateRect(m_hBtnStart, nullptr, TRUE);
                    InvalidateRect(m_hStatusLabel, nullptr, TRUE);
                } else if (!isRunning && std::wstring(curText).find(L"STOP") != std::wstring::npos) {
                    SetWindowTextW(m_hStatusLabel, L"[ STOPPED ] Press F6 to Start");
                    SetWindowTextW(m_hBtnStart, L"START (F6)");
                    InvalidateRect(m_hBtnStart, nullptr, TRUE);
                    InvalidateRect(m_hStatusLabel, nullptr, TRUE);
                }
            }
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            HWND hCtl = (HWND)lParam;

            if (hCtl == m_hStatusLabel) {
                bool running = isRunning() || m_macro.isRecording();
                SetTextColor(hdc, running ? RGB(22, 163, 74) : RGB(220, 38, 38)); // Vibrant Green / Red
            } else {
                SetTextColor(hdc, RGB(15, 23, 42)); // High contrast dark charcoal text
            }
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)m_hBrBg;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(15, 23, 42)); // Dark input text
            SetBkColor(hdc, RGB(255, 255, 255)); // White input background
            return (LRESULT)m_hBrInput;
        }

        case WM_DESTROY: {
            KillTimer(hwnd, ID_TIMER_UPDATE);
            m_macro.disable();
            m_hotkeys.unregister();
            m_engine.stop();
            autoSaveToRegistry();
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int AppUI::run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK AppUI::PickerOverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (s_instance) {
        return s_instance->handlePickerMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT AppUI::handlePickerMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SETCURSOR: {
            SetCursor(LoadCursor(nullptr, IDC_CROSS));
            return TRUE;
        }

        case WM_MOUSEMOVE: {
            GetCursorPos(&m_pickerCurPos);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);

            int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
            int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
            int localX = m_pickerCurPos.x - vx;
            int localY = m_pickerCurPos.y - vy;

            // Draw Red Crosshair guidelines across screen
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(239, 68, 68));
            HGDIOBJ oldPen = SelectObject(hdc, hPen);

            MoveToEx(hdc, rc.left, localY, nullptr);
            LineTo(hdc, rc.right, localY);

            MoveToEx(hdc, localX, rc.top, nullptr);
            LineTo(hdc, localX, rc.bottom);

            SelectObject(hdc, oldPen);
            DeleteObject(hPen);

            // Draw floating coordinate badge near cursor
            wchar_t badgeText[128];
            swprintf_s(badgeText, L"X: %d, Y: %d\n[Click: Capture | ESC: Cancel]", m_pickerCurPos.x, m_pickerCurPos.y);

            RECT badgeRc;
            badgeRc.left = localX + 18;
            badgeRc.top = localY + 18;
            badgeRc.right = badgeRc.left + 240;
            badgeRc.bottom = badgeRc.top + 48;

            if (badgeRc.right > rc.right - 10) {
                badgeRc.left = localX - 250;
                badgeRc.right = localX - 10;
            }
            if (badgeRc.bottom > rc.bottom - 10) {
                badgeRc.top = localY - 58;
                badgeRc.bottom = localY - 10;
            }

            HBRUSH hBadgeBr = CreateSolidBrush(RGB(15, 23, 42));
            HPEN hBadgePen = CreatePen(PS_SOLID, 1, RGB(37, 99, 235));
            SelectObject(hdc, hBadgeBr);
            SelectObject(hdc, hBadgePen);

            RoundRect(hdc, badgeRc.left, badgeRc.top, badgeRc.right, badgeRc.bottom, 6, 6);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            HGDIOBJ oldFont = SelectObject(hdc, m_hFontPicker ? m_hFontPicker : m_hFontMain);

            RECT textRc = badgeRc;
            textRc.left += 10;
            textRc.top += 5;
            DrawTextW(hdc, badgeText, -1, &textRc, DT_LEFT | DT_TOP);

            SelectObject(hdc, oldFont);
            DeleteObject(hBadgeBr);
            DeleteObject(hBadgePen);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP: {
            if (msg == WM_LBUTTONUP) {
                ReleaseCapture();
                POINT pt;
                GetCursorPos(&pt);

                DestroyWindow(hwnd);
                m_hPickerOverlay = nullptr;

                // Restore hidden windows
                if (m_hDlgCurrent && IsWindow(m_hDlgCurrent)) {
                    ShowWindow(m_hDlgCurrent, SW_SHOW);
                    SetForegroundWindow(m_hDlgCurrent);
                }
                if (m_hwndMain && IsWindow(m_hwndMain)) {
                    ShowWindow(m_hwndMain, SW_SHOW);
                    if (!m_hDlgCurrent || !IsWindow(m_hDlgCurrent)) {
                        SetForegroundWindow(m_hwndMain);
                    }
                }

                if (m_pickerCallback) {
                    auto cb = m_pickerCallback;
                    m_pickerCallback = nullptr;
                    cb(pt, true);
                }
            }
            return 0;
        }

        case WM_RBUTTONDOWN:
        case WM_KEYDOWN: {
            if (msg == WM_RBUTTONDOWN || (msg == WM_KEYDOWN && wParam == VK_ESCAPE)) {
                ReleaseCapture();
                POINT pt{0, 0};

                DestroyWindow(hwnd);
                m_hPickerOverlay = nullptr;

                // Restore hidden windows
                if (m_hDlgCurrent && IsWindow(m_hDlgCurrent)) {
                    ShowWindow(m_hDlgCurrent, SW_SHOW);
                    SetForegroundWindow(m_hDlgCurrent);
                }
                if (m_hwndMain && IsWindow(m_hwndMain)) {
                    ShowWindow(m_hwndMain, SW_SHOW);
                    if (!m_hDlgCurrent || !IsWindow(m_hDlgCurrent)) {
                        SetForegroundWindow(m_hwndMain);
                    }
                }

                if (m_pickerCallback) {
                    auto cb = m_pickerCallback;
                    m_pickerCallback = nullptr;
                    cb(pt, false);
                }
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
