// engine/reimpl/statusiconcomponent.cpp
// Hook wrappers for StatusIconComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/statusiconcomponent.cpp.
//
// StatusIconComponent (0x108 bytes) displays floating status icons above entities.
// Vtable at 0x140164620. Non-trivial overrides:
//   Shutdown  (0x14012bff0) — Serializes iconId field
//   Serialize (0x14012bed0) — Reads sprite data from building description
//   OnRemove  (0x14012bbd0) — Manages Scene add/remove, positions icon
//   OnUpdate2 (0x14012bf40) — Updates icon visibility

#include "StatusIconComponent.h"
#include "Component/statusiconcomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookSerialize;
static HookInfo g_hookOnRemove;
static HookInfo g_hookOnUpdate2;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(StatusIconComponent* self, void* stream) {
    g_lastHook = "StatusIconComponent::Shutdown";
    StatusIconComponent_Shutdown(self, stream);
}

static void Reimpl_Serialize(StatusIconComponent* self) {
    g_lastHook = "StatusIconComponent::Serialize";
    StatusIconComponent_Serialize(self);
}

static void Reimpl_OnRemove(StatusIconComponent* self) {
    g_lastHook = "StatusIconComponent::OnRemove";
    StatusIconComponent_OnRemove(self);
}

static void Reimpl_OnUpdate2(StatusIconComponent* self) {
    g_lastHook = "StatusIconComponent::OnUpdate2";
    StatusIconComponent_OnUpdate2(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallStatusIconComponentShutdown() {
    StatusIconComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012bff0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[StatusIconComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusIconComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallStatusIconComponentSerialize() {
    StatusIconComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012bed0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[StatusIconComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusIconComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallStatusIconComponentOnRemove() {
    StatusIconComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012bbd0);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 15);
#ifdef _DEBUG
    printf("[StatusIconComponent] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusIconComponentOnRemove() { RemoveHook(&g_hookOnRemove); }

void InstallStatusIconComponentOnUpdate2() {
    StatusIconComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012bf40);
    InstallHook(target, (void*)Reimpl_OnUpdate2, &g_hookOnUpdate2, 19);
#ifdef _DEBUG
    printf("[StatusIconComponent] OnUpdate2 hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusIconComponentOnUpdate2() { RemoveHook(&g_hookOnUpdate2); }
