#include <Windows.h>
#include <string>
#include <vector>
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>

#include "debugLog.h"

std::vector<std::string> getMonitorNames() {
    std::vector<std::string> paths;

    // Get all monitor devices
    HDEVINFO devInfo = SetupDiGetClassDevsW(
        &GUID_DEVCLASS_MONITOR,
        NULL,
        NULL,
        DIGCF_PRESENT // Only currently present devices
    );

    if (devInfo == INVALID_HANDLE_VALUE) {
        return paths;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devInfoData); i++) {
        // Get device instance ID
        WCHAR deviceInstanceId[MAX_DEVICE_ID_LEN];
        if (CM_Get_Device_IDW(devInfoData.DevInst, deviceInstanceId,
                              MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS) {
            // Convert to narrow string
            char narrowId[MAX_DEVICE_ID_LEN * 2];
            WideCharToMultiByte(CP_UTF8, 0, deviceInstanceId, -1,
                                narrowId, sizeof(narrowId), NULL, NULL);
            paths.push_back(narrowId);
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return paths;
}

std::string getMonitorNameIfOnlyOne() {
    auto monitorNames = getMonitorNames();
    if (monitorNames.size() == 1) return monitorNames[0];
    return "";
}
