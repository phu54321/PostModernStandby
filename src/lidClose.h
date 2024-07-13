#pragma once

#include <windows.h>

void registerPowerNotificationReceiver(HWND hWnd);

bool ProcessLidCloseEvent(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool isLidClosed();

bool isBattery();
