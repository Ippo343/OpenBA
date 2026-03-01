// engine/reimpl/pasturework.cpp
// Reimplementation of PastureWork methods from Application-x64.exe.
//
// PastureWork — Pasture animal work action (0x58 bytes).
// Inherits: Object::Internal -> Work -> AutoPickupWork -> PastureWork
// Vtable at 0x1401617a0 (12 slots).
// Same field layout as HuntWork (different vtable, different work logic).
//
// Hookable methods (all passthrough):
//   [0] ScalarDtor (0x14002BF70) — calls dtor body 0x1400F2CE0, conditional free
//   [3] Serialize  (0x1400F2C60) — Work::Serialize + tree-set (no extra int)
//   [4] Slot4      (0x1400F2370) — 509 instructions, pasture work assignment logic
//   [6] Slot6      (0x1400F2350) — thunk: sub rsp,0x28 + add rcx,0x48 + call + ret
//   [7] Slot7      (0x1400F22C0) — tree insertion with validation
//   [8] Slot8      (0x1400F2B90) — CancelAll
//   [9] Slot9      (0x1400F2090) — RemoveInvalid (164 instructions)
//  [10] Slot10     (0x1400F2C00) — GetWorkEntity
//
// Too small to hook:
//   [1] VectorDtor (0x14002BEF0) — 8 bytes
//   [2] BaseDtor   (0x14002BEE0) — 7 bytes
//   [5] Execute    (0x1400F7540) — 3 bytes, returns false
//  [11] Slot11     (0x140029860) — 9 bytes, returns (this->field50 > 0)

#include "PastureWork.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// ScalarDtor (0x14002BF70) — passthrough
// ============================================================
typedef void* (*PW_ScalarDtor_t)(void* self, uint32_t flags);
static HookInfo g_hookScalarDtor;

static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "PastureWork::ScalarDtor";
    return ((PW_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

void InstallPastureWorkScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002BF70);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[PastureWork] ScalarDtor hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemovePastureWorkScalarDtor() { RemoveHook(&g_hookScalarDtor); }

// ============================================================
// Serialize (0x1400F2C60) — passthrough
// Work::Serialize + tree-set at +0x48 only.
// ============================================================
typedef void (*PW_Serialize_t)(void* self, void* stream);
static HookInfo g_hookSerialize;

static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "PastureWork::Serialize";
    ((PW_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

void InstallPastureWorkSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400F2C60);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[PastureWork] Serialize hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemovePastureWorkSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// Slot4 (0x1400F2370) — passthrough
// 509 instructions. Pasture work assignment logic.
// ============================================================
typedef uint64_t (*PW_Slot4_t)(void* self, void* param2, uint64_t param3, uint64_t param4);
static HookInfo g_hookSlot4;

static uint64_t Reimpl_Slot4(void* self, void* param2, uint64_t param3, uint64_t param4) {
    g_lastHook = "PastureWork::Slot4";
    return ((PW_Slot4_t)g_hookSlot4.trampoline)(self, param2, param3, param4);
}

void InstallPastureWorkSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400F2370);
    // Prologue: MOV [RSP+0x10],RDX(5) + MOV [RSP+0x8],RCX(5) + PUSH RBP(1) + PUSH RDI(1) = 12
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 12);
#ifdef _DEBUG
    printf("[PastureWork] Slot4 hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemovePastureWorkSlot4() { RemoveHook(&g_hookSlot4); }

// ============================================================
// Slot6 (0x1400F2350) — NOT HOOKABLE
// 20-byte thunk with RIP-relative CALL rel32 at offset 8.
// Trampoline cannot relocate CALL rel32 — wrong target address.
// sub rsp,0x28; add rcx,0x48; call 0x140025520; xor al,al; add rsp,0x28; ret
// ============================================================

// ============================================================
// Slot7 (0x1400F22C0) — passthrough
// Tree insertion with validation. Larger than the 9-byte thunks
// in ForesterWork/HerbalistWork/HuntWork.
// ============================================================
typedef void (*PW_Slot7_t)(void* self, void* param2, void* param3);
static HookInfo g_hookSlot7;

static void Reimpl_Slot7(void* self, void* param2, void* param3) {
    g_lastHook = "PastureWork::Slot7";
    ((PW_Slot7_t)g_hookSlot7.trampoline)(self, param2, param3);
}

void InstallPastureWorkSlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400F22C0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + ADD RCX,0x48(4) + MOV R8,RBX... = 13
    // Actually: 40 53(2) + 48 83 EC 20(4) + 48 83 C1 48(4) + 49 8B D8(3) = 13
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 13);
#ifdef _DEBUG
    printf("[PastureWork] Slot7 hooked at %p (passthrough, prologue=13)\n", (void*)target);
#endif
}
void RemovePastureWorkSlot7() { RemoveHook(&g_hookSlot7); }

// ============================================================
// Slot8 / CancelAll (0x1400F2B90) — passthrough
// ============================================================
typedef void (*PW_Slot8_t)(void* self);
static HookInfo g_hookSlot8;

static void Reimpl_Slot8(void* self) {
    g_lastHook = "PastureWork::Slot8";
    ((PW_Slot8_t)g_hookSlot8.trampoline)(self);
}

void InstallPastureWorkSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400F2B90);
    // Prologue: PUSH RDI(2) + SUB RSP,0x20(4) + CMP [RCX+0x50],0(4) + MOV RDI,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[PastureWork] Slot8 hooked at %p (passthrough, prologue=13)\n", (void*)target);
#endif
}
void RemovePastureWorkSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Slot9 / RemoveInvalid (0x1400F2090) — passthrough
// 164 instructions.
// ============================================================
typedef bool (*PW_Slot9_t)(void* self, void* param2);
static HookInfo g_hookSlot9;

static bool Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "PastureWork::Slot9";
    return ((PW_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

void InstallPastureWorkSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400F2090);
    // Prologue: MOV RAX,RSP(3) + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) +
    //           PUSH R12(2) + PUSH R13(2) + PUSH R14(2) = 12
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 12);
#ifdef _DEBUG
    printf("[PastureWork] Slot9 hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemovePastureWorkSlot9() { RemoveHook(&g_hookSlot9); }

// ============================================================
// Slot10 / GetWorkEntity (0x1400F2C00) — passthrough
// ============================================================
typedef void* (*PW_Slot10_t)(void* self, void* outPtr);
static HookInfo g_hookSlot10;

static void* Reimpl_Slot10(void* self, void* outPtr) {
    g_lastHook = "PastureWork::Slot10";
    return ((PW_Slot10_t)g_hookSlot10.trampoline)(self, outPtr);
}

void InstallPastureWorkSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400F2C00);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 14
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 14);
#ifdef _DEBUG
    printf("[PastureWork] Slot10 hooked at %p (passthrough, prologue=14)\n", (void*)target);
#endif
}
void RemovePastureWorkSlot10() { RemoveHook(&g_hookSlot10); }
