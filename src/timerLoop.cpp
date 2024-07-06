//
// Created by whyask37 on 2024-07-06.
//

#include <memory>
#include <Windows.h>
#include <powerbase.h>
#include <powrprof.h>
#include "debugLog.h"
#include "utils/volumeRAII.h"
#include "preference.h"
#include "getMonitorName.h"
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

BOOL shouldSleep(const preference &pref) {
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

double currentTime() {
    return GetTickCount64() / 1000.0;
}

void sleepNow(HWND hWnd) {
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
    bool isLidClosedNow = isLidClosed();
    double now = currentTime();

//    logA("Timer loop");

    // Variables for each state
    // ACTIVE
    // No items

    // LIDCLOSEDACTIVE
    static double lidCloseAlarmTime;
    std::unique_ptr<VolumeRAII> volumeRAII = nullptr;
    static double lidCloseAlarmEndTime;  // Private variable. Only valid when `volumeRAII` is not NULL.

    // SLEEPPENDING
    static double sleepInitiateTime;  // SLEEPPENDING & LIDCLOSEDSLEEP

    // SLEEP
    // static double sleepInitiateTime;  // SLEEPPENDING & LIDCLOSEDSLEEP
    static double lastTickTime;

    switch (state) {
        case State::ACTIVE:
            // Laptop is awake. No sleep.
            if (shouldSleepNow) {
                logA("Transition: ACTIVE -> SLEEPPENDING (shouldSleepNow)");
                state = State::SLEEPPENDING;
                sleepInitiateTime = now + 30;
            } else if (isLidClosedNow) {
                logA("Transition: ACTIVE -> LIDCLOSEDACTIVE (isLidClosedNow)");
                state = State::LIDCLOSEDACTIVE;
                lidCloseAlarmTime = now;
            }
            break;

        case State::SLEEPPENDING:
            // Wait for 30 seconds before sleeping
            if (shouldSleepNow) {
                if (now >= sleepInitiateTime) {
                    logA("Transition: SLEEPPENDING -> SLEEP (time elapsed)");
                    sleepInitiateTime = now + 300;
                    lastTickTime = now;
                    state = State::SLEEP;
                    sleepNow(hWnd);
                }
            } else {
                if (isLidClosedNow) {
                    logA("Transition: SLEEPPENDING -> LIDCLOSEDACTIVE");
                    state = State::LIDCLOSEDACTIVE;
                    lidCloseAlarmTime = now;
                } else {
                    logA("Transition: SLEEPPENDING -> ACTIVE");
                    state = State::ACTIVE;
                }
            }
            break;

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
                    sleepNow(hWnd);
                }

                lastTickTime = now;
            } else {
                if (isLidClosedNow) {
                    logA("Transition: SLEEP -> LIDCLOSEDACTIVE (isLidClosedNow)");
                    state = State::LIDCLOSEDACTIVE;
                    lidCloseAlarmTime = now;
                } else {
                    logA("Transition: SLEEP -> ACTIVE");
                    state = State::ACTIVE;
                }
            }
            break;

        case State::LIDCLOSEDACTIVE:
            // Lid closed, but active due to clamshell mode or caffeinate
            // Throw alarm notification periodically
            // TODO: don't trigger alarm on clamshell mode
            if (shouldSleepNow) {
                logA("Transition: LIDCLOSEDACTIVE -> SLEEPPENDING");
                volumeRAII = nullptr;
                state = State::SLEEPPENDING;
                sleepInitiateTime = now + 30;
            } else if (!isLidClosedNow) {
                logA("Transition: LIDCLOSEDACTIVE -> ACTIVE");
                volumeRAII = nullptr;
                state = State::ACTIVE;
            } else {
                // Trigger alarm sound!
                if (now >= lidCloseAlarmTime) {
                    auto hInstance = (HINSTANCE) GetWindowLongPtr(hWnd, GWLP_HINSTANCE);

                    // Set volume RAII so that the main speaker volumes up, even on mute.
                    volumeRAII = std::make_unique<VolumeRAII>(pref.lid_close_alert_volume / 100.0f);
                    PlaySound(MAKEINTRESOURCE(IDR_ALERT), hInstance, SND_RESOURCE | SND_ASYNC);
                    lidCloseAlarmEndTime = now + 3;
                    lidCloseAlarmTime += 30 * 60;  // Next alarm should fire 30 minute later.
                }

                if (volumeRAII && now >= lidCloseAlarmEndTime) {
                    volumeRAII = nullptr;
                }
            }
    }
}

