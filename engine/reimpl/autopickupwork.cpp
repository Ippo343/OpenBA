// engine/reimpl/autopickupwork.cpp
// Reimplementation of AutoPickupWork base class methods from Application-x64.exe.
//
// AutoPickupWork — Abstract base for auto-pickup work actions (0x48 bytes).
// Inherits: Object::Internal -> Work -> AutoPickupWork
// Vtable at 0x140160fa0 (12 slots):
//
// Hookable methods:
//   [3] Serialize (0x1400E3F80) — passthrough (calls Work::Serialize + empty serializer block)
//   [8] Slot8    (0x1400E3F30) — passthrough (frees item list from entity)
//   Ctor        (0x1400E4050) — passthrough (initializes Work base fields + sets APW vtable)
//
// Too small to hook (documented only):
//   [0] ScalarDtor (0x1400294E0) — 15-inst function, calls Work dtor chain
//   [1] VectorDtor (0x1400294D0) — 8 bytes, returns global
//   [2] BaseDtor   (0x1400294C0) — 7 bytes, returns global
//   [5] Execute    (0x1400F7540) — 3 bytes, returns false (xor al,al; ret)
//   [4],[6],[7],[9],[10],[11] = _purecall (0x14003ADDC)

#include "AutoPickupWork.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Serialize (0x1400E3F80) — NOT HOOKABLE
// Prologue: PUSH RBX(2) + SUB RSP,0x30(4) + MOV RBX,RDX(3) = 9 bytes,
// then CALL rel32 at byte 9. Prologue boundary (9) < minimum hook size (12).
// Cannot hook without overwriting the RIP-relative CALL instruction.
// ============================================================

// ============================================================
// Slot8 (0x1400E3F30) — passthrough
// Frees the item list from the entity at this+0x10.
// Looks up entity, gets sub-object, frees memory at +0x30/+0x38.
// ============================================================
typedef void (*APW_Slot8_t)(void* self);
static HookInfo g_hookSlot8;

static void Reimpl_Slot8(void* self) {
    g_lastHook = "AutoPickupWork::Slot8";
    ((APW_Slot8_t)g_hookSlot8.trampoline)(self);
}

void InstallAutoPickupWorkSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400E3F30);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) + MOV RCX,RBX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[AutoPickupWork] Slot8 hooked at %p (passthrough, prologue=13)\n", (void*)target);
#endif
}
void RemoveAutoPickupWorkSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Ctor (0x1400E4050) — passthrough
// Calls Object::Internal::Internal, sets Work vtable, zeroes all
// Work fields (0x10-0x40), sets sentinels (-1 at +0x20/+0x28),
// then overwrites vtable to AutoPickupWork vtable.
// ============================================================
typedef void* (*APW_Ctor_t)(void* self);
static HookInfo g_hookCtor;

static void* Reimpl_Ctor(void* self) {
    g_lastHook = "AutoPickupWork::Ctor";
    return ((APW_Ctor_t)g_hookCtor.trampoline)(self);
}

void InstallAutoPickupWorkCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400E4050);
    // Prologue: MOV [RSP+8],RCX(5) + PUSH RBX(1) + SUB RSP,0x30(4) + MOV [RSP+0x20],-2(9) = 19
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[AutoPickupWork] Ctor hooked at %p (passthrough, prologue=19)\n", (void*)target);
#endif
}
void RemoveAutoPickupWorkCtor() { RemoveHook(&g_hookCtor); }
