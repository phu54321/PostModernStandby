#include <Windows.h>
#include <Shlobj.h>

#include <cstdio>
#include <string>
#include <fstream>
#include <ctime>
#include <iomanip>

std::wstring getLogPath() {
    static bool initialized = false;
    static std::wstring path;

    if (!initialized) {
        WCHAR dir[MAX_PATH] = {0};
        SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, dir);
        path = std::wstring(dir) + L"\\delaySleepOnLidClose.log";
    }
    return path;
}

void logA(const char *format, ...) {
    char buf[1024] = "";

    va_list args;
            va_start(args, format);
    vsnprintf(buf, 1024, format, args);
            va_end(args);

    OutputDebugStringA(buf);
    puts(buf);

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::fstream of(getLogPath().c_str(), std::ios_base::app);
    of << std::put_time(&tm, "[%d-%m-%Y %H-%M-%S] ") << buf << std::endl;
}
