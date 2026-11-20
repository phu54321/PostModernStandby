#include <windows.h>
#include <initguid.h>
#include <devguid.h>
#include <setupapi.h>
#include <cfgmgr32.h>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <string>
#include <vector>

#include "../preference.h"
#include "debugLog.h"
#include "nvidiaGpu.h"

static constexpr DWORD kDeviceWaitMs = 10000;
// NVDisplay.Container can take a long time to stop while still initializing.
static constexpr DWORD kServiceTimeoutMs = 60000;

static const wchar_t *const kNvServices[] = {L"NVDisplay.ContainerLocalSystem",
                                             L"NvContainerLocalSystem"};

static std::atomic<bool> g_managed{false};
static std::atomic<bool> g_sleepIntent{false};
// Bumped on every switch to sleep intent; the worker records the revision
// whose enable it has finished, so the timer loop knows when to sleep.
static std::atomic<unsigned> g_intentRev{0};
static std::atomic<unsigned> g_enabledRev{0};
static HANDLE g_wake = nullptr; // auto-reset: state changed
static HANDLE g_quit = nullptr; // manual-reset: shutting down
static HANDLE g_thread = nullptr;

// ---- Marker file -----------------------------------------------------------
// Lists the adapters this app enabled for a sleep, one instance ID per line.
// It exists only between the enable and the restore, so a crash in between
// can be undone on the next start.

static tstring markerPath() {
    return getPrivateFolder() + TEXT("nvidiaEnabledByApp.txt");
}

static void writeMarker(const std::vector<std::wstring> &ids) {
    std::ofstream ofs(markerPath(), std::ios::trunc);
    // Device instance IDs are plain ASCII.
    for (const auto &id : ids)
        ofs << std::string(id.begin(), id.end()) << '\n';
}

static std::vector<std::wstring> readMarker() {
    std::vector<std::wstring> ids;
    std::ifstream ifs(markerPath());
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (!line.empty())
            ids.emplace_back(line.begin(), line.end());
    }
    return ids;
}

static void clearMarker() { DeleteFile(markerPath().c_str()); }

// ---- Device ----------------------------------------------------------------

// Instance IDs of all present NVIDIA display adapters.
static std::vector<std::wstring> findNvidiaGpus() {
    HDEVINFO set = SetupDiGetClassDevsW(&GUID_DEVCLASS_DISPLAY, nullptr,
                                        nullptr, DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE)
        return {};

    std::vector<std::wstring> found;
    SP_DEVINFO_DATA dev = {};
    dev.cbSize = sizeof(dev);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &dev); ++i) {
        wchar_t id[MAX_DEVICE_ID_LEN];
        if (!SetupDiGetDeviceInstanceIdW(set, &dev, id, MAX_DEVICE_ID_LEN,
                                         nullptr))
            continue;
        if (_wcsnicmp(id, L"PCI\\VEN_10DE", 12) == 0)
            found.push_back(id);
    }
    SetupDiDestroyDeviceInfoList(set);
    return found;
}

static bool isDisabled(const std::wstring &instanceId) {
    DEVINST inst;
    if (CM_Locate_DevNodeW(&inst, const_cast<DEVINSTID_W>(instanceId.c_str()),
                           CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
        return false;
    ULONG status = 0, problem = 0;
    if (CM_Get_DevNode_Status(&status, &problem, inst, 0) != CR_SUCCESS)
        return false;
    return (status & DN_HAS_PROBLEM) && problem == CM_PROB_DISABLED;
}

static bool setDeviceEnabled(const std::wstring &instanceId, bool enable) {
    HDEVINFO set = SetupDiCreateDeviceInfoList(nullptr, nullptr);
    if (set == INVALID_HANDLE_VALUE)
        return false;

    bool ok = false;
    SP_DEVINFO_DATA dev = {};
    dev.cbSize = sizeof(dev);
    if (SetupDiOpenDeviceInfoW(set, instanceId.c_str(), nullptr, 0, &dev)) {
        SP_PROPCHANGE_PARAMS pc = {};
        pc.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        pc.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        pc.StateChange = enable ? DICS_ENABLE : DICS_DISABLE;
        pc.Scope = DICS_FLAG_GLOBAL;
        pc.HwProfile = 0;
        ok = SetupDiSetClassInstallParamsW(set, &dev, &pc.ClassInstallHeader,
                                           sizeof(pc)) &&
             SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, set, &dev);
    }
    if (!ok)
        logA("[nvidia] %s failed: error %lu", enable ? "enable" : "disable",
             GetLastError());
    SetupDiDestroyDeviceInfoList(set);
    return ok;
}

static bool waitForDisabled(const std::wstring &instanceId, bool disabled) {
    const DWORD start = GetTickCount();
    while (isDisabled(instanceId) != disabled) {
        if (GetTickCount() - start >= kDeviceWaitMs)
            return false;
        Sleep(250);
    }
    return true;
}

// ---- Services --------------------------------------------------------------

struct ScHandle {
    SC_HANDLE h;
    explicit ScHandle(SC_HANDLE handle) : h(handle) {}
    ~ScHandle() {
        if (h)
            CloseServiceHandle(h);
    }
    ScHandle(const ScHandle &) = delete;
    ScHandle &operator=(const ScHandle &) = delete;
};

static DWORD serviceState(SC_HANDLE svc) {
    SERVICE_STATUS_PROCESS ss = {};
    DWORD needed = 0;
    if (!QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO,
                              reinterpret_cast<BYTE *>(&ss), sizeof(ss),
                              &needed))
        return 0;
    return ss.dwCurrentState;
}

static bool waitForServiceState(SC_HANDLE svc, DWORD wanted) {
    const DWORD start = GetTickCount();
    for (;;) {
        DWORD state = serviceState(svc);
        if (state == wanted)
            return true;
        if (state == 0 || GetTickCount() - start >= kServiceTimeoutMs)
            return false;
        Sleep(250);
    }
}

static bool serviceDisabled(SC_HANDLE svc) {
    DWORD needed = 0;
    QueryServiceConfigW(svc, nullptr, 0, &needed);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        return false;
    std::vector<BYTE> buf(needed);
    auto *cfg = reinterpret_cast<QUERY_SERVICE_CONFIGW *>(buf.data());
    return QueryServiceConfigW(svc, cfg, needed, &needed) &&
           cfg->dwStartType == SERVICE_DISABLED;
}

// Stops a service after its running dependents; stopped dependents are
// appended (innermost first) so they can be started again afterwards.
static void stopService(SC_HANDLE scm, const std::wstring &name,
                        std::vector<std::wstring> &stoppedDependents) {
    ScHandle svc(OpenServiceW(scm, name.c_str(),
                              SERVICE_STOP | SERVICE_QUERY_STATUS |
                                  SERVICE_ENUMERATE_DEPENDENTS));
    if (!svc.h || serviceState(svc.h) == SERVICE_STOPPED)
        return;

    DWORD needed = 0, count = 0;
    if (!EnumDependentServicesW(svc.h, SERVICE_ACTIVE, nullptr, 0, &needed,
                                &count) &&
        GetLastError() == ERROR_MORE_DATA) {
        std::vector<BYTE> buf(needed);
        auto *deps = reinterpret_cast<ENUM_SERVICE_STATUSW *>(buf.data());
        if (EnumDependentServicesW(svc.h, SERVICE_ACTIVE, deps, needed,
                                   &needed, &count)) {
            for (DWORD i = 0; i < count; ++i) {
                std::wstring dep = deps[i].lpServiceName;
                stopService(scm, dep, stoppedDependents);
                stoppedDependents.push_back(dep);
            }
        }
    }

    SERVICE_STATUS ss = {};
    ControlService(svc.h, SERVICE_CONTROL_STOP, &ss);
    if (!waitForServiceState(svc.h, SERVICE_STOPPED))
        logA("[nvidia] timed out stopping a service");
}

static void startService(SC_HANDLE scm, const std::wstring &name) {
    ScHandle svc(OpenServiceW(
        scm, name.c_str(),
        SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG));
    if (!svc.h || serviceDisabled(svc.h) ||
        serviceState(svc.h) == SERVICE_RUNNING)
        return;
    StartServiceW(svc.h, 0, nullptr);
    if (!waitForServiceState(svc.h, SERVICE_RUNNING))
        logA("[nvidia] timed out starting a service");
}

// Stops the NVIDIA services so they release the GPU. Returns the dependents
// that were stopped, for startNvidiaServices.
static std::vector<std::wstring> stopNvidiaServices() {
    std::vector<std::wstring> dependents;
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm.h) {
        logA("[nvidia] cannot open service manager: error %lu",
             GetLastError());
        return dependents;
    }
    for (const wchar_t *name : kNvServices)
        stopService(scm.h, name, dependents);
    return dependents;
}

static void startNvidiaServices(const std::vector<std::wstring> &dependents) {
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm.h)
        return;
    for (const wchar_t *name : kNvServices)
        startService(scm.h, name);
    // Dependents were recorded innermost-first; start them in reverse.
    for (auto it = dependents.rbegin(); it != dependents.rend(); ++it) {
        if (std::find(std::begin(kNvServices), std::end(kNvServices), *it) ==
            std::end(kNvServices))
            startService(scm.h, *it);
    }
}

// ---- Operations (worker thread) -------------------------------------------

static bool quitting() {
    return WaitForSingleObject(g_quit, 0) == WAIT_OBJECT_0;
}

// Before sleep: enables the NVIDIA adapters that are disabled, recording them
// so they can be disabled again after wake. Adapters that are already enabled
// are left alone. Only the device matters here; the services are left alone
// since the adapter goes back to disabled right after wake.
static void enableForSleep() {
    std::vector<std::wstring> targets;
    for (const auto &id : findNvidiaGpus()) {
        if (isDisabled(id))
            targets.push_back(id);
    }
    if (targets.empty())
        return;

    logA("[nvidia] sleep pending: enabling adapter before sleep");
    const ULONGLONG start = GetTickCount64();
    // Record first: a crash mid-enable must still restore the disabled state.
    std::vector<std::wstring> owned = readMarker();
    for (const auto &id : targets) {
        if (std::find(owned.begin(), owned.end(), id) == owned.end())
            owned.push_back(id);
    }
    writeMarker(owned);

    bool ok = true;
    for (const auto &id : targets) {
        if (!setDeviceEnabled(id, true) || !waitForDisabled(id, false)) {
            logA("[nvidia] could not enable an adapter");
            ok = false;
        }
    }
    logA("[nvidia] enable before sleep done in %llu ms (ok=%d)",
         GetTickCount64() - start, ok);
}

// After wake (or on quit / preference off / after a crash): disables again
// the adapters this app enabled for a sleep, so the user's choice is restored.
static void restoreOwned() {
    std::vector<std::wstring> targets;
    for (const auto &id : readMarker()) {
        if (!isDisabled(id))
            targets.push_back(id);
    }
    if (targets.empty()) {
        clearMarker();
        return;
    }

    logA("[nvidia] restoring adapter to disabled");
    const ULONGLONG start = GetTickCount64();
    // Stop the services first so they release the GPU, then bring them back.
    auto dependents = stopNvidiaServices();
    for (const auto &id : targets) {
        if (!setDeviceEnabled(id, false) || !waitForDisabled(id, true))
            logA("[nvidia] could not disable an adapter");
    }
    startNvidiaServices(dependents);
    clearMarker();
    logA("[nvidia] restore done in %llu ms", GetTickCount64() - start);
}

// Brings the device in line with the latest state. Returns false on quit.
static bool converge() {
    for (;;) {
        const unsigned rev = g_intentRev;

        if (g_managed && g_sleepIntent) {
            enableForSleep();
            // Ready even if the enable failed: blocking sleep forever is worse.
            g_enabledRev = rev;
        } else {
            restoreOwned();
        }

        if (quitting())
            return false;
        // Another change arrived while working: converge again.
        if (WaitForSingleObject(g_wake, 0) != WAIT_OBJECT_0)
            return true;
    }
}

static DWORD WINAPI workerProc(void *) {
    HANDLE events[2] = {g_quit, g_wake};
    for (;;) {
        if (WaitForMultipleObjects(2, events, FALSE, INFINITE) == WAIT_OBJECT_0)
            break;
        if (!converge())
            break;
    }
    restoreOwned();
    return 0;
}

// ---- Public API ------------------------------------------------------------

void nvidiaGpuStart(bool managed) {
    if (g_thread)
        return;
    g_wake = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    g_quit = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    g_managed = managed;
    g_thread = CreateThread(nullptr, 0, workerProc, nullptr, 0, nullptr);
    SetEvent(g_wake); // undoes an enable left behind by a crashed run
}

void nvidiaGpuSetManaged(bool managed) {
    g_managed = managed;
    if (g_wake)
        SetEvent(g_wake);
}

void nvidiaGpuSetSleepIntent(bool sleeping) {
    if (g_sleepIntent.exchange(sleeping) == sleeping)
        return;
    if (sleeping)
        ++g_intentRev;
    if (g_wake)
        SetEvent(g_wake);
}

bool nvidiaGpuIsReadyForSleep() {
    return !g_managed || (g_sleepIntent && g_enabledRev == g_intentRev);
}

void nvidiaGpuShutdown() {
    if (!g_thread)
        return;
    SetEvent(g_quit);
    WaitForSingleObject(g_thread, INFINITE);
    CloseHandle(g_thread);
    g_thread = nullptr;
}
