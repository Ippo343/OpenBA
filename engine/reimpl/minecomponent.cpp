// engine/reimpl/minecomponent.cpp
// Hook wrappers for MineComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/minecomponent.cpp.

#include "MineComponent.h"
#include "Component/minecomponent_funcs.h"
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
static void Reimpl_Shutdown(MineComponent* self, void* stream) {
    g_lastHook = "MineComponent::Shutdown";
    MineComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallMineComponentShutdown() {
    MineComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004f260);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[MineComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveMineComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}
