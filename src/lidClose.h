#pragma once

#include <windows.h>

void registerPowerSuspendNotificationReceiver(HWND hWnd);

bool ProcessLidCloseEvent(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool isLidClosed();

bool isBattery();
