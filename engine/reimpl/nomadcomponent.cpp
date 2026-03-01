// engine/reimpl/nomadcomponent.cpp
// Reimplementation of NomadComponent lifecycle methods.
//
// NomadComponent (0x78 bytes) — Component with SHIFTED controller sub-object at +0x58.
// Manages nomad tracking (position, time, entity reference).
//
// Vtable at 0x14015a020:
//   [3] Shutdown = 0x14004fc40 (passthrough — serialization with InternalBasePtr + Vector3 + Time)
//   [7] OnCreate = 0x14004fd20 (passthrough — registers owner with game state nomad list)
//   All other slots are base no-ops.

#include "NomadComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Shutdown (0x14004fc40) — passthrough
// Serializes Component base + version block + InternalBasePtr at +0x28 (with
// IsDerivedFrom validation) + two Vector3 at +0x30/+0x3c + int at +0x50 +
// Time at +0x48. 49-instruction function with heavy engine deps.
// ============================================================
typedef void (*NomadShutdown_t)(void* self, void* stream);
static HookInfo g_hookShutdown;

static void Reimpl_Shutdown(void* self, void* stream) {
    g_lastHook = "NomadComponent::Shutdown";
    ((NomadShutdown_t)g_hookShutdown.trampoline)(self, stream);
}

void InstallNomadComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14004fc40);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[NomadComponent] Shutdown hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveNomadComponentShutdown() { RemoveHook(&g_hookShutdown); }

// ============================================================
// OnCreate (0x14004fd20) — passthrough
// Registers owner entity (+0x10) with game state nomad list at
// DAT_1401c3dd8 + 0x120 via FUN_140103480.
// Only 9 instructions but uses game state singleton + internal list push.
// ============================================================
typedef void (*NomadOnCreate_t)(void* self);
static HookInfo g_hookOnCreate;

static void Reimpl_OnCreate(void* self) {
    g_lastHook = "NomadComponent::OnCreate";
    ((NomadOnCreate_t)g_hookOnCreate.trampoline)(self);
}

void InstallNomadComponentOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14004fd20);
    // Prologue: SUB RSP,0x28(4) + MOV RAX,[RCX+0x10](4) + MOV RCX,[0x1401c3dd8](7) = 15
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 15);
#ifdef _DEBUG
    printf("[NomadComponent] OnCreate hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveNomadComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
