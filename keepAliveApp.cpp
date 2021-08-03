#include <set>
#include <windows.h>
#include <algorithm>
#include <string>
#include <tlhelp32.h>

static std::set<std::wstring> keepAliveApps{
    L"caffeine.exe",
    L"caffeine64.exe",
};

bool checkKeepAliveApps()
{
    HANDLE hSnapShot;
    PROCESSENTRY32W pEntry;
    bool ret = false;

    hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
    pEntry.dwSize = sizeof(pEntry);

    for (BOOL hRes = Process32FirstW(hSnapShot, &pEntry); hRes; hRes = Process32NextW(hSnapShot, &pEntry))
    {
        std::wstring pn = pEntry.szExeFile;
        std::transform(pn.begin(), pn.end(), pn.begin(), towlower);
        if (keepAliveApps.find(pn) != keepAliveApps.end())
        {
            ret = true;
            break;
        }
    }
    CloseHandle(hSnapShot);
    return ret;
}
