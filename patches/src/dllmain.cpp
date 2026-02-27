// Banished Reimplementation - Proxy DLL Entry Point
// This DLL masquerades as version.dll. Drop it next to Application-x64.exe
// and it loads automatically with the game. All version.dll calls are
// forwarded to the real system DLL (see proxy.cpp).
//
// Init sequence:
//   1. ProxyInit() — load real version.dll
//   2. PatchInit() — start background thread that waits for Runtime-x64.dll
//   3. Background thread: ResolveInit() -> EngineFuncsInit() -> InstallHooks()

#include "proxy.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// Legacy wrapper — use ResolveAddr(Module::Exe, addr) in new code.
uintptr_t ghidra_addr(uintptr_t ghidra_address) {
    return ResolveAddr(Module::Exe, ghidra_address);
}

// Background init thread — waits for Runtime-x64.dll, then resolves everything.
static HANDLE g_initThread = nullptr;
static volatile bool g_initRunning = false;

static DWORD WINAPI InitThread(LPVOID) {
    g_initRunning = true;

    // Poll for Runtime-x64.dll (up to 30 seconds)
    for (int i = 0; i < 300 && g_initRunning; i++) {
        if (ResolveInit()) {
            // Runtime loaded — resolve engine functions
            if (!EngineFuncsInit()) {
#ifdef _DEBUG
                printf("[BanishedPatch] WARNING: EngineFuncsInit failed (non-critical funcs missing)\n");
#endif
            }

            // Install hooks and reimplementation redirects
            InstallHooks();

            g_initRunning = false;
            return 0;
        }
        Sleep(100);
    }

#ifdef _DEBUG
    printf("[BanishedPatch] WARNING: Runtime-x64.dll not loaded after 30s\n");
#endif
    g_initRunning = false;
    return 1;
}

static void PatchInit() {
#ifdef _DEBUG
    AllocConsole();
    freopen("patch_debug.log", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    setvbuf(stdout, NULL, _IONBF, 0);  // unbuffered so we can read in real-time
    printf("[BanishedPatch] Loaded. Starting init thread...\n");
#endif

    // Try immediate init (Runtime-x64.dll might already be loaded)
    if (ResolveInit()) {
        EngineFuncsInit();
        InstallHooks();
    } else {
        // Not loaded yet — start background thread
        g_initThread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    }
}

static void PatchShutdown() {
    // Stop init thread if still running
    g_initRunning = false;
    if (g_initThread) {
        WaitForSingleObject(g_initThread, 2000);
        CloseHandle(g_initThread);
        g_initThread = nullptr;
    }

    RemoveHooks();
    ProxyShutdown();

#ifdef _DEBUG
    printf("[BanishedPatch] Unloaded.\n");
    FreeConsole();
#endif
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        if (!ProxyInit()) return FALSE;
        PatchInit();
        break;
    case DLL_PROCESS_DETACH:
        PatchShutdown();
        break;
    }
    return TRUE;
}
