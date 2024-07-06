#pragma once

#define WIN32_LEAN_AND_MEAN

#include <memory>
#include <set>
#include <string>
#include <windows.h>
#include "lib/json_fwd.hpp"

using nlohmann::json;
using tstring = std::basic_string<TCHAR>;

struct preference
{
    preference();

    int lid_close_alert_volume;
	std::set<std::string> sleep_preventing_apps;
	std::string main_monitor_name;
};

tstring getPrefFilePath();
preference load_pref();
json dump_pref(const preference &props);
void save_pref(const preference &props);
