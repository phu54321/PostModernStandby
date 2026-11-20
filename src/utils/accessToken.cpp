#include "accessToken.h"

#include <shellapi.h>

// Code from https://m.blog.naver.com/stop2y/221130663783

DWORD EnableNTPrivilege(LPCTSTR szPrivilege, DWORD dwState) {
    DWORD dwRtn = 0;
    HANDLE hToken;
    LUID luid;
    TOKEN_PRIVILEGES TP;

    if (OpenProcessToken(
            GetCurrentProcess(),
            TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, // NOLINT(hicpp-signed-bitwise)
            &hToken)) {
        if (LookupPrivilegeValue(nullptr, szPrivilege, &luid)) {

            TP.Privileges[0].Attributes = dwState;
            TP.PrivilegeCount = 1;
            TP.Privileges[0].Luid = luid;

            if (AdjustTokenPrivileges(hToken, FALSE, &TP, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr))
                dwRtn = TP.Privileges[0].Attributes;
        }

        CloseHandle(hToken);
    }

    return dwRtn;
}

bool isElevated() {
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    TOKEN_ELEVATION elevation = {};
    DWORD size = 0;
    bool elevated = GetTokenInformation(token, TokenElevation, &elevation,
                                        sizeof(elevation), &size) &&
                    elevation.TokenIsElevated;
    CloseHandle(token);
    return elevated;
}

bool relaunchElevated() {
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return false;

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}
