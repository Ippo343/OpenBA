// engine/reimpl/workplacecomponent.cpp
// Reimplementation of WorkPlaceComponent lifecycle methods.
//
// WorkPlaceComponent (0x68 bytes) — ControllerComponent subclass.
// Manages worker assignments to buildings.
//
// Vtable at 0x140164d30:
//   [3] Shutdown = 0x14012f120 (passthrough — 80-instruction serialization with version check)
//   [7] OnCreate = 0x14012f090 (passthrough — BST traversal to register with profession list)
//   All other slots are base no-ops.

#include "WorkPlaceComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Shutdown (0x14012f120) — passthrough
// 80-instruction function. Serializes Component base + version block +
// worker array at +0x58 (count at +0x60) with IsDerivedFrom validation +
// isAccepting bool at +0x50. Has version check (GetVersion < 1 path reads
// from +0x18 object via IsDerivedFrom). Very complex.
// ============================================================
typedef void (*WorkPlaceShutdown_t)(void* self, void* stream);
static HookInfo g_hookShutdown;

static void Reimpl_Shutdown(void* self, void* stream) {
    g_lastHook = "WorkPlaceComponent::Shutdown";
    ((WorkPlaceShutdown_t)g_hookShutdown.trampoline)(self, stream);
}

void InstallWorkPlaceComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012f120);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[WorkPlaceComponent] Shutdown hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveWorkPlaceComponentShutdown() { RemoveHook(&g_hookShutdown); }

// ============================================================
// OnCreate (0x14012f090) — passthrough
// 36-instruction function. Reads +0x48 (profession ID), looks it up in a
// BST (binary search tree) at game state +0xf0+0x100. If found, registers
// owner via FUN_140002cc0. If not found, calls FUN_14012ee60 fallback init.
// Uses BST traversal — too complex to safely reimpl without full tree types.
// ============================================================
typedef void (*WorkPlaceOnCreate_t)(void* self);
static HookInfo g_hookOnCreate;

static void Reimpl_OnCreate(void* self) {
    g_lastHook = "WorkPlaceComponent::OnCreate";
    ((WorkPlaceOnCreate_t)g_hookOnCreate.trampoline)(self);
}

void InstallWorkPlaceComponentOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012f090);
    // Prologue: PUSH RBX(1) + SUB RSP,0x20(4) + MOV RAX,[RCX+0x48](4) + MOV RBX,RCX(3) = 12
    // 0x14012f09d - 0x14012f090 = 0xD = 13
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[WorkPlaceComponent] OnCreate hooked at %p (passthrough, prologue=13)\n", (void*)target);
#endif
}
void RemoveWorkPlaceComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
