// engine/reimpl/happinesscomponent.cpp
// Reimplementation of HappinessComponent lifecycle methods.
//
// HappinessComponent (0x50 bytes) — ControllerComponent subclass (shifted controller
// sub-object at +0x30). Tracks citizen happiness state.
//
// Vtable at 0x1401595d0:
//   [3] Shutdown  = 0x14004aa80 (serialization)
//   [7] OnCreate  = 0x14004aa20 (registers with game state list)

#include "HappinessComponent.h"
#include "Component/happinesscomponent_funcs.h"
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
// Shutdown (0x14004aa80)
// ============================================================
static void Reimpl_Shutdown(HappinessComponent* self, void* stream) {
    g_lastHook = "HappinessComponent::Shutdown";
    HappinessComponent_Shutdown(self, stream);
}

// ============================================================
// OnCreate (0x14004aa20)
// ============================================================
static void Reimpl_OnCreate(HappinessComponent* self) {
    g_lastHook = "HappinessComponent::OnCreate";
    HappinessComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallHappinessComponentShutdown() {
    HappinessComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004aa80);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[HappinessComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveHappinessComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallHappinessComponentOnCreate() {
    HappinessComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004aa20);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[HappinessComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveHappinessComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
