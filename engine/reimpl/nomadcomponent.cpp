// engine/reimpl/nomadcomponent.cpp
// Hook wrappers for NomadComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/nomadcomponent.cpp.

#include "NomadComponent.h"
#include "Component/nomadcomponent_funcs.h"
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
static void Reimpl_Shutdown(NomadComponent* self, void* stream) {
    g_lastHook = "NomadComponent::Shutdown";
    NomadComponent_Shutdown(self, stream);
}

static void Reimpl_OnCreate(NomadComponent* self) {
    g_lastHook = "NomadComponent::OnCreate";
    NomadComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallNomadComponentShutdown() {
    NomadComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004fc40);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[NomadComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveNomadComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallNomadComponentOnCreate() {
    NomadComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004fd20);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 15);
#ifdef _DEBUG
    printf("[NomadComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveNomadComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
