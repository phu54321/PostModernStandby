cmake_minimum_required(VERSION 3.5.0)
project(PostModernStandby VERSION 0.1.0)

add_executable(PostModernStandby WIN32
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

target_link_libraries(PostModernStandby powrprof winmm runtimeobject)
set_property(TARGET PostModernStandby PROPERTY CXX_STANDARD 17)
