// engine/reimpl/autopickupwork.cpp
// Reimplementation of AutoPickupWork base class methods from Application-x64.exe.
//
// AutoPickupWork — Abstract base for auto-pickup work actions (0x48 bytes).
// Inherits: Object::Internal -> Work -> AutoPickupWork
// Vtable at 0x140160fa0 (12 slots):
//
// Deeply reimplemented methods (clean C++ in code/Game/Work/autopickupwork.cpp):
//   [8] Slot8/OnRemove (0x1400E3F30) — frees item list from AutoPickupComponent
//   Ctor              (0x1400E4050) — initializes Work base fields + sets APW vtable
//
// Clean C++ only (not hookable, for future standalone exe):
//   [0] ScalarDtor (0x1400294E0) — too small to hook (19 bytes, but MOV [RSP+8] prologue)
//   [3] Serialize  (0x1400E3F80) — NOT hookable (prologue 9 bytes < 12 minimum)
//   [5] Execute    (0x1400F7540) — 3 bytes (xor al,al; ret), trivial return 0
//
// Not implemented (abstract or trivial):
//   [1] VectorDtor (0x1400294D0) — 8 bytes, returns global
//   [2] BaseDtor   (0x1400294C0) — 7 bytes, returns global
//   [4],[6],[7],[9],[10],[11] = _purecall (0x14003ADDC)

#include "AutoPickupWork.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Work/autopickupwork_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Slot8/OnRemove (0x1400E3F30) — deeply reimplemented
// Frees the item list from the AutoPickupComponent on the entity.
// ============================================================
typedef void (*APW_Slot8_t)(void* self);
static HookInfo g_hookSlot8;

static void Reimpl_Slot8(AutoPickupWork* self) {
    g_lastHook = "AutoPickupWork::OnRemove";
    AutoPickupWork_OnRemove(self);
}

void InstallAutoPickupWorkSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400E3F30);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) + MOV RCX,RBX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[AutoPickupWork] OnRemove hooked at %p (deep reimpl)\n", (void*)target);
#endif
}
void RemoveAutoPickupWorkSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Ctor (0x1400E4050) — deeply reimplemented
// Initializes Work base fields + sets AutoPickupWork vtable.
// ============================================================
typedef void* (*APW_Ctor_t)(void* self);
static HookInfo g_hookCtor;

static void* Reimpl_Ctor(AutoPickupWork* self) {
    g_lastHook = "AutoPickupWork::Ctor";
    return AutoPickupWork_Ctor(self);
}

void InstallAutoPickupWorkCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400E4050);
    // Prologue: MOV [RSP+8],RCX(5) + PUSH RBX(1) + SUB RSP,0x30(4) + MOV [RSP+0x20],-2(9) = 19
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[AutoPickupWork] Ctor hooked at %p (deep reimpl)\n", (void*)target);
#endif
}
void RemoveAutoPickupWorkCtor() { RemoveHook(&g_hookCtor); }
