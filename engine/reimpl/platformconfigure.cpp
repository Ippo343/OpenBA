// engine/reimpl/platformconfigure.cpp
// Skips the pre-game launcher dialog (Options/Video Options/Play).
//
// Engine::Engine::PlatformConfigure (0x1800a1670) shows a Win32 DialogBoxParamW
// launcher unless the game has been configured before. There appears to be a bug
// where the "already configured" flag doesn't persist, so the dialog always shows.
// This hook returns true unconditionally to skip it.

#include "hooks.h"
#include "resolve.h"
#include <cstdio>

namespace PlatformConfigureAddr {
    constexpr uintptr_t Func = 0x1800a1670;
}

static HookInfo g_hook;

static bool Reimpl_PlatformConfigure(void* /*engine*/, bool /*param1*/, void* /*hInstance*/) {
#ifdef _DEBUG
    printf("[PlatformConfigure] Skipping launcher dialog\n");
#endif
    return true;
}

void InstallPlatformConfigure() {
    uintptr_t target = ResolveAddr(Module::Runtime, PlatformConfigureAddr::Func);
    // Prologue: PUSH(2)+PUSH(2)+PUSH(2)+MOV(3)+SUB(4)=13 bytes
    InstallHook(target, (void*)Reimpl_PlatformConfigure, &g_hook, 13);
#ifdef _DEBUG
    printf("[PlatformConfigure] Hooked at %p -> skip launcher\n", (void*)target);
#endif
}

void RemovePlatformConfigure() {
    RemoveHook(&g_hook);
#ifdef _DEBUG
    printf("[PlatformConfigure] Hook removed\n");
#endif
}
