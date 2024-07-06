#include <cstdio>
#include <windows.h>

void logA(const char *format, ...) {
    char buf[1024] = "[delaySleepOnLidClose] ";

    va_list args;
    va_start(args, format);
    vsnprintf(buf + 23, 1024 - 23, format,  args);
    va_end(args);

    OutputDebugStringA(buf);
    puts(buf);
}

