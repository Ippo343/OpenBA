// engine/reimpl/huntwork.cpp
// Reimplementation of HuntWork methods from Application-x64.exe.
//
// HuntWork — Hunting work action (0x58 bytes).
// Inherits: Object::Internal -> Work -> AutoPickupWork -> HuntWork
// Vtable at 0x140161620 (12 slots).
//
// REIMPLEMENTED methods (clean C++ in code/Game/Work/huntwork.cpp):
//   [0] ScalarDtor (0x14002A700) — calls dtor body 0x1400EF680, conditional free
//   [3] Serialize  (0x1400EF600) — Work::Serialize + tree-set (no extra int field)
//   [4] Slot4      (0x1400EEFF0) — 326 instructions, hunt target assignment logic
//   [8] Slot8      (0x1400EF530) — CancelAll
//  [10] Slot10     (0x1400EF5A0) — GetWorkEntity
//
// DISABLED (hook has MOV RAX,RSP — bug class 3, clean code only):
//   [9] Slot9      (0x1400EED90) — RemoveInvalid (passthrough via trampoline)
//
// NOT HOOKABLE (clean code in huntwork.cpp only, no hook wrapper):
//   [6] Slot6      (0x1400EEFD0) — thunk: sub rsp,0x28 + add rcx,0x48 + call + ret
//
// Too small to hook (clean code in huntwork.cpp only):
//   [1] VectorDtor (0x14002A680) — 8 bytes
//   [2] BaseDtor   (0x14002A670) — 7 bytes
//   [5] Execute    (0x1400F7540) — 3 bytes, returns false
//   [7] Slot7      (0x1400EEFC0) — 9 bytes, thunk: add rcx,0x48 + jmp
//  [11] Slot11     (0x140029860) — 9 bytes, returns (this->field50 > 0)

#include "HuntWork.h"
#include "Work/huntwork_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// ScalarDtor (0x14002A700) — REIMPLEMENTED
// ============================================================
typedef void* (*HuW_ScalarDtor_t)(void* self, uint32_t flags);
static HookInfo g_hookScalarDtor;

static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "HuntWork::ScalarDtor";
    return HuntWork_ScalarDtor_Impl((HuntWork*)self, flags);
}

void InstallHuntWorkScalarDtor() {
    HuntWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002A700);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[HuntWork] ScalarDtor hooked at %p (REIMPLEMENTED, prologue=12)\n", (void*)target);
#endif
}
void RemoveHuntWorkScalarDtor() { RemoveHook(&g_hookScalarDtor); }

// ============================================================
// Serialize (0x1400EF600) — REIMPLEMENTED
// Work::Serialize + tree-set at +0x48 only (no extra int field like ForesterWork).
// ============================================================
typedef void (*HuW_Serialize_t)(void* self, void* stream);
static HookInfo g_hookSerialize;

static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "HuntWork::Serialize";
    HuntWork_Serialize_Impl((HuntWork*)self, stream);
}

void InstallHuntWorkSerialize() {
    HuntWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EF600);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[HuntWork] Serialize hooked at %p (REIMPLEMENTED, prologue=15)\n", (void*)target);
#endif
}
void RemoveHuntWorkSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// Slot4 (0x1400EEFF0) — REIMPLEMENTED (clean C++)
// 326-instruction hunt target assignment logic.
// Uses int return type (bug class 7).
// ============================================================
typedef uint64_t (*HuW_Slot4_t)(void* self, void* param2, uint64_t param3, uint64_t param4);
static HookInfo g_hookSlot4;

static uint64_t Reimpl_Slot4(void* self, void* param2, uint64_t param3, uint64_t param4) {
    g_lastHook = "HuntWork::Slot4";
    return (uint64_t)HuntWork_Slot4_Impl((HuntWork*)self, param2, param3, param4);
}

void InstallHuntWorkSlot4() {
    HuntWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EEFF0);
    // Prologue: MOV [RSP+0x10],RDX(5) + MOV [RSP+0x8],RCX(5) + PUSH RBP(1) + PUSH RSI(1) = 12
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 12);
#ifdef _DEBUG
    printf("[HuntWork] Slot4 hooked at %p (REIMPLEMENTED, prologue=12)\n", (void*)target);
#endif
}
void RemoveHuntWorkSlot4() { RemoveHook(&g_hookSlot4); }

// ============================================================
// Slot6 (0x1400EEFD0) — NOT HOOKABLE
// 20-byte thunk with RIP-relative CALL rel32 at offset 8.
// Trampoline cannot relocate CALL rel32 — wrong target address.
// sub rsp,0x28; add rcx,0x48; call 0x140025520; xor al,al; add rsp,0x28; ret
// Clean implementation in code/Game/Work/huntwork.cpp only.
// ============================================================

// ============================================================
// Slot8 / CancelAll (0x1400EF530) — REIMPLEMENTED
// ============================================================
typedef void (*HuW_Slot8_t)(void* self);
static HookInfo g_hookSlot8;

static void Reimpl_Slot8(void* self) {
    g_lastHook = "HuntWork::Slot8";
    HuntWork_Slot8_Impl((HuntWork*)self);
}

void InstallHuntWorkSlot8() {
    HuntWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EF530);
    // Prologue: PUSH RDI(2) + SUB RSP,0x20(4) + CMP [RCX+0x50],0(4) + MOV RDI,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[HuntWork] Slot8 hooked at %p (REIMPLEMENTED, prologue=13)\n", (void*)target);
#endif
}
void RemoveHuntWorkSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Slot9 / RemoveInvalid (0x1400EED90) — passthrough (DISABLED hook)
// 164 instructions. Hook DISABLED (bug class 3 — MOV RAX,RSP).
// Clean code ready in huntwork.cpp for standalone exe.
// ============================================================
typedef bool (*HuW_Slot9_t)(void* self, void* param2);
static HookInfo g_hookSlot9;

static bool Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "HuntWork::Slot9";
    return ((HuW_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

void InstallHuntWorkSlot9() {
    HuntWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EED90);
    // Prologue: MOV RAX,RSP(3) + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) +
    //           PUSH R12(2) + PUSH R13(2) + PUSH R14(2) = 12
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 12);
#ifdef _DEBUG
    printf("[HuntWork] Slot9 hooked at %p (passthrough/DISABLED, prologue=12)\n", (void*)target);
#endif
}
void RemoveHuntWorkSlot9() { RemoveHook(&g_hookSlot9); }

// ============================================================
// Slot10 / GetWorkEntity (0x1400EF5A0) — REIMPLEMENTED
// ============================================================
typedef void* (*HuW_Slot10_t)(void* self, void* outPtr);
static HookInfo g_hookSlot10;

static void* Reimpl_Slot10(void* self, void* outPtr) {
    g_lastHook = "HuntWork::Slot10";
    return HuntWork_Slot10_Impl((HuntWork*)self, outPtr);
}

void InstallHuntWorkSlot10() {
    HuntWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EF5A0);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 14
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 14);
#ifdef _DEBUG
    printf("[HuntWork] Slot10 hooked at %p (REIMPLEMENTED, prologue=14)\n", (void*)target);
#endif
}
void RemoveHuntWorkSlot10() { RemoveHook(&g_hookSlot10); }
