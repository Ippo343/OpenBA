// engine/reimpl/trackercomponent.cpp
// Hook wrappers for TrackerComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/trackercomponent.cpp.

#include "TrackerComponent.h"
#include "Component/trackercomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(TrackerComponent* self, void* stream) {
    g_lastHook = "TrackerComponent::Shutdown";
    TrackerComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTrackerComponentShutdown() {
    TrackerComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140059130);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[TrackerComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveTrackerComponentShutdown() { RemoveHook(&g_hookShutdown); }
