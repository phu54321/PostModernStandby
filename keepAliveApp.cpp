#include <set>
#include <windows.h>
#include <algorithm>
#include <string>
#include <tlhelp32.h>

#include "keepAliveApp.hpp"

// std::string to std::wstring converter
// see https://json.nlohmann.me/home/faq/#wide-string-handling
#include <codecvt>
#include <locale>

static std::string to_utf8(const std::wstring& wide_string)
{
    static std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
    return utf8_conv.to_bytes(wide_string);
}

//

bool checkKeepAliveApps(const pref& pref)
{
    HANDLE hSnapShot;
    PROCESSENTRY32W pEntry;
    bool ret = false;

    const auto& keepAliveApps = pref.sleep_preventing_apps;

    hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
    pEntry.dwSize = sizeof(pEntry);

    for (BOOL hRes = Process32FirstW(hSnapShot, &pEntry); hRes; hRes = Process32NextW(hSnapShot, &pEntry))
    {
        std::string pn = to_utf8(pEntry.szExeFile);
        std::transform(pn.begin(), pn.end(), pn.begin(), tolower);
        if (keepAliveApps.find(pn) != keepAliveApps.end())
        {
            ret = true;
            break;
        }
    }
    CloseHandle(hSnapShot);
    return ret;
}
