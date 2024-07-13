#include <windows.h>
#include <powrprof.h>
#include "debugLog.h"

static bool _isLidClosed = false;
static bool _isBattery = false;

bool ProcessLidCloseEvent(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_POWERBROADCAST) {
        if (wParam == PBT_POWERSETTINGCHANGE) {
            auto settings = reinterpret_cast<POWERBROADCAST_SETTING *>(lParam);
            auto powerSetting = settings->PowerSetting;
            auto dataLength = settings->DataLength;
            if (dataLength == 0);
            else if (powerSetting == GUID_ACDC_POWER_SOURCE) {
                _isBattery = (settings->Data[0] == 1);
                logA("isBattery = %d", _isBattery);
            } else if (powerSetting == GUID_LIDSWITCH_STATE_CHANGE) {
                _isLidClosed = (settings->Data[0] == 0);
                logA("isLidClosed = %d", _isLidClosed);
            }
        }
        return true;
    }
    return false;
}

bool isLidClosed() {
    return _isLidClosed;
}

bool isBattery() {
    return _isBattery;
}

void registerPowerNotificationReceiver(HWND hWnd) {
    RegisterPowerSettingNotification(hWnd, &GUID_ACDC_POWER_SOURCE, 0);
    RegisterPowerSettingNotification(hWnd, &GUID_LIDSWITCH_STATE_CHANGE, 0);
}
