// engine/reimpl/pasturework.cpp
// Reimplementation of PastureWork methods from Application-x64.exe.
//
// PastureWork — Pasture animal work action (0x58 bytes).
// Inherits: Object::Internal -> Work -> AutoPickupWork -> PastureWork
// Vtable at 0x1401617a0 (12 slots).
// Same field layout as HuntWork (different vtable, different work logic).
//
// REIMPLEMENTED methods (clean C++ in code/Game/Work/pasturework.cpp):
//   [0] ScalarDtor (0x14002BF70) — calls dtor body 0x1400F2CE0, conditional free
//   [3] Serialize  (0x1400F2C60) — Work::Serialize + tree-set (no extra int)
//   [4] Slot4      (0x1400F2370) — 509 instructions, pasture work assignment logic
//   [7] Slot7      (0x1400F22C0) — tree insertion with validation
//   [8] Slot8      (0x1400F2B90) — CancelAll
//  [10] Slot10     (0x1400F2C00) — GetWorkEntity
//
// DISABLED (hook has MOV RAX,RSP — bug class 3, clean code only):
//   [9] Slot9      (0x1400F2090) — RemoveInvalid (passthrough via trampoline)
//
// NOT HOOKABLE (clean code in pasturework.cpp only, no hook wrapper):
//   [6] Slot6      (0x1400F2350) — thunk: sub rsp,0x28 + add rcx,0x48 + call + ret
//
// Too small to hook (clean code in pasturework.cpp only):
//   [1] VectorDtor (0x14002BEF0) — 8 bytes
//   [2] BaseDtor   (0x14002BEE0) — 7 bytes
//   [5] Execute    (0x1400F7540) — 3 bytes, returns false
//  [11] Slot11     (0x140029860) — 9 bytes, returns (this->field50 > 0)

#include "PastureWork.h"
#include "Work/pasturework_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// ScalarDtor (0x14002BF70) — REIMPLEMENTED
// ============================================================
typedef void* (*PW_ScalarDtor_t)(void* self, uint32_t flags);
static HookInfo g_hookScalarDtor;

static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "PastureWork::ScalarDtor";
    return PastureWork_ScalarDtor_Impl((PastureWork*)self, flags);
}

void InstallPastureWorkScalarDtor() {
    PastureWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002BF70);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[PastureWork] ScalarDtor hooked at %p (REIMPLEMENTED, prologue=12)\n", (void*)target);
#endif
}
void RemovePastureWorkScalarDtor() { RemoveHook(&g_hookScalarDtor); }

// ============================================================
// Serialize (0x1400F2C60) — REIMPLEMENTED
// Work::Serialize + tree-set at +0x48 only.
// ============================================================
typedef void (*PW_Serialize_t)(void* self, void* stream);
static HookInfo g_hookSerialize;

static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "PastureWork::Serialize";
    PastureWork_Serialize_Impl((PastureWork*)self, stream);
}

void InstallPastureWorkSerialize() {
    PastureWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400F2C60);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[PastureWork] Serialize hooked at %p (REIMPLEMENTED, prologue=15)\n", (void*)target);
#endif
}
void RemovePastureWorkSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// Slot4 (0x1400F2370) — REIMPLEMENTED (clean C++)
// 509-instruction pasture work assignment logic.
// Uses int return type (bug class 7).
// ============================================================
typedef uint64_t (*PW_Slot4_t)(void* self, void* param2, uint64_t param3, uint64_t param4);
static HookInfo g_hookSlot4;

static uint64_t Reimpl_Slot4(void* self, void* param2, uint64_t param3, uint64_t param4) {
    g_lastHook = "PastureWork::Slot4";
    return (uint64_t)PastureWork_Slot4_Impl((PastureWork*)self, param2, param3, param4);
}

void InstallPastureWorkSlot4() {
    PastureWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400F2370);
    // Prologue: MOV [RSP+0x10],RDX(5) + MOV [RSP+0x8],RCX(5) + PUSH RBP(1) + PUSH RDI(1) = 12
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 12);
#ifdef _DEBUG
    printf("[PastureWork] Slot4 hooked at %p (REIMPLEMENTED, prologue=12)\n", (void*)target);
#endif
}
void RemovePastureWorkSlot4() { RemoveHook(&g_hookSlot4); }

// ============================================================
// Slot6 (0x1400F2350) — NOT HOOKABLE
// 20-byte thunk with RIP-relative CALL rel32 at offset 8.
// Trampoline cannot relocate CALL rel32 — wrong target address.
// sub rsp,0x28; add rcx,0x48; call 0x140025520; xor al,al; add rsp,0x28; ret
// Clean implementation in code/Game/Work/pasturework.cpp only.
// ============================================================

// ============================================================
// Slot7 (0x1400F22C0) — REIMPLEMENTED
// Tree insertion with validation. Larger than the 9-byte thunks
// in ForesterWork/HerbalistWork/HuntWork.
// ============================================================
typedef int (*PW_Slot7_t)(void* self, void* param2, void* param3);
static HookInfo g_hookSlot7;

static int Reimpl_Slot7(void* self, void* param2, void* param3) {
    g_lastHook = "PastureWork::Slot7";
    return PastureWork_Slot7_Impl((PastureWork*)self, param2, param3);
}

void InstallPastureWorkSlot7() {
    PastureWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400F22C0);
    // Prologue: 40 53(2) + 48 83 EC 20(4) + 48 83 C1 48(4) + 49 8B D8(3) = 13
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 13);
#ifdef _DEBUG
    printf("[PastureWork] Slot7 hooked at %p (REIMPLEMENTED, prologue=13)\n", (void*)target);
#endif
}
void RemovePastureWorkSlot7() { RemoveHook(&g_hookSlot7); }

// ============================================================
// Slot8 / CancelAll (0x1400F2B90) — REIMPLEMENTED
// ============================================================
typedef int (*PW_Slot8_t)(void* self);
static HookInfo g_hookSlot8;

static int Reimpl_Slot8(void* self) {
    g_lastHook = "PastureWork::Slot8";
    return PastureWork_Slot8_Impl((PastureWork*)self);
}

void InstallPastureWorkSlot8() {
    PastureWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400F2B90);
    // Prologue: PUSH RDI(2) + SUB RSP,0x20(4) + CMP [RCX+0x50],0(4) + MOV RDI,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[PastureWork] Slot8 hooked at %p (REIMPLEMENTED, prologue=13)\n", (void*)target);
#endif
}
void RemovePastureWorkSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Slot9 / RemoveInvalid (0x1400F2090) — passthrough (DISABLED hook)
// 164 instructions. Hook DISABLED (bug class 3 — MOV RAX,RSP).
// Clean code ready in pasturework.cpp for standalone exe.
// ============================================================
typedef bool (*PW_Slot9_t)(void* self, void* param2);
static HookInfo g_hookSlot9;

static bool Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "PastureWork::Slot9";
    return ((PW_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

void InstallPastureWorkSlot9() {
    PastureWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400F2090);
    // Prologue: MOV RAX,RSP(3) + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) +
    //           PUSH R12(2) + PUSH R13(2) + PUSH R14(2) = 12
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 12);
#ifdef _DEBUG
    printf("[PastureWork] Slot9 hooked at %p (passthrough/DISABLED, prologue=12)\n", (void*)target);
#endif
}
void RemovePastureWorkSlot9() { RemoveHook(&g_hookSlot9); }

// ============================================================
// Slot10 / GetWorkEntity (0x1400F2C00) — REIMPLEMENTED
// ============================================================
typedef void* (*PW_Slot10_t)(void* self, void* outPtr);
static HookInfo g_hookSlot10;

static void* Reimpl_Slot10(void* self, void* outPtr) {
    g_lastHook = "PastureWork::Slot10";
    return PastureWork_Slot10_Impl((PastureWork*)self, outPtr);
}

void InstallPastureWorkSlot10() {
    PastureWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400F2C00);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 14
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 14);
#ifdef _DEBUG
    printf("[PastureWork] Slot10 hooked at %p (REIMPLEMENTED, prologue=14)\n", (void*)target);
#endif
}
void RemovePastureWorkSlot10() { RemoveHook(&g_hookSlot10); }
