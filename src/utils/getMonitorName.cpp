#include <Windows.h>
#include <string>
#include <vector>

#include "debugLog.h"

static BOOL CALLBACK MonitorEnumProc(HMONITOR hmonitor, HDC hdc, LPRECT lprect, LPARAM lparam) {
    auto monitors = reinterpret_cast<std::vector<HMONITOR> *>(lparam);
    monitors->push_back(hmonitor);
    return TRUE;
}

std::vector<std::string> getMonitorNames() {
    std::vector<HMONITOR> monitors;
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));
    std::vector<std::string> monitorNames;
    for (auto &monitor: monitors) {
        MONITORINFOEX info;
        info.cbSize = sizeof(info);
        GetMonitorInfo(monitors[0], &info);
        monitorNames.emplace_back(info.szDevice);
    }
    return monitorNames;
}

std::string getMonitorNameIfOnlyOne() {
    auto monitorNames = getMonitorNames();
    if (monitorNames.size() == 1) return monitorNames[0];
    return "";
}
