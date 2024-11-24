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

void logA(const char *format, ...) {
    char buf[1024] = "[PostModernStandby] ";
    char prefixLen = 23;

    va_list args;
            va_start(args, format);
    vsnprintf(buf + prefixLen, 1024 - prefixLen, format, args);
            va_end(args);

    OutputDebugStringA(buf);

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::fstream of(getLogPath().c_str(), std::ios_base::app);
    of << std::put_time(&tm, "[%d-%m-%Y %H-%M-%S] ") << (buf + prefixLen) << std::endl;
}
