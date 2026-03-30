// engine/reimpl/orchardcomponent.cpp
// Hook wrappers for OrchardComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/orchardcomponent.cpp.

#include "OrchardComponent.h"
#include "Component/orchardcomponent_funcs.h"
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
static void Reimpl_Shutdown(OrchardComponent* self, void* stream) {
    g_lastHook = "OrchardComponent::Shutdown";
    OrchardComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallOrchardComponentShutdown() {
    OrchardComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140051de0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[OrchardComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveOrchardComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}
