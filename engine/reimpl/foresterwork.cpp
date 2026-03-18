// engine/reimpl/foresterwork.cpp
// Hook wrappers for ForesterWork methods from Application-x64.exe.
//
// ForesterWork — Forester resource gathering work action (0x60 bytes).
// Inherits: Object::Internal -> Work -> AutoPickupWork -> ForesterWork
// Vtable at 0x1401613a0 (12 slots).
//
// Reimplemented methods (delegating to code/Game/Work/foresterwork.cpp):
//   [0] ScalarDtor (0x14002A370) — reimplemented
//   [3] Serialize  (0x1400EB970) — reimplemented
//   [4] Slot4      (0x1400EAEC0) — reimplemented (forwards to original internally)
//   [8] Slot8      (0x1400EB8A0) — reimplemented (CancelAll)
//  [10] Slot10     (0x1400EB910) — reimplemented (GetWorkEntity)
//
// Disabled hooks (documented only, clean code exists for standalone exe):
//   [9] Slot9      (0x1400EAC60) — DISABLED (bug class 3, MOV RAX,RSP prologue)
//
// Not hookable:
//   [1] VectorDtor (0x14002A300) — 8 bytes
//   [2] BaseDtor   (0x14002A2F0) — 7 bytes
//   [5] Execute    (0x1400F7540) — 3 bytes, returns false
//   [6] Slot6      (0x1400EAEA0) — RIP-relative CALL (bug class 2)
//   [7] Slot7      (0x1400EAE90) — 9 bytes, thunk
//  [11] Slot11     (0x140029860) — 9 bytes, returns (this->field50 > 0)

#include "ForesterWork.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Work/foresterwork_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool ForesterWork_ResolveFuncs();

// ============================================================
// ScalarDtor (0x14002A370) — reimplemented
// ============================================================
typedef void* (*FW_ScalarDtor_t)(void* self, uint32_t flags);
static HookInfo g_hookScalarDtor;

static void* Reimpl_ScalarDtor(ForesterWork* self, uint32_t flags) {
    g_lastHook = "ForesterWork::ScalarDtor";
    return ForesterWork_ScalarDtor_Impl(self, flags);
}

void InstallForesterWorkScalarDtor() {
    ForesterWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002A370);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[ForesterWork] ScalarDtor hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveForesterWorkScalarDtor() { RemoveHook(&g_hookScalarDtor); }

// ============================================================
// Serialize (0x1400EB970) — reimplemented
// ============================================================
typedef void (*FW_Serialize_t)(void* self, void* stream);
static HookInfo g_hookSerialize;

static void Reimpl_Serialize(ForesterWork* self, void* stream) {
    g_lastHook = "ForesterWork::Serialize";
    ForesterWork_Serialize_Impl(self, stream);
}

void InstallForesterWorkSerialize() {
    ForesterWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EB970);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[ForesterWork] Serialize hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveForesterWorkSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// Slot4 (0x1400EAEC0) — reimplemented (forwards to original internally)
// 599-instruction main work-assignment logic.
// ============================================================
typedef uint64_t (*FW_Slot4_t)(void* self, void* param2, uint64_t param3, uint64_t param4);
static HookInfo g_hookSlot4;

static uint64_t Reimpl_Slot4(ForesterWork* self, void* param2, uint64_t param3, uint64_t param4) {
    g_lastHook = "ForesterWork::Slot4";
    return (uint64_t)ForesterWork_Slot4_Impl(self, param2, param3, param4);
}

void InstallForesterWorkSlot4() {
    ForesterWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EAEC0);
    // Prologue: MOV [RSP+0x10],RDX(5) + MOV [RSP+0x8],RCX(5) + PUSH RBP(1) + PUSH R13(2) = 13
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[ForesterWork] Slot4 hooked at %p (reimplemented, forwards to original)\n", (void*)target);
#endif
}
void RemoveForesterWorkSlot4() { RemoveHook(&g_hookSlot4); }

// ============================================================
// Slot6 (0x1400EAEA0) — NOT HOOKABLE
// 20-byte thunk with RIP-relative CALL rel32 at offset 8.
// Clean code exists in foresterwork.cpp for standalone exe.
// ============================================================

// ============================================================
// Slot8 / CancelAll (0x1400EB8A0) — reimplemented
// ============================================================
typedef void (*FW_Slot8_t)(void* self);
static HookInfo g_hookSlot8;

static void Reimpl_Slot8(ForesterWork* self) {
    g_lastHook = "ForesterWork::Slot8";
    ForesterWork_Slot8_Impl(self);
}

void InstallForesterWorkSlot8() {
    ForesterWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EB8A0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x20(4) + CMP [RCX+0x50],0(4) + MOV RDI,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[ForesterWork] Slot8 hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveForesterWorkSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Slot9 / RemoveInvalid (0x1400EAC60) — DISABLED (bug class 3)
// MOV RAX,RSP prologue captures wrong RSP in trampoline.
// Clean code exists in foresterwork.cpp for standalone exe.
// Kept as passthrough for when hook is re-enabled.
// ============================================================
typedef bool (*FW_Slot9_t)(void* self, void* param2);
static HookInfo g_hookSlot9;

static bool Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "ForesterWork::Slot9";
    return ((FW_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

void InstallForesterWorkSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EAC60);
    // Prologue: MOV RAX,RSP(3) + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) +
    //           PUSH R12(2) + PUSH R13(2) + PUSH R14(2) = 12
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 12);
#ifdef _DEBUG
    printf("[ForesterWork] Slot9 hooked at %p (passthrough, DISABLED bug class 3)\n", (void*)target);
#endif
}
void RemoveForesterWorkSlot9() { RemoveHook(&g_hookSlot9); }

// ============================================================
// Slot10 / GetWorkEntity (0x1400EB910) — reimplemented
// Hidden return ptr pattern (RCX=this, RDX=outPtr).
// ============================================================
typedef void* (*FW_Slot10_t)(void* self, void* outPtr);
static HookInfo g_hookSlot10;

static void* Reimpl_Slot10(ForesterWork* self, void* outPtr) {
    g_lastHook = "ForesterWork::Slot10";
    return ForesterWork_Slot10_Impl(self, outPtr);
}

void InstallForesterWorkSlot10() {
    ForesterWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EB910);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 14
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 14);
#ifdef _DEBUG
    printf("[ForesterWork] Slot10 hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveForesterWorkSlot10() { RemoveHook(&g_hookSlot10); }
