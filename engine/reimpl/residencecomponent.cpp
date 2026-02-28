// engine/reimpl/residencecomponent.cpp
// Reimplementation of ResidenceComponent lifecycle methods.
//
// ResidenceComponent (0x60 bytes) — ControllerComponent subclass.
// Manages citizen residence assignments (housing).
//
// Vtable at 0x1401642c0:
//   [3] Shutdown  = 0x14012a170 (passthrough — complex serialization with Time + arrays)
//   [4] Serialize = 0x14012a120 (passthrough — IsDerivedFrom check + field init)
//   [7] OnCreate  = 0x14012a0e0 (passthrough — calls FUN_140129ef0 + registers with game state)
//   All other slots are base no-ops.

#include "ResidenceComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Shutdown (0x14012a170) — passthrough
// Serializes Component base + version block + field58 count + array resize +
// Time::Serialize at +0x48. Complex serialization chain.
// ============================================================
typedef void (*ResidenceShutdown_t)(void* self, void* stream);
static HookInfo g_hookShutdown;

static void Reimpl_Shutdown(void* self, void* stream) {
    ((ResidenceShutdown_t)g_hookShutdown.trampoline)(self, stream);
}

void InstallResidenceComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012a170);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ResidenceComponent] Shutdown hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveResidenceComponentShutdown() { RemoveHook(&g_hookShutdown); }

// ============================================================
// Serialize (0x14012a120) — passthrough
// Checks IsDerivedFrom on +0x18 with type DAT_1401c7d28, reads +0x20 from
// derived object, calls FUN_140037ad0 to init +0x50 array.
// ============================================================
typedef void (*ResidenceSerialize_t)(void* self);
static HookInfo g_hookSerialize;

static void Reimpl_Serialize(void* self) {
    ((ResidenceSerialize_t)g_hookSerialize.trampoline)(self);
}

void InstallResidenceComponentSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012a120);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV RBX,[RCX+0x18](4) = 14
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[ResidenceComponent] Serialize hooked at %p (passthrough, prologue=14)\n", (void*)target);
#endif
}
void RemoveResidenceComponentSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// OnCreate (0x14012a0e0) — passthrough
// Calls FUN_140129ef0 (initialization), then registers owner (+0x10)
// with game state list at DAT_1401c3dd8+0xf0+0x20.
// ============================================================
typedef void (*ResidenceOnCreate_t)(void* self);
static HookInfo g_hookOnCreate;

static void Reimpl_OnCreate(void* self) {
    ((ResidenceOnCreate_t)g_hookOnCreate.trampoline)(self);
}

void InstallResidenceComponentOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012a0e0);
    // Prologue: PUSH RBX(1) + SUB RSP,0x20(4) + MOV RBX,RCX(3) + CALL(5) = 13
    // 0x14012a0e9 - 0x14012a0e0 = 9, next boundary at CALL end = 0x14012a0ee = 14
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 14);
#ifdef _DEBUG
    printf("[ResidenceComponent] OnCreate hooked at %p (passthrough, prologue=14)\n", (void*)target);
#endif
}
void RemoveResidenceComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
