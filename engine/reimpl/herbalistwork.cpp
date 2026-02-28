// engine/reimpl/herbalistwork.cpp
// Reimplementation of HerbalistWork methods from Application-x64.exe.
//
// HerbalistWork — Herbalist gathering work action (0x60 bytes).
// Inherits: Object::Internal -> Work -> AutoPickupWork -> HerbalistWork
// Vtable at 0x1401615a0 (12 slots).
// Same field layout as ForesterWork (different vtable, different work logic).
//
// Hookable methods (all passthrough):
//   [0] ScalarDtor (0x14002A640) — calls dtor body 0x1400EEC40, conditional free
//   [3] Serialize  (0x1400EEBB0) — same structure as ForesterWork (tree-set + int)
//   [4] Slot4      (0x1400EE3B0) — 457 instructions, herb gathering assignment logic
//   [6] Slot6      (0x1400EE390) — thunk: sub rsp,0x28 + add rcx,0x48 + call + ret
//   [8] Slot8      (0x1400EEAE0) — CancelAll
//   [9] Slot9      (0x1400EE150) — RemoveInvalid (164 instructions)
//  [10] Slot10     (0x1400EEB50) — GetWorkEntity
//
// Too small to hook:
//   [1] VectorDtor (0x14002A5D0) — 8 bytes
//   [2] BaseDtor   (0x14002A5C0) — 7 bytes
//   [5] Execute    (0x1400F7540) — 3 bytes, returns false
//   [7] Slot7      (0x1400EE380) — 9 bytes, thunk: add rcx,0x48 + jmp
//  [11] Slot11     (0x140029860) — 9 bytes, returns (this->field50 > 0)

#include "HerbalistWork.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// ScalarDtor (0x14002A640) — passthrough
// ============================================================
typedef void* (*HW_ScalarDtor_t)(void* self, uint32_t flags);
static HookInfo g_hookScalarDtor;

static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    return ((HW_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

void InstallHerbalistWorkScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002A640);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[HerbalistWork] ScalarDtor hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemoveHerbalistWorkScalarDtor() { RemoveHook(&g_hookScalarDtor); }

// ============================================================
// Serialize (0x1400EEBB0) — passthrough
// ============================================================
typedef void (*HW_Serialize_t)(void* self, void* stream);
static HookInfo g_hookSerialize;

static void Reimpl_Serialize(void* self, void* stream) {
    ((HW_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

void InstallHerbalistWorkSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EEBB0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[HerbalistWork] Serialize hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveHerbalistWorkSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// Slot4 (0x1400EE3B0) — passthrough
// 457 instructions. Herb gathering assignment logic.
// ============================================================
typedef uint64_t (*HW_Slot4_t)(void* self, void* param2, uint64_t param3, uint64_t param4);
static HookInfo g_hookSlot4;

static uint64_t Reimpl_Slot4(void* self, void* param2, uint64_t param3, uint64_t param4) {
    return ((HW_Slot4_t)g_hookSlot4.trampoline)(self, param2, param3, param4);
}

void InstallHerbalistWorkSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EE3B0);
    // Prologue: MOV [RSP+0x10],RDX(5) + MOV [RSP+0x8],RCX(5) + PUSH RBP(1) + PUSH RDI(1) = 12
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 12);
#ifdef _DEBUG
    printf("[HerbalistWork] Slot4 hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemoveHerbalistWorkSlot4() { RemoveHook(&g_hookSlot4); }

// ============================================================
// Slot6 (0x1400EE390) — NOT HOOKABLE
// 20-byte thunk with RIP-relative CALL rel32 at offset 8.
// Trampoline cannot relocate CALL rel32 — wrong target address.
// sub rsp,0x28; add rcx,0x48; call 0x140025520; xor al,al; add rsp,0x28; ret
// ============================================================

// ============================================================
// Slot8 / CancelAll (0x1400EEAE0) — passthrough
// ============================================================
typedef void (*HW_Slot8_t)(void* self);
static HookInfo g_hookSlot8;

static void Reimpl_Slot8(void* self) {
    ((HW_Slot8_t)g_hookSlot8.trampoline)(self);
}

void InstallHerbalistWorkSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EEAE0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x20(4) + CMP [RCX+0x50],0(4) + MOV RDI,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[HerbalistWork] Slot8 hooked at %p (passthrough, prologue=13)\n", (void*)target);
#endif
}
void RemoveHerbalistWorkSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Slot9 / RemoveInvalid (0x1400EE150) — passthrough
// 164 instructions.
// ============================================================
typedef bool (*HW_Slot9_t)(void* self, void* param2);
static HookInfo g_hookSlot9;

static bool Reimpl_Slot9(void* self, void* param2) {
    return ((HW_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

void InstallHerbalistWorkSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EE150);
    // Prologue: MOV RAX,RSP(3) + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) +
    //           PUSH R12(2) + PUSH R13(2) + PUSH R14(2) = 12
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 12);
#ifdef _DEBUG
    printf("[HerbalistWork] Slot9 hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemoveHerbalistWorkSlot9() { RemoveHook(&g_hookSlot9); }

// ============================================================
// Slot10 / GetWorkEntity (0x1400EEB50) — passthrough
// ============================================================
typedef void* (*HW_Slot10_t)(void* self, void* outPtr);
static HookInfo g_hookSlot10;

static void* Reimpl_Slot10(void* self, void* outPtr) {
    return ((HW_Slot10_t)g_hookSlot10.trampoline)(self, outPtr);
}

void InstallHerbalistWorkSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EEB50);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 14
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 14);
#ifdef _DEBUG
    printf("[HerbalistWork] Slot10 hooked at %p (passthrough, prologue=14)\n", (void*)target);
#endif
}
void RemoveHerbalistWorkSlot10() { RemoveHook(&g_hookSlot10); }
