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

ULONGLONG currentTime()
{
    return GetTickCount64() / 1000;
}

BOOL shouldSleep()
{
    return isLidClosed() && !checkKeepAliveApps();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ProcessLidCloseEvent(hWnd, msg, wParam, lParam))
        return TRUE;
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

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

    MSG msg = {};
    ULONGLONG sleepT = 0;

    while (msg.message != WM_QUIT)
    {
        while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        logA("[Debug] TimerProc callback");
        if (shouldSleep())
        {
            if (!sleepT)
            {
                logA("Lid close detected. Waiting for 30 seconds.");
                sleepT = currentTime();
            }
            if (sleepT && sleepT + 30 <= currentTime())
            {
                logA("Going to sleep... [2]");
                SetSuspendState(FALSE, TRUE, FALSE);
            }
            // else if (sleepT && sleepT + 30 <= currentTime())
            // {
            //     logA("Going to sleep...");
            //     SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2); // For S0ix supporting laptops.
            // }
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
