#pragma once

#include <string>

// Autostart is a Task Scheduler logon task. With elevated = true it runs with
// highest privileges, so an app that needs administrator rights starts at
// logon without a UAC prompt; registering that requires an elevated caller.
// Registering again replaces the task. Requires COM on the calling thread.
bool isAutostartEnabled();
bool setAutostart(bool enable, bool elevated, std::wstring &error);
