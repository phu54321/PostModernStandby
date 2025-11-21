#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <powrprof.h>
#include <tlhelp32.h>
#include <string>
#include <algorithm>
#include <set>
#include <tchar.h>

#include "lidClose.h"
#include "keepAliveApp.hpp"
#include "utils/debugLog.h"
#include "utils/accessToken.h"
#include "utils/getMonitorName.h"
#include "utils/radio.h"
#include "preference.h"
#include "res/resource.h"
#include "timerLoop.h"

#define TRAY_ICON_ID 12567
#define TRAY_NOTIFY (WM_APP + 100)
#define MENU_QUIT_MESSAGE 0x101
#define MENU_PREF_EDIT_MESSAGE 0x102
#define MENU_PREF_RELOAD_MESSAGE 0x103

#define ID_MAIN_TIMER 0x301

preference pref = load_pref();

UINT WM_TASKBARCREATED = -1;

void addTrayIcon(HINSTANCE hInstance, HWND hWnd) {
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = TRAY_NOTIFY;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
    _tcsncpy(nid.szTip, TEXT("PostModernStandby"), 128);
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void deleteTrayIcon(HWND hWnd) {
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICON_ID;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

bool ProcessSuspendResumeNotification(HWND rhWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_POWERBROADCAST) {
        switch (wParam) {
            case PBT_APMSUSPEND: {
                if (pref.disable_bluetooth_on_sleep) {
                    logA("[PBT_APMSUSPEND] Turning bluetooth OFF...");
                    turnOffRadio();
                }
                return true;
            }

            case PBT_APMRESUMESUSPEND: {
                if (pref.disable_bluetooth_on_sleep) {
                    logA("[PBT_APMRESUMESUSPEND] Turning bluetooth ON...");
                    resumeRadio();
                }
                return true;
            }

            case PBT_APMRESUMECRITICAL: {
                if (pref.disable_bluetooth_on_sleep) {
                    logA("[PBT_APMRESUMECRITICAL] Turning bluetooth ON...");
                    resumeRadio();
                }
                return true;
            }

            default:;
        }
    }
    return false;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ProcessLidCloseEvent(hWnd, msg, wParam, lParam))
        return TRUE;

    if (ProcessSuspendResumeNotification(hWnd, msg, wParam, lParam))
        return TRUE;

    if (msg == WM_TASKBARCREATED) {
        auto hInstance = (HINSTANCE) GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
        addTrayIcon(hInstance, hWnd);
        return 0;
    } else if (msg == WM_CREATE) {
        auto hInstance = (HINSTANCE) GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
        registerPowerSuspendNotificationReceiver(hWnd);
        addTrayIcon(hInstance, hWnd);

        SetTimer(hWnd, ID_MAIN_TIMER, 5000, nullptr);
        return 0;
    } else if (msg == WM_DESTROY) {
        deleteTrayIcon(hWnd);
        PostQuitMessage(0);
        return 0;
    } else if (msg == TRAY_NOTIFY) {
        if (wParam == TRAY_ICON_ID && lParam == WM_RBUTTONUP) {
            auto hMenu = CreateMenu();
            AppendMenu(hMenu, MF_STRING, MENU_PREF_EDIT_MESSAGE, TEXT("Edit preference"));
            AppendMenu(hMenu, MF_STRING, MENU_PREF_RELOAD_MESSAGE, TEXT("Reload preference"));
            AppendMenu(hMenu, MF_STRING, MENU_QUIT_MESSAGE, TEXT("Quit"));

            auto hMenubar = CreateMenu();
            AppendMenu(hMenubar, MF_POPUP, (UINT_PTR) hMenu, TEXT("Menu"));

            auto hPopupMenu = GetSubMenu(hMenubar, 0);
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
            SetForegroundWindow(hWnd);
            PostMessage(hWnd, WM_NULL, 0, 0);

            DestroyMenu(hMenu);
            DestroyMenu(hMenubar);

            return 0;
        }
    } else if (msg == WM_COMMAND) {
        auto wmId = LOWORD(wParam);
        auto wmEvent = HIWORD(wParam);

        if (wmId == MENU_QUIT_MESSAGE) {
            DestroyWindow(hWnd);
            return 0;
        }

        if (wmId == MENU_PREF_EDIT_MESSAGE) {
            ShellExecute(hWnd, nullptr, getPrefFilePath().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }

        if (wmId == MENU_PREF_RELOAD_MESSAGE) {
            pref = load_pref();
            return 0;
        }
    } else if (msg == WM_TIMER) {
        if (wParam == ID_MAIN_TIMER) {
            timerLoop(hWnd, pref);
        }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

///

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
    if (!EnableNTPrivilege(SE_SHUTDOWN_NAME, SE_PRIVILEGE_ENABLED)) {
        MessageBox(nullptr, TEXT("No suspend permission."), TEXT("Error"), MB_OK);
        return 2;
    }

    auto mutex = CreateMutex(nullptr, TRUE, TEXT("Global\\PostModernStandby"));
    if (!mutex || GetLastError()) {
        MessageBox(nullptr, TEXT("Another instance already running."), TEXT("Error"), MB_OK);
        return 0;
    }

    winrt::init_apartment();
    CoInitialize(nullptr);

    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASS wndClass;
    memset(&wndClass, 0, sizeof(WNDCLASS));
    wndClass.hInstance = hInstance;
    wndClass.lpszClassName = TEXT("PostModernStandby");
    wndClass.lpfnWndProc = WndProc;
    RegisterClass(&wndClass);

    HWND hWnd = CreateWindowEx(
            WS_EX_LEFT,
            TEXT("PostModernStandby"),
            TEXT("PostModernStandby"),
            0,
            0, 0,
            CW_USEDEFAULT, CW_USEDEFAULT,
            nullptr,
            nullptr,
            hInstance,
            nullptr);

    if (!hWnd) {
        MessageBox(nullptr, TEXT("Cannot create window"), TEXT("Error"), MB_OK);
        return 1;
    }

    MSG msg = {};

    pref = load_pref();
    // rfind(s0, 0) == 0 means string starts with s0.
    if (pref.main_monitor_name.rfind(R"(\\.\)", 0) == 0) {
        MessageBoxA(nullptr, "Old format `main_monitor_name` found. Removing it.", "Info", MB_OK);
        pref.main_monitor_name = "";
    }

    if (pref.main_monitor_name.empty()) {
        auto currentMonitor = getMonitorNameIfOnlyOne();
        if (!currentMonitor.empty()) {
            pref.main_monitor_name = currentMonitor;
            auto msg = "`main_monitor_name` set to current only monitor " + currentMonitor;
            MessageBoxA(nullptr, msg.c_str(), "Info", MB_OK);
        } else {
            MessageBoxA(nullptr, "`main_monitor_name` not set but \n"
                        " - Multiple monitors are connected, or...\n"
                        " - No active monitors are connected,\n"
                        "Lid detection / Clamshell detection will not work correctly."
                        , "Error", MB_OK);
        }
    }
    save_pref(pref);

    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
