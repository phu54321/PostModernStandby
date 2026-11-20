#include <Windows.h>
#include <Shlobj.h>

#include <cstdio>
#include <string>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <filesystem>

static std::wstring getLogPath() {
    static bool initialized = false;
    static std::wstring path;

    if (!initialized) {
        WCHAR dir[MAX_PATH] = {0};
        SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, dir);
        path = std::wstring(dir) + L"\\PostModernStandby.log";
    }
    return path;
}

static const char *logPrefix = "[PostModernStandby] ";
static const int logPrefixLen = strlen(logPrefix);

void logA(const char *format, ...) {
    char buf[1024];
    strncpy(buf, logPrefix, 1023);

    va_list args;
    va_start(args, format);
    vsnprintf(buf + logPrefixLen, 1024 - logPrefixLen, format, args);
    va_end(args);

    OutputDebugStringA(buf);

    auto logPath = getLogPath();
    if (std::filesystem::exists(logPath)) {
        // Millisecond resolution: power transitions are timing sensitive.
        SYSTEMTIME st;
        GetLocalTime(&st);
        char stamp[40];
        snprintf(stamp, sizeof(stamp), "[%02d-%02d-%04d %02d-%02d-%02d.%03d] ",
                 st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute,
                 st.wSecond, st.wMilliseconds);
        std::fstream of(logPath, std::ios_base::app);
        of << stamp << (buf + logPrefixLen) << std::endl;
    }
}
