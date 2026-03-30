// engine/reimpl/wellcomponent.cpp
// Hook wrappers for WellComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/wellcomponent.cpp.

#include "WellComponent.h"
#include "Component/wellcomponent_funcs.h"
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
static void Reimpl_Shutdown(WellComponent* self, void* stream) {
    g_lastHook = "WellComponent::Shutdown";
    WellComponent_Shutdown(self, stream);
}

static void Reimpl_OnCreate(WellComponent* self) {
    g_lastHook = "WellComponent::OnCreate";
    WellComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallWellComponentShutdown() {
    WellComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14005cfa0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[WellComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveWellComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}

void InstallWellComponentOnCreate() {
    WellComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14005cf60);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[WellComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}

void RemoveWellComponentOnCreate() {
    RemoveHook(&g_hookOnCreate);
}
