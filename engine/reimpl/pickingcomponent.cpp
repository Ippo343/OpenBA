// engine/reimpl/pickingcomponent.cpp
// Hook wrappers for PickingComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/pickingcomponent.cpp.

#include "PickingComponent.h"
#include "Component/pickingcomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookOnCreate;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(PickingComponent* self, void* stream) {
    g_lastHook = "PickingComponent::Shutdown";
    PickingComponent_Shutdown(self, stream);
}

static void Reimpl_OnCreate(PickingComponent* self) {
    g_lastHook = "PickingComponent::OnCreate";
    PickingComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallPickingComponentShutdown() {
    PickingComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140128bb0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[PickingComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemovePickingComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallPickingComponentOnCreate() {
    PickingComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140128b50);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[PickingComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemovePickingComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
