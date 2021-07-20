#include <windows.h>
#include <powrprof.h>
#include <tlhelp32.h>
#include <string>
#include <algorithm>
#include <set>
#include "debugLog.h"
#include "accessToken.h"

bool isLidClosed = false;

std::set<std::wstring> keepAliveApps{
    L"caffeine.exe",
    L"caffeine64.exe",
};

BOOL checkKeepAliveApps()
{
    HANDLE hSnapShot;
    PROCESSENTRY32W pEntry;
    BOOL ret = FALSE;

    hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
    pEntry.dwSize = sizeof(pEntry);

    for (BOOL hRes = Process32FirstW(hSnapShot, &pEntry); hRes; hRes = Process32NextW(hSnapShot, &pEntry)) {
        std::wstring pn = pEntry.szExeFile;
        std::transform(pn.begin(), pn.end(), pn.begin(), towlower);
        if (keepAliveApps.find(pn) != keepAliveApps.end())
        {
            ret = TRUE;
            break;
        }
    }
    CloseHandle(hSnapShot);
    return ret;
}

BOOL shouldSleep()
{
    return isLidClosed && !checkKeepAliveApps();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_POWERBROADCAST)
    {
        if (wParam == PBT_POWERSETTINGCHANGE)
        {
            auto settings = reinterpret_cast<POWERBROADCAST_SETTING *>(lParam);
            auto powerSetting = settings->PowerSetting;
            auto dataLength = settings->DataLength;
            if (dataLength == 0)
                ;
            else if (powerSetting == GUID_LIDSWITCH_STATE_CHANGE)
            {
                isLidClosed = (settings->Data[0] == 0);
                logA("isLidClosed = %d", isLidClosed);
            }
        }
        return TRUE;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

ULONGLONG currentTime()
{
    return GetTickCount64() / 1000;
}

void CALLBACK TimerProc(HWND hWnd, UINT nMsg, UINT_PTR nIDEvent, DWORD dwTime)
{
    static ULONGLONG sleepT = 0;

    if (shouldSleep())
    {
        if (!sleepT)
        {
            logA("Lid close detected. Waiting for 30 seconds.");
            sleepT = currentTime();
        }
        if (sleepT && sleepT + 90 <= currentTime())
        {
            logA("Going to sleep... [2]");
            SetSuspendState(FALSE, TRUE, FALSE);
        }
        else if (sleepT && sleepT + 30 <= currentTime())
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
}

bool checkSuspendPermission();

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
    if (!EnableNTPrivilege(SE_SHUTDOWN_NAME, SE_PRIVILEGE_ENABLED))
    {
        MessageBox(nullptr, TEXT("No suspend permission."), TEXT("Error"), MB_OK);
        return 2;
    }

    auto mutex = CreateMutex(nullptr, TRUE, "Global\\delaySleepOnLidClose");
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

    RegisterPowerSettingNotification(hWnd, &GUID_ACDC_POWER_SOURCE, 0);
    RegisterPowerSettingNotification(hWnd, &GUID_LIDSWITCH_STATE_CHANGE, 0);
    SetTimer(hWnd, 1, 500, TimerProc);

    BOOL bRet;
    MSG msg = {};
    while ((bRet = GetMessage(&msg, nullptr, 0, 0)) != 0)
    {
        if (bRet == -1)
        {
            break;
        }
        else
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return 0;
}
