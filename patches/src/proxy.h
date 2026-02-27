#pragma once
// Proxy version.dll - forwards all calls to the real system version.dll
// Drop the compiled version.dll next to Application-x64.exe and it auto-loads.

#include <windows.h>

// Initialize the proxy: loads the real version.dll and resolves all exports
bool ProxyInit();

// Cleanup
void ProxyShutdown();
