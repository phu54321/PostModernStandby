#include <string>
#include <Windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <stdexcept>
#include <fstream>

#include "preference.h"
#include "json.hpp"
#include "debugLog.h"

#pragma comment(lib,"shlwapi.lib")

static std::string getPrefFolder() {
	TCHAR szPath[MAX_PATH];
	if (SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath) == 0)
	{
		PathAppend(szPath, TEXT("\\trgksoft\\delaySleepOnLidClose\\"));
		return szPath;
	}
	else {
		throw new std::runtime_error("Cannot get preference folder");
	}
}

std::string getPrefFilePath() {
	auto prefFolder = getPrefFolder();
	{
		auto ret = SHCreateDirectoryEx(NULL, prefFolder.c_str(), NULL);
		if (ret != 0 && ret != ERROR_ALREADY_EXISTS) {
			throw new std::runtime_error("Cannot create preference folder.");
		}
	}

	auto prefFilePath = prefFolder + "preference.json";
	return prefFilePath;
}

static pref getDefaultPref() {
	pref p = { 0 };
	p.try_s0ix_sleep_first = false;
	return p;
}


pref load_pref() {
	std::ifstream ifs(getPrefFilePath());
	auto pref = getDefaultPref();
	if (ifs.fail()) return pref;

#define json_get_pref_required(pref_name) \
	pref.pref_name = data[#pref_name];  /* Error if no such pref */

#define json_get_pref_optional(pref_name) \
	if (data.contains(#pref_name)) { /* Ignore if no such pref */ \
		pref.pref_name = data[#pref_name]; \
	}

	try {
		json data = json::parse(ifs);
		json_get_pref_optional(try_s0ix_sleep_first);
		logA("Loaded preference: %s", dump_pref(pref).dump(2).c_str());
		return pref;
	}
	catch (json::exception e) {
		MessageBoxA(nullptr, e.what(), "delayLidCloseSleep: preference parse error", MB_OK);
		return getDefaultPref();
	}
}

json dump_pref(const pref& props) {
	return json {
		{ "try_s0ix_sleep_first", props.try_s0ix_sleep_first }
	};
}

void save_pref(const pref& props) {
	std::ofstream ofs(getPrefFilePath());
	ofs << dump_pref(props).dump(2);
}
