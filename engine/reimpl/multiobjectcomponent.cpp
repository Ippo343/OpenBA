// engine/reimpl/multiobjectcomponent.cpp
// Hook wrappers for MultiObjectComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/multiobjectcomponent.cpp.

#include "MultiObjectComponent.h"
#include "Component/multiobjectcomponent_funcs.h"
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
static void Reimpl_Shutdown(MultiObjectComponent* self, void* stream) {
    g_lastHook = "MultiObjectComponent::Shutdown";
    MultiObjectComponent_Shutdown(self, stream);
}

static void Reimpl_OnCreate(MultiObjectComponent* self) {
    g_lastHook = "MultiObjectComponent::OnCreate";
    MultiObjectComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallMultiObjectComponentShutdown() {
    MultiObjectComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140125880);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 18);
#ifdef _DEBUG
    printf("[MultiObjectComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveMultiObjectComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallMultiObjectComponentOnCreate() {
    MultiObjectComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x1401257C0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 15);
#ifdef _DEBUG
    printf("[MultiObjectComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveMultiObjectComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
