#include <Windows.h>
#include <string>
#include <vector>

#include "debugLog.h"

static BOOL CALLBACK MonitorEnumProc(HMONITOR hmonitor, HDC hdc, LPRECT lprect, LPARAM lparam)
{
	auto monitors = reinterpret_cast<std::vector<HMONITOR>*>(lparam);
	monitors->push_back(hmonitor);
	return TRUE;
}


std::string getMonitorNameIfOnlyOne() {
	std::vector<HMONITOR> monitors;
	EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));

	if (monitors.size() == 1) {
		MONITORINFOEX info;
		info.cbSize = sizeof(info);
		GetMonitorInfo(monitors[0], &info);
		return info.szDevice;
	}
	return "";
}
