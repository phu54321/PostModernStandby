// Copyright (c) 2023 Park Hyunwoo
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <map>
#include <fstream>
#include <stdexcept>

#include <shlobj.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.System.h>

#include "debugLog.h"
#include "radio.h"
#include "../preference.h"
#include "../lib/json.hpp"

using namespace winrt;
using namespace winrt::Windows::Devices::Radios;
template <typename T>
using IAsyncOperation = winrt::Windows::Foundation::IAsyncOperation<T>;

std::map<hstring, RadioState> oldBluetoothStateMap;
static bool g_isRadioTurnedOff = false;

static tstring getRadioStateFilePath() {
    auto folder = getPrivateFolder();
    auto ret = SHCreateDirectoryEx(NULL, folder.c_str(), NULL);
    if (ret != 0 && ret != ERROR_ALREADY_EXISTS) {
        throw new std::runtime_error("Cannot create radio state folder.");
    }
    return folder + TEXT("lastSavedRadioState.json");
}

static bool backupRadioStates() {
    nlohmann::json data = nlohmann::json::object();
    for (const auto &[name, state]: oldBluetoothStateMap) {
        data[to_string(name)] = static_cast<int>(state);
    }

    std::ofstream ofs(getRadioStateFilePath(), std::ios::trunc);
    if (ofs.fail()) return false;
    ofs << data.dump(2);
    ofs.flush();
    return !ofs.fail();
}


bool failsafeRestoreRadioState() {
    auto path = getRadioStateFilePath();
    if (GetFileAttributes(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    std::ifstream ifs(path);
    nlohmann::json data;
    try {
        data = nlohmann::json::parse(ifs);
    } catch (const nlohmann::json::exception &e) {
        MessageBoxA(NULL,
            e.what(),
            "Cannot restore saved Bluetooth state",
            MB_OK | MB_ICONWARNING);

        DeleteFile(path.c_str());
        return false;
    }

    // Windows MAY initialize radio enumeration after the application starts
    // but I guess that's too edge case to fix right now.
    // Just check against current list of available radios on failsafe restore.
    std::vector<IAsyncOperation<RadioAccessStatus>> ops;
    for (Radio radio: Radio::GetRadiosAsync().get()) {
        if (radio.Kind() != RadioKind::Bluetooth) continue;

        auto name = to_string(radio.Name());
        if (!data.contains(name)) continue;

        auto savedState = static_cast<RadioState>(data[name].get<int>());
        ops.push_back(radio.SetStateAsync(savedState));
        logA(" - Failsafe restoring %s to %d", name.c_str(), static_cast<int>(savedState));
    }

    for (auto &&op: ops) {
        op.get();
    }

    DeleteFile(path.c_str());
    return true;
}

void removeBackupRadioState() {
    auto path = getRadioStateFilePath();

    // we don't care if the file exists or not.
    // it will not exist after this function exits. that's the only thing required.
    DeleteFile(path.c_str());
}

// ------------------------

void turnOffRadio() {
    if (g_isRadioTurnedOff) return;

    std::vector<Radio> turnOffList;
    auto radioList = Radio::GetRadiosAsync().get();
    for (Radio radio: radioList) {
        if (radio.Kind() == RadioKind::Bluetooth) {
            auto it = oldBluetoothStateMap.find(radio.Name());
            if (it == oldBluetoothStateMap.end()) {
                auto oldState = radio.State();
                oldBluetoothStateMap[radio.Name()] = oldState;
                turnOffList.push_back(radio);
            }
        }
    }

    if (!backupRadioStates()) {
        MessageBoxW(
            nullptr,
             L"Cannot backup radio states",
             L"PostModernStandby Error",
             MB_OK | MB_ICONWARNING);
    }

    // wait all SetStateAsync to complete
    std::vector<IAsyncOperation<RadioAccessStatus>> ops;
    for (Radio& radio: turnOffList) {
        auto oldState = oldBluetoothStateMap[radio.Name()];
        ops.push_back(radio.SetStateAsync(RadioState::Off));
        logA(" - %s: %d -> Off(2)", to_string(radio.Name()).c_str(), static_cast<int>(oldState));
    }

    for (auto &&op: ops) {
        op.get();
    }

    g_isRadioTurnedOff = true;
}

void resumeRadio() {
    if (!g_isRadioTurnedOff) return;

    std::vector<IAsyncOperation<RadioAccessStatus>> ops;
    for (Radio radio: Radio::GetRadiosAsync().get()) {
        if (radio.Kind() == RadioKind::Bluetooth) {
            auto it = oldBluetoothStateMap.find(radio.Name());
            if (it != oldBluetoothStateMap.end()) {
                auto newState = it->second;
                ops.push_back(radio.SetStateAsync(newState));
                oldBluetoothStateMap.erase(it);
                logA(" - %s: Off(2) -> %d", to_string(radio.Name()).c_str(), static_cast<int>(newState));
            }
        }
    }
    // wait all SetStateAsync to complete
    for (auto &&op: ops) {
        op.get();
    }
    removeBackupRadioState();
    g_isRadioTurnedOff = false;
}
