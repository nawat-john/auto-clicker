#include <windows.h>
#include "app_ui.hpp"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;

    // Single instance mutex to prevent duplicate hotkey hooks
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"AutoClickerKeySpammerSingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existingWnd = FindWindowW(L"AutoClickerMainWindowClass", nullptr);
        if (existingWnd) {
            SetForegroundWindow(existingWnd);
            ShowWindow(existingWnd, SW_RESTORE);
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    AppUI app(hInstance);
    if (!app.create(nCmdShow)) {
        MessageBoxW(NULL, L"Failed to initialize application window.", L"Fatal Error", MB_OK | MB_ICONERROR);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    int ret = app.run();
    if (hMutex) CloseHandle(hMutex);
    return ret;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
