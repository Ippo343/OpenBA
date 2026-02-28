// engine/reimpl/mapcomponent.cpp
// Reimplementation of MapComponent lifecycle methods from Application-x64.exe.
//
// MapComponent (0x38 bytes) manages tile map coordinates for entities.
// Fields: +0x28..+0x2E = four uint16 tile coords (initialized to 0xFFFF = invalid)
//
// Vtable at 0x140163aa0. Non-trivial overrides:
//   Shutdown  (0x1401243e0) — Serializes tile coords (+0x28..+0x2E as shorts) + extra int at +0x30
//   Draw      (0x140124370) — Checks building data flag, calls map update helper
//   OnCreate  (0x1401242f0) — Reads building data, calls map clear + map setup helpers
//
// Note: Serialize, OnRemove, Update, Slot10, OnUpdate2 are all no-ops.
//
// Shutdown decompilation (0x1401243e0):
//   Component_Serialize(self, stream)
//   Serializer(stream, version=0)
//   Serialize(self+0x28), Serialize(self+0x2A), Serialize(self+0x2C), Serialize(self+0x2E)
//   Serialize(self+0x30)    // extra int field (padding area in header)
//   ~Serializer()
//
// Draw decompilation (0x140124370):
//   Get building data via IsDerivedFrom(self+0x18, DAT_1401c7a18)
//   If building[0x30] != 0: call FUN_1401241b0(self)  // update map overlay
//
// OnCreate decompilation (0x1401242f0):
//   Get building data via IsDerivedFrom
//   If building[0x32] != 0: call terrain clear function + map setup helper
//   call FUN_140124750(self)  // additional map initialization
//
// Strategy: Passthrough wrappers. Draw/OnCreate interact with terrain/map subsystem.
//
// Prologue sizes:
//   Shutdown: 15 (PUSH RDI + SUB RSP,0x40 + MOV [RSP+0x20],-0x2)
//   Draw:     14 (MOV [RSP+8],RBX + PUSH RDI + SUB RSP,0x20 + MOV RBX,[RCX+0x18])
//   OnCreate: 14 (MOV [RSP+8],RBX + PUSH RDI + SUB RSP,0x30 + MOV RBX,[RCX+0x18])

#include "MapComponent.h"
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
// Passthrough wrappers
// ============================================================
static void Reimpl_Shutdown(void* self, void* stream) {
    typedef void (*Orig_t)(void*, void*);
    ((Orig_t)g_hookShutdown.trampoline)(self, stream);
}

static void Reimpl_Draw(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookDraw.trampoline)(self);
}

static void Reimpl_OnCreate(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookOnCreate.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallMapComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401243e0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[MapComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveMapComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallMapComponentDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140124370);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 14);
#ifdef _DEBUG
    printf("[MapComponent] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveMapComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallMapComponentOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401242f0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 14);
#ifdef _DEBUG
    printf("[MapComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveMapComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
