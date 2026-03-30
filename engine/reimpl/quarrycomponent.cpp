// engine/reimpl/quarrycomponent.cpp
// Hook wrappers for QuarryComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/quarrycomponent.cpp.

#include "QuarryComponent.h"
#include "Component/quarrycomponent_funcs.h"
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
static void Reimpl_Shutdown(QuarryComponent* self, void* stream) {
    g_lastHook = "QuarryComponent::Shutdown";
    QuarryComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallQuarryComponentShutdown() {
    QuarryComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140055230);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[QuarryComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveQuarryComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}
