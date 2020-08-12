#include <windows.h>
#include <powrprof.h>
#include "debugLog.h"
#include "accessToken.h"

bool isBattery = false;
bool isLidClosed = false;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_POWERBROADCAST) {
        if (wParam == PBT_POWERSETTINGCHANGE) {
            auto settings = reinterpret_cast<POWERBROADCAST_SETTING *>(lParam);
            auto powerSetting = settings->PowerSetting;
            auto dataLength = settings->DataLength;
            if (dataLength == 0);
            else if (powerSetting == GUID_ACDC_POWER_SOURCE) {
                isBattery = (settings->Data[0] == 1);
                logA("isBattery = %d", isBattery);
            } else if (powerSetting == GUID_LIDSWITCH_STATE_CHANGE) {
                isLidClosed = (settings->Data[0] == 0);
                logA("isLidClosed = %d", isLidClosed);
            }
        }
        return TRUE;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

ULONGLONG currentTime() {
    return GetTickCount64() / 1000;
}

void CALLBACK TimerProc(HWND hWnd, UINT nMsg, UINT_PTR nIDEvent, DWORD dwTime) {
    static bool prevIsLidClosed = false;
    static ULONGLONG sleepT = 0;

    if (isLidClosed) {
        if (!prevIsLidClosed) {
            if (isBattery && !sleepT) {
                logA("Lid close detected while on battery. Waiting for 30 seconds.");
                sleepT = currentTime() + 30;
            }
        } else {
            if (!sleepT && isBattery) {
                logA("Battery detached while lid closed. Waiting 60 seconds.");
                sleepT = currentTime() + 60;
            }
            if (sleepT <= currentTime()) {
                logA("Going to sleep...");
                SetSuspendState(FALSE, TRUE, FALSE);
            }
        }
    } else {
        if (sleepT) {
            logA("Lid re-opened. Interrupting timer.");
            sleepT = 0;
        }
    }

    prevIsLidClosed = isLidClosed;
}

bool checkSuspendPermission();

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
    if (!EnableNTPrivilege(SE_SHUTDOWN_NAME, SE_PRIVILEGE_ENABLED)) {
        MessageBox(nullptr, TEXT("No suspend permission."), TEXT("Error"), MB_OK);
        return 2;
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

    if (!hWnd) {
        MessageBox(nullptr, TEXT("Cannot create window"), TEXT("Error"), MB_OK);
        return 1;
    }

    RegisterPowerSettingNotification(hWnd, &GUID_ACDC_POWER_SOURCE, 0);
    RegisterPowerSettingNotification(hWnd, &GUID_LIDSWITCH_STATE_CHANGE, 0);
    SetTimer(hWnd, 1, 500, TimerProc);

    BOOL bRet;
    MSG msg = {};
    while ((bRet = GetMessage(&msg, nullptr, 0, 0)) != 0) {
        if (bRet == -1) {
            break;
        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return 0;
}
