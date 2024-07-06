#include <string>
#include <Windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <stdexcept>
#include <fstream>

#include "preference.h"
#include "lib/json.hpp"
#include "debugLog.h"
#include "getMonitorName.h"

#pragma comment(lib, "shlwapi.lib")

static tstring getPrefFolder()
{
	TCHAR szPath[MAX_PATH];
	if (SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath) == 0)
	{
		PathAppend(szPath, TEXT("\\trgksoft\\delaySleepOnLidClose\\"));
		return szPath;
	}
	else
	{
		throw new std::runtime_error("Cannot get preference folder");
	}
}

tstring getPrefFilePath()
{
	auto prefFolder = getPrefFolder();
	{
		auto ret = SHCreateDirectoryEx(NULL, prefFolder.c_str(), NULL);
		if (ret != 0 && ret != ERROR_ALREADY_EXISTS)
		{
			throw new std::runtime_error("Cannot create preference folder.");
		}
	}

	auto prefFilePath = prefFolder + TEXT("preference.json");
	return prefFilePath;
}

preference::preference() {
	lid_close_alert_volume = 100;
	sleep_preventing_apps = {
			"caffeine.exe",
			"caffeine32.exe",
			"caffeine64.exe"};
	main_monitor_name = getMonitorNameIfOnlyOne();
}

preference load_pref()
{
	std::ifstream ifs(getPrefFilePath());
	preference pref;
	if (ifs.fail())
		return pref;

#define json_get_pref_required(pref_name) \
	pref.pref_name = data[#pref_name]; /* Error if no such pref */

#define json_get_pref_optional(pref_name) \
	if (data.contains(#pref_name))          \
	{ /* Ignore if no such pref */          \
		pref.pref_name = data[#pref_name];    \
	}

	try
	{
		json data = json::parse(ifs);
		json_get_pref_optional(sleep_preventing_apps);
		json_get_pref_optional(lid_close_alert_volume);
		json_get_pref_optional(main_monitor_name);
		logA("Loaded preference: %s", dump_pref(pref).dump(2).c_str());
		return pref;
	}
	catch (json::exception e)
	{
		MessageBoxA(nullptr, e.what(), "delaySleepOnLidClose: preference parse error", MB_OK);
        preference pref;
		return pref;
	}
}

json dump_pref(const preference &props)
{
	return json{
			{"lid_close_alert_volume", props.lid_close_alert_volume},
			{"sleep_preventing_apps", props.sleep_preventing_apps},
			{"main_monitor_name", props.main_monitor_name},
	};
}

void save_pref(const preference &props)
{
	std::ofstream ofs(getPrefFilePath());
	ofs << dump_pref(props).dump(2);
}
