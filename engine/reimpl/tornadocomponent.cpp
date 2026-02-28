// engine/reimpl/tornadocomponent.cpp
// Reimplementation of TornadoComponent lifecycle methods from Application-x64.exe.
//
// TornadoComponent (0x68 bytes) manages tornado disasters. Has the LARGEST
// Serialize (236 instructions / 981 bytes) and a massive OnRemove (210 instructions
// / 859 bytes) among all components.
//
// Vtable at 0x14015aa80. Non-trivial overrides:
//   Shutdown  (0x140058b80) — Serializes Vector3 position (+0x30), tornadoId (+0x50),
//     path waypoint array (+0x40/+0x48), Time (+0x28), direction Vector3 (+0x58)
//   Serialize (0x1400585a0) — 236 instructions, full tornado state serialization
//   OnRemove  (0x140057cd0) — 210 instructions: tornado movement, pathfinding,
//     audio emitter updates, entity damage, building interaction
//
// Note: Draw, OnCreate, Update, Slot5, Slot10, OnUpdate2 are all base no-ops.
//
// Strategy: All passthrough wrappers. Serialize and OnRemove are among the most
// complex methods in the entire component system (combined 446 instructions).
// They interact with pathfinding, audio scene, entity destruction, and rendering.
//
// Prologue sizes:
//   Shutdown: 15 (PUSH RDI + SUB RSP,0x40 + MOV [RSP+0x20],-0x2)
//   Serialize: 12 (PUSH RBP + PUSH RBX + PUSH RSI + PUSH RDI + PUSH R12 + ...)
//   OnRemove: 12 (PUSH RBP + PUSH RBX + PUSH RSI + PUSH R12 + PUSH R13 + ...)

#include "TornadoComponent.h"
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

// ============================================================
// Passthrough wrappers
// ============================================================
static void Reimpl_Shutdown(void* self, void* stream) {
    typedef void (*Orig_t)(void*, void*);
    ((Orig_t)g_hookShutdown.trampoline)(self, stream);
}

static void Reimpl_Serialize(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookSerialize.trampoline)(self);
}

static void Reimpl_OnRemove(void* self, void* time) {
    typedef void (*Orig_t)(void*, void*);
    ((Orig_t)g_hookOnRemove.trampoline)(self, time);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTornadoComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140058b80);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[TornadoComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveTornadoComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTornadoComponentSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400585a0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 12);
#ifdef _DEBUG
    printf("[TornadoComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTornadoComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallTornadoComponentOnRemove() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140057cd0);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 12);
#ifdef _DEBUG
    printf("[TornadoComponent] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveTornadoComponentOnRemove() { RemoveHook(&g_hookOnRemove); }
