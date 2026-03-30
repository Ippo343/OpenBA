// engine/reimpl/naturalresourcecomponent.cpp
// Hook wrappers for NaturalResourceComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/naturalresourcecomponent.cpp.

#include "NaturalResourceComponent.h"
#include "Component/naturalresourcecomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookOnRemove;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(NaturalResourceComponent* self, void* stream) {
    g_lastHook = "NaturalResourceComponent::Shutdown";
    NaturalResourceComponent_Shutdown(self, stream);
}

static void Reimpl_OnRemove(NaturalResourceComponent* self) {
    g_lastHook = "NaturalResourceComponent::OnRemove";
    NaturalResourceComponent_OnRemove(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallNaturalResourceComponentShutdown() {
    NaturalResourceComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140125cf0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[NaturalResourceComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveNaturalResourceComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}

void InstallNaturalResourceComponentOnRemove() {
    NaturalResourceComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140125c40);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 13);
#ifdef _DEBUG
    printf("[NaturalResourceComponent] OnRemove hooked at %p\n", (void*)target);
#endif
}

void RemoveNaturalResourceComponentOnRemove() {
    RemoveHook(&g_hookOnRemove);
}
