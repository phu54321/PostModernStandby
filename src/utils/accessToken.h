#pragma once

// Code from https://m.blog.naver.com/stop2y/221130663783

#include <windows.h>

DWORD EnableNTPrivilege(LPCTSTR szPrivilege, DWORD dwState);

// True if the process runs with an elevated (administrator) token.
bool isElevated();

// Starts another instance of this exe with administrator rights (shows the UAC
// prompt). Returns false if it could not be started, e.g. the user declined.
bool relaunchElevated();
