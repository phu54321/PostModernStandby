//
// Created by whyask37 on 2024-07-06.
//

#include <memory>
#include <Windows.h>
#include <powerbase.h>
#include <powrprof.h>
#include "utils/debugLog.h"
#include "utils/volumeRAII.h"
#include "utils/getMonitorName.h"
#include "utils/radio.h"
#include "preference.h"
#include "lidClose.h"
#include "keepAliveApp.hpp"
#include "res/resource.h"


bool isUsingS0ix() {
    static bool initialized = false;
    static bool isUsingS0ix;

    if (!initialized) {
        SYSTEM_POWER_CAPABILITIES spc = {0};
        NTSTATUS nRet = CallNtPowerInformation(SystemPowerCapabilities, NULL, 0, &spc, sizeof(spc));
        isUsingS0ix = spc.AoAc;
    }
    return isUsingS0ix;
}

bool isClamshell(const preference &pref) {
    if (!pref.allow_clamshell_on_battery && isBattery()) return false;

    auto names = getMonitorNames();
    bool hasMainMonitor = false;
    int nonMainMonitorCount = 0;
    for (const auto &n: names) {
        if (n == pref.main_monitor_name) {
            return false;
        }
        nonMainMonitorCount++;
    }
    return nonMainMonitorCount > 0;
}

bool shouldSleep(const preference &pref) {
    if (isLidClosed()) {
        if (checkKeepAliveApps(pref))
            return false;

        // check clamshell mode
        if (!isBattery()) {
            auto currentMonitor = getMonitorNameIfOnlyOne();
            if (!currentMonitor.empty() && currentMonitor != pref.main_monitor_name)
                return false;
        }

        return true;
    }
    return false;
}

bool isLidClosedAndNotClamshell(const preference &pref) {
    return isLidClosed() && !isClamshell(pref);
}

double currentTime() {
    return GetTickCount64() / 1000.0;
}

void sleepNow(const preference &pref, HWND hWnd) {
    if (isUsingS0ix()) {
        logA("[sleepNow] S0ix sleep");
        SendMessageA(hWnd, WM_SYSCOMMAND, SC_MONITORPOWER, (LPARAM) 2);
    } else {
        logA("[sleepNow] S3/S4 sleep");
        SetSuspendState(FALSE, TRUE, FALSE);
    }
}

// State
enum class State {
    ACTIVE,
    LIDCLOSEDACTIVE,
    SLEEPPENDING,
    SLEEP
};


void timerLoop(HWND hWnd, preference &pref) {
    static State state = State::ACTIVE;
    bool shouldSleepNow = shouldSleep(pref);
    bool isLidClosedNow = isLidClosedAndNotClamshell(pref);
    double now = currentTime();

//    logA("Timer loop");

    // Variables for each state
    // ACTIVE
    // No items

    // LIDCLOSEDACTIVE
    static double lidCloseAlarmTime;
    static std::unique_ptr<VolumeRAII> volumeRAII = nullptr;
    static double lidCloseAlarmEndTime;  // Private variable. Only valid when `volumeRAII` is not NULL.

    // SLEEPPENDING
    static double sleepInitiateTime;  // SLEEPPENDING & LIDCLOSEDSLEEP

    // SLEEP
    // static double sleepInitiateTime;  // SLEEPPENDING & LIDCLOSEDSLEEP
    static double lastTickTime;

    int failSafeCounter = 0;
    while ((++failSafeCounter) < 5) {
        switch (state) {
            case State::ACTIVE:
                // Laptop is awake. No sleep.
                if (shouldSleepNow) {
                    logA("Transition: ACTIVE -> SLEEPPENDING (shouldSleepNow)");
                    state = State::SLEEPPENDING;
                    sleepInitiateTime = now + pref.lid_close_sleep_delay_time;
                    break;
                } else if (isLidClosedNow) {
                    logA("Transition: ACTIVE -> LIDCLOSEDACTIVE (isLidClosedNow)");
                    state = State::LIDCLOSEDACTIVE;
                    lidCloseAlarmTime = now;
                    break;
                }
                return;

            case State::SLEEPPENDING:
                // Wait for 30 seconds before sleeping
                if (shouldSleepNow) {
                    if (now >= sleepInitiateTime) {
                        logA("Transition: SLEEPPENDING -> SLEEP (time elapsed) - now sleeping...");
                        sleepInitiateTime = now + 300;
                        lastTickTime = now;
                        state = State::SLEEP;
                        sleepNow(pref, hWnd);
                        break;
                    }
                } else {
                    if (isLidClosedNow) {
                        logA("Transition: SLEEPPENDING -> LIDCLOSEDACTIVE");
                        state = State::LIDCLOSEDACTIVE;
                        lidCloseAlarmTime = now;
                        break;
                    } else {
                        logA("Transition: SLEEPPENDING -> ACTIVE");
                        state = State::ACTIVE;
                        break;
                    }
                }
                return;

            case State::SLEEP:
                // Initiate sleep & (if required) re-sleep when laptop wake.
                if (shouldSleepNow) {
                    // Tick in 180 seconds: timer loop was interrupted more than 3 minutes
                    // This probably mean that the laptop was sleeping for at least 3 minutes
                    // before running this loop again.

                    // Since this application is legacy desktop application, after laptop paas
                    // DAM (desktop activity moderator) phase, this code shouldn't really run.

                    // In cases laptop wakes intermittently, Log that incidence & sleep again
                    if (lastTickTime + 180 <= now) {
                        // Check after 60 seconds
                        logA("Woken up during sleep: triggering re-sleep after 60 secs");
                        sleepInitiateTime = now + 60;
                    }

                    // Woke midwhile: sleep again
                    if (now >= sleepInitiateTime) {
                        // https://learn.microsoft.com/en-us/windows-hardware/design/device-experiences/modern-standby-states
                        // Laptop might not be able to sleep quickly enough, so we give 5 more minutes before trying to sleep again.
                        sleepInitiateTime = now + 300;
                        logA("Woke mid-sleep: sleeping again...");
                        sleepNow(pref, hWnd);
                    }

                    lastTickTime = now;
                } else {
                    if (isLidClosedNow) {
                        logA("Transition: SLEEP -> LIDCLOSEDACTIVE (isLidClosedNow)");
                        if (pref.disable_bluetooth_on_sleep) {
                            logA(" - resuming bluetooth");
                            resumeRadio();
                        }
                        state = State::LIDCLOSEDACTIVE;
                        lidCloseAlarmTime = now;
                        break;
                    } else {
                        logA("Transition: SLEEP -> ACTIVE");
                        if (pref.disable_bluetooth_on_sleep) {
                            resumeRadio();
                        }
                        state = State::ACTIVE;
                        break;
                    }
                }
                return;

            case State::LIDCLOSEDACTIVE:
                // Lid closed, but active due to clamshell mode or caffeinate
                // Throw alarm notification periodically
                // TODO: don't trigger alarm on clamshell mode
                if (shouldSleepNow) {
                    logA("Transition: LIDCLOSEDACTIVE -> SLEEPPENDING");
                    volumeRAII = nullptr;
                    state = State::SLEEPPENDING;
                    sleepInitiateTime = now + 10;
                    break;
                } else if (!isLidClosedNow) {
                    logA("Transition: LIDCLOSEDACTIVE -> ACTIVE");
                    volumeRAII = nullptr;
                    state = State::ACTIVE;
                    break;
                } else {
                    // Trigger alarm sound!
                    if (now >= lidCloseAlarmTime) {
                        auto hInstance = (HINSTANCE) GetWindowLongPtr(hWnd, GWLP_HINSTANCE);

                        // Set volume RAII so that the main speaker volumes up, even on mute.
                        volumeRAII = std::make_unique<VolumeRAII>(pref.lid_close_alert_volume / 100.0f);
                        PlaySound(MAKEINTRESOURCE(IDR_ALERT), hInstance, SND_RESOURCE | SND_ASYNC);
                        lidCloseAlarmEndTime = now + 1.5;
                        lidCloseAlarmTime += 30 * 60;  // Next alarm should fire 30 minute later.
                    }

                    if (volumeRAII && now >= lidCloseAlarmEndTime) {
                        logA("Unser volumeRAII");
                        volumeRAII = nullptr;
                    }
                }
                return;
        }
    }

    // Failsafe to prevent infinite loop;
    logA("somemthing weird happened");
}

