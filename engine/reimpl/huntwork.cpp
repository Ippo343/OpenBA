// engine/reimpl/huntwork.cpp
// Reimplementation of HuntWork methods from Application-x64.exe.
//
// HuntWork — Hunting work action (0x58 bytes).
// Inherits: Object::Internal -> Work -> AutoPickupWork -> HuntWork
// Vtable at 0x140161620 (12 slots).
//
// Hookable methods (all passthrough):
//   [0] ScalarDtor (0x14002A700) — calls dtor body 0x1400EF680, conditional free
//   [3] Serialize  (0x1400EF600) — Work::Serialize + tree-set (no extra int field)
//   [4] Slot4      (0x1400EEFF0) — 326 instructions, hunt target assignment logic
//   [6] Slot6      (0x1400EEFD0) — thunk: sub rsp,0x28 + add rcx,0x48 + call + ret
//   [8] Slot8      (0x1400EF530) — CancelAll
//   [9] Slot9      (0x1400EED90) — RemoveInvalid (164 instructions)
//  [10] Slot10     (0x1400EF5A0) — GetWorkEntity
//
// Too small to hook:
//   [1] VectorDtor (0x14002A680) — 8 bytes
//   [2] BaseDtor   (0x14002A670) — 7 bytes
//   [5] Execute    (0x1400F7540) — 3 bytes, returns false
//   [7] Slot7      (0x1400EEFC0) — 9 bytes, thunk: add rcx,0x48 + jmp
//  [11] Slot11     (0x140029860) — 9 bytes, returns (this->field50 > 0)

#include "HuntWork.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// ScalarDtor (0x14002A700) — passthrough
// ============================================================
typedef void* (*HuW_ScalarDtor_t)(void* self, uint32_t flags);
static HookInfo g_hookScalarDtor;

static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    return ((HuW_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

void InstallHuntWorkScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002A700);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[HuntWork] ScalarDtor hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemoveHuntWorkScalarDtor() { RemoveHook(&g_hookScalarDtor); }

// ============================================================
// Serialize (0x1400EF600) — passthrough
// Work::Serialize + tree-set at +0x48 only (no extra int field like ForesterWork).
// ============================================================
typedef void (*HuW_Serialize_t)(void* self, void* stream);
static HookInfo g_hookSerialize;

static void Reimpl_Serialize(void* self, void* stream) {
    ((HuW_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

void InstallHuntWorkSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EF600);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[HuntWork] Serialize hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveHuntWorkSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// Slot4 (0x1400EEFF0) — passthrough
// 326 instructions. Hunt target assignment logic.
// ============================================================
typedef uint64_t (*HuW_Slot4_t)(void* self, void* param2, uint64_t param3, uint64_t param4);
static HookInfo g_hookSlot4;

static uint64_t Reimpl_Slot4(void* self, void* param2, uint64_t param3, uint64_t param4) {
    return ((HuW_Slot4_t)g_hookSlot4.trampoline)(self, param2, param3, param4);
}

void InstallHuntWorkSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EEFF0);
    // Prologue: MOV [RSP+0x10],RDX(5) + MOV [RSP+0x8],RCX(5) + PUSH RBP(1) + PUSH RSI(1) = 12
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 12);
#ifdef _DEBUG
    printf("[HuntWork] Slot4 hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemoveHuntWorkSlot4() { RemoveHook(&g_hookSlot4); }

// ============================================================
// Slot6 (0x1400EEFD0) — NOT HOOKABLE
// 20-byte thunk with RIP-relative CALL rel32 at offset 8.
// Trampoline cannot relocate CALL rel32 — wrong target address.
// sub rsp,0x28; add rcx,0x48; call 0x140025520; xor al,al; add rsp,0x28; ret
// ============================================================

// ============================================================
// Slot8 / CancelAll (0x1400EF530) — passthrough
// ============================================================
typedef void (*HuW_Slot8_t)(void* self);
static HookInfo g_hookSlot8;

static void Reimpl_Slot8(void* self) {
    ((HuW_Slot8_t)g_hookSlot8.trampoline)(self);
}

void InstallHuntWorkSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EF530);
    // Prologue: PUSH RDI(2) + SUB RSP,0x20(4) + CMP [RCX+0x50],0(4) + MOV RDI,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[HuntWork] Slot8 hooked at %p (passthrough, prologue=13)\n", (void*)target);
#endif
}
void RemoveHuntWorkSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Slot9 / RemoveInvalid (0x1400EED90) — passthrough
// 164 instructions.
// ============================================================
typedef bool (*HuW_Slot9_t)(void* self, void* param2);
static HookInfo g_hookSlot9;

static bool Reimpl_Slot9(void* self, void* param2) {
    return ((HuW_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

void InstallHuntWorkSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EED90);
    // Prologue: MOV RAX,RSP(3) + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) +
    //           PUSH R12(2) + PUSH R13(2) + PUSH R14(2) = 12
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 12);
#ifdef _DEBUG
    printf("[HuntWork] Slot9 hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemoveHuntWorkSlot9() { RemoveHook(&g_hookSlot9); }

// ============================================================
// Slot10 / GetWorkEntity (0x1400EF5A0) — passthrough
// ============================================================
typedef void* (*HuW_Slot10_t)(void* self, void* outPtr);
static HookInfo g_hookSlot10;

static void* Reimpl_Slot10(void* self, void* outPtr) {
    return ((HuW_Slot10_t)g_hookSlot10.trampoline)(self, outPtr);
}

void InstallHuntWorkSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EF5A0);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 14
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 14);
#ifdef _DEBUG
    printf("[HuntWork] Slot10 hooked at %p (passthrough, prologue=14)\n", (void*)target);
#endif
}
void RemoveHuntWorkSlot10() { RemoveHook(&g_hookSlot10); }
