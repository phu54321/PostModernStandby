#include <windows.h>
#include <powrprof.h>
#include <tlhelp32.h>
#include <string>
#include <algorithm>
#include <set>
#include "lidClose.h"
#include "keepAliveApp.hpp"
#include "debugLog.h"
#include "accessToken.h"
#include "preference.h"
#include "getMonitorName.h"
#include "resource.h"


#define TRAY_ICON_ID 12567
#define TRAY_NOTIFY (WM_APP + 100)
#define MENU_QUIT_MESSAGE 0x101
#define MENU_PREF_EDIT_MESSAGE 0x102
#define MENU_PREF_RELOAD_MESSAGE 0x103

preference pref = load_pref();

void addTrayIcon(HINSTANCE hInstance, HWND hWnd) {
    NOTIFYICONDATA  nid;
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICON_ID;  // 아이콘 ID
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; // 플래그 설정
    nid.uCallbackMessage = TRAY_NOTIFY; // 콜백메시지 설정
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON)); // 아이콘 로드  
    strncpy(nid.szTip, "delaySleepOnLidClose", 128);
    Shell_NotifyIcon(NIM_ADD, &nid);
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ProcessLidCloseEvent(hWnd, msg, wParam, lParam))
        return TRUE;

    if (msg == TRAY_NOTIFY) {
        if (wParam == TRAY_ICON_ID && lParam == WM_RBUTTONUP) {//등록시 설정한 트레이 아이콘 식별자
            auto hMenu = CreateMenu();
            auto hMenubar = CreateMenu();
            AppendMenu(hMenu, MF_STRING, MENU_PREF_EDIT_MESSAGE, TEXT("Edit preference"));
            AppendMenu(hMenu, MF_STRING, MENU_PREF_RELOAD_MESSAGE, TEXT("Reload preference"));
            AppendMenu(hMenu, MF_STRING, MENU_QUIT_MESSAGE, TEXT("Quit"));
            AppendMenu(hMenubar, MF_POPUP, (UINT_PTR)hMenu, TEXT("Menu"));

            auto hPopupMenu = GetSubMenu(hMenubar, 0);
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
            SetForegroundWindow(hWnd);
            PostMessage(hWnd, WM_NULL, 0, 0);

            DestroyMenu(hMenu);
            DestroyMenu(hMenubar);

            return 0;
        }
    }
    else if (msg == WM_COMMAND) {
        auto wmId = LOWORD(wParam); 
        auto wmEvent = HIWORD(wParam);
        
        if (wmId == MENU_QUIT_MESSAGE) {
            PostQuitMessage(0);
            return 0;
        }

        if (wmId == MENU_PREF_EDIT_MESSAGE) {
            ShellExecute(hWnd, NULL, getPrefFilePath().c_str(), NULL, NULL, SW_SHOWNORMAL);
            return 0;
        }

        if (wmId == MENU_PREF_RELOAD_MESSAGE) {
            pref = load_pref();
            return 0;
        }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

///


ULONGLONG currentTime()
{
    return GetTickCount64() / 1000;
}

BOOL shouldSleep(const preference& pref)
{
    if (isLidClosed()) {
        if (checkKeepAliveApps(pref)) return false;

        // check clamshell mode
        if (!isBattery()) {
            auto currentMonitor = getMonitorNameIfOnlyOne();
            if (!currentMonitor.empty() && currentMonitor != pref.main_monitor_name) return false;
        }
            
        return true;
    }
    return false;
}

///

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
    if (!EnableNTPrivilege(SE_SHUTDOWN_NAME, SE_PRIVILEGE_ENABLED))
    {
        MessageBox(nullptr, TEXT("No suspend permission."), TEXT("Error"), MB_OK);
        return 2;
    }

    auto mutex = CreateMutex(nullptr, TRUE, TEXT("Global\\delaySleepOnLidClose"));
    if (!mutex || GetLastError())
    {
        MessageBox(nullptr, TEXT("Another instance already running."), TEXT("Error"), MB_OK);
        return 0;
    }

    WNDCLASS wndClass;
    memset(&wndClass, 0, sizeof(WNDCLASS));
    wndClass.hInstance = hInstance;
    wndClass.lpszClassName = TEXT("delaySleepOnLidClose");
    wndClass.lpfnWndProc = WndProc;
    RegisterClass(&wndClass);

    HWND hWnd = CreateWindowEx(
        WS_EX_LEFT,
        TEXT("delaySleepOnLidClose"),
        TEXT("delaySleepOnLidClose"),
        0,
        0,
        0,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hWnd)
    {
        MessageBox(nullptr, TEXT("Cannot create window"), TEXT("Error"), MB_OK);
        return 1;
    }

    registerPowerNotificationReceiver(hWnd);
    addTrayIcon(hInstance, hWnd);

    MSG msg = {};
    ULONGLONG sleepT = 0;
    pref = load_pref();
    if (pref.main_monitor_name == "") {
        auto currentMonitor = getMonitorNameIfOnlyOne();
        if (!currentMonitor.empty()) pref.main_monitor_name = currentMonitor;
    }
    save_pref(pref);

    while (1)
    {
        while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
        {   
            if (msg.message == WM_QUIT) return 0;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // logA("[Debug] TimerProc callback");
        if (shouldSleep(pref))
        {
            if (!sleepT)
            {
                logA("Lid close detected. Waiting for 30 seconds.");
                sleepT = currentTime();
            }

            auto suspendSleepTimer = pref.try_s0ix_sleep_first ? 90 : 30;
            auto s0ixSleepTimer = 30;

            if (sleepT && sleepT + suspendSleepTimer <= currentTime())
            {
                logA("Going to sleep... [2]");
                SetSuspendState(FALSE, TRUE, FALSE);
            }
             else if (pref.try_s0ix_sleep_first && sleepT && sleepT + 30 <= currentTime())
             {
                 logA("Going to sleep...");
                 SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2); // For S0ix supporting laptops.
             }
        }
        else
        {
            if (sleepT)
            {
                logA("Lid re-opened. Interrupting timer.");
                sleepT = 0;
            }
        }

        Sleep(1000);
    }
    return 0;
}
