#pragma once

// Makes the sleeps this app initiates safe for a disabled NVIDIA dGPU, which
// can fail to come back after a Modern Standby wake. Windows gives apps almost
// no time once the display turns off, so a disabled dGPU is enabled before the
// app turns the display off, and disabled again after wake:
//
//   timer loop enters SLEEPPENDING -> nvidiaGpuSetSleepIntent(true) (enable)
//   timer loop sleeps only once nvidiaGpuIsReadyForSleep()
//   timer loop returns to an awake state -> nvidiaGpuSetSleepIntent(false)
//
// Enabled and disabled are both normal states: only an adapter that was
// disabled when the sleep started is touched, and it is put back to disabled
// afterwards. Nothing is toggled on startup or quit, except undoing an enable
// left behind by an unfinished sleep (e.g. a crash).
//
// All device and service work runs on one background thread that converges to
// the latest intent. Requires administrator rights; failures are logged.

void nvidiaGpuStart(bool managed);

// Called when the reenable_nvidia_before_sleep preference changes.
void nvidiaGpuSetManaged(bool managed);

// true: the app is about to sleep, enable the dGPU if it is disabled.
// false: the app is awake, disable it again if this app had enabled it.
void nvidiaGpuSetSleepIntent(bool sleeping);

// True when sleeping now is safe: the module is unmanaged, or the enable for
// the current sleep intent has finished (or given up; failures are logged).
bool nvidiaGpuIsReadyForSleep();

// Stops the worker and undoes an enable this app made for a sleep.
void nvidiaGpuShutdown();
