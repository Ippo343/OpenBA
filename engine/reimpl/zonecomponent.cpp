// engine/reimpl/zonecomponent.cpp
// Hook wrappers for ZoneComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/zonecomponent.cpp.

#include "ZoneComponent.h"
#include "Component/zonecomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(ZoneComponent* self, void* stream) {
    g_lastHook = "ZoneComponent::Shutdown";
    ZoneComponent_Shutdown(self, stream);
}

static void Reimpl_Draw(ZoneComponent* self) {
    g_lastHook = "ZoneComponent::Draw";
    ZoneComponent_Draw(self);
}

static void Reimpl_OnCreate(ZoneComponent* self) {
    g_lastHook = "ZoneComponent::OnCreate";
    ZoneComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallZoneComponentShutdown() {
    ZoneComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x1401302F0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ZoneComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveZoneComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallZoneComponentDraw() {
    ZoneComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x1401301F0);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 12);
#ifdef _DEBUG
    printf("[ZoneComponent] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveZoneComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallZoneComponentOnCreate() {
    ZoneComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140130080);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[ZoneComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveZoneComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
