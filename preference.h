#pragma once

#include <memory>
#include <set>
#include <string>
#include "json_fwd.hpp"

using nlohmann::json;

struct pref {
	bool try_s0ix_sleep_first = false;
	std::set<std::string> sleep_preventing_apps;
	std::string main_monitor_name = "";
};

std::string getPrefFilePath();
pref load_pref();
json dump_pref(const pref& props);
void save_pref(const pref& props);
