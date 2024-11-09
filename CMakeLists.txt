cmake_minimum_required(VERSION 3.5.0)
project(delaySleepOnLidClose VERSION 0.1.0)

add_executable(delaySleepOnLidClose WIN32
		src/main.cpp
		src/keepAliveApp.cpp
		src/lidClose.cpp
		src/preference.cpp
		src/preference.h
		src/res/resource.rc

		src/utils/accessToken.cpp
		src/utils/radio.cpp
		src/utils/debugLog.cpp
		src/utils/getMonitorName.cpp
		src/utils/volumeRAII.cpp
		src/utils/WASAPIUtils.cpp
		src/timerLoop.cpp)

target_link_libraries(delaySleepOnLidClose powrprof winmm)
set_property(TARGET delaySleepOnLidClose PROPERTY CXX_STANDARD 17)
