cmake_minimum_required(VERSION 3.5.0)
project(delaySleepOnLidClose VERSION 0.1.0)

add_executable(delaySleepOnLidClose WIN32
		src/main.cpp
		src/accessToken.cpp
		src/debugLog.cpp
		src/keepAliveApp.cpp
		src/lidClose.cpp
		src/preference.cpp
		src/preference.h
		src/getMonitorName.cpp
		src/res/resource.rc

		src/utils/volumeRAII.cpp
		src/utils/WASAPIUtils.cpp
		src/timerLoop.cpp)

TARGET_LINK_LIBRARIES(delaySleepOnLidClose powrprof winmm)
