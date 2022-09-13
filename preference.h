#pragma once

#include <memory>
#include <string>
#include "json.hpp"

using nlohmann::json;

struct pref {
	bool try_s0ix_sleep_first = false;
};

std::string getPrefFilePath();
pref load_pref();
json dump_pref(const pref& props);
void save_pref(const pref& props);
