cmake_minimum_required(VERSION 3.0.0)
project(delaySleepOnLidClose VERSION 0.1.0)

add_executable(delaySleepOnLidClose WIN32 main.cpp accessToken.cpp debugLog.cpp keepAliveApp.cpp lidClose.cpp)
TARGET_LINK_LIBRARIES(delaySleepOnLidClose powrprof)
