// engine/reimpl/mapcomponent.cpp
// Hook wrappers for MapComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/mapcomponent.cpp.
//
// MapComponent (0x38 bytes) manages tile map coordinates for entities.
// Vtable at 0x140163aa0. Non-trivial overrides:
//   Shutdown  (0x1401243e0) — Serializes tile coords + extra int
//   Draw      (0x140124370) — Checks building data flag, calls map update helper
//   OnCreate  (0x1401242f0) — Reads building data, calls map clear + map setup helpers

#include "MapComponent.h"
#include "Component/mapcomponent_funcs.h"
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
static void Reimpl_Shutdown(MapComponent* self, void* stream) {
    g_lastHook = "MapComponent::Shutdown";
    MapComponent_Shutdown(self, stream);
}

static void Reimpl_Draw(MapComponent* self) {
    g_lastHook = "MapComponent::Draw";
    MapComponent_Draw(self);
}

static void Reimpl_OnCreate(MapComponent* self) {
    g_lastHook = "MapComponent::OnCreate";
    MapComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallMapComponentShutdown() {
    MapComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401243e0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[MapComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveMapComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallMapComponentDraw() {
    MapComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140124370);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 14);
#ifdef _DEBUG
    printf("[MapComponent] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveMapComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallMapComponentOnCreate() {
    MapComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401242f0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 14);
#ifdef _DEBUG
    printf("[MapComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveMapComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
