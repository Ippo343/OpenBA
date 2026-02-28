// engine/reimpl/happinesscomponent.cpp
// Reimplementation of HappinessComponent lifecycle methods.
//
// HappinessComponent (0x50 bytes) — ControllerComponent subclass (shifted controller
// sub-object at +0x30). Tracks citizen happiness state.
//
// Vtable at 0x1401595d0:
//   [3] Shutdown  = 0x14004aa80 (passthrough — serialization)
//   [7] OnCreate  = 0x14004aa20 (passthrough — registers with game state list)
//   All other slots are base no-ops.

#include "HappinessComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Shutdown (0x14004aa80) — passthrough
// Serializes Component base + happinessFlag byte at +0x28.
// Uses IO::Serializer heavily; passthrough is safest.
// ============================================================
typedef void (*HappinessShutdown_t)(void* self, void* stream);
static HookInfo g_hookShutdown;

static void Reimpl_Shutdown(void* self, void* stream) {
    ((HappinessShutdown_t)g_hookShutdown.trampoline)(self, stream);
}

void InstallHappinessComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14004aa80);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[HappinessComponent] Shutdown hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveHappinessComponentShutdown() { RemoveHook(&g_hookShutdown); }

// ============================================================
// OnCreate (0x14004aa20) — passthrough
// If happinessFlag != 0, registers owner with game state happiness list,
// calls FUN_14004a300 to set index to -1, then clears flag.
// Uses InternalBasePtr + game state singleton; passthrough is safest.
// ============================================================
typedef void (*HappinessOnCreate_t)(void* self);
static HookInfo g_hookOnCreate;

static void Reimpl_OnCreate(void* self) {
    ((HappinessOnCreate_t)g_hookOnCreate.trampoline)(self);
}

void InstallHappinessComponentOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14004aa20);
    // Prologue: PUSH RBX(1) + SUB RSP,0x20(4) + CMP [RCX+0x28],0(4) + MOV RBX,RCX(3) = 12
    // But 0x14004aa2d - 0x14004aa20 = 0xD = 13
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[HappinessComponent] OnCreate hooked at %p (passthrough, prologue=13)\n", (void*)target);
#endif
}
void RemoveHappinessComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
