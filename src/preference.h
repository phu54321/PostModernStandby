#pragma once

#define WIN32_LEAN_AND_MEAN

#include <memory>
#include <set>
#include <string>
#include <windows.h>
#include "lib/json_fwd.hpp"
#include "getMonitorName.h"

using nlohmann::json;
using tstring = std::basic_string<TCHAR>;

struct preference {
    int lid_close_sleep_delay_time = 30;
    bool allow_clamshell_on_battery = false;
    int lid_close_alert_volume = 100;
    std::set<std::string> sleep_preventing_apps = {
            "caffeine.exe",
            "caffeine32.exe",
            "caffeine64.exe"};
    std::string main_monitor_name = getMonitorNameIfOnlyOne();
};

tstring getPrefFilePath();

preference load_pref();

json dump_pref(const preference &props);

void save_pref(const preference &props);
