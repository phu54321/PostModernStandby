#include "accessToken.h"

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
