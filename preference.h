#pragma once

#include <memory>
#include <set>
#include <string>
#include "json_fwd.hpp"

using nlohmann::json;

struct preference {
	bool try_s0ix_sleep_first = false;
	std::set<std::string> sleep_preventing_apps;
	std::string main_monitor_name = "";
};

std::string getPrefFilePath();
preference load_pref();
json dump_pref(const preference& props);
void save_pref(const preference& props);
