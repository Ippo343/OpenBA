// engine/reimpl/foresterwork.cpp
// Reimplementation of ForesterWork methods from Application-x64.exe.
//
// ForesterWork — Forester resource gathering work action (0x60 bytes).
// Inherits: Object::Internal -> Work -> AutoPickupWork -> ForesterWork
// Vtable at 0x1401613a0 (12 slots).
//
// Hookable methods (all passthrough — complex game logic):
//   [0] ScalarDtor (0x14002A370) — calls dtor body 0x1400EBA00, conditional free
//   [3] Serialize  (0x1400EB970) — Work::Serialize + tree-set + int field serialization
//   [4] Slot4      (0x1400EAEC0) — 599 instructions, main work-assignment logic
//   [6] Slot6      (0x1400EAEA0) — 15 bytes, thunk: sub rsp,0x28 + add rcx,0x48 + call + ret
//   [8] Slot8      (0x1400EB8A0) — CancelAll: loops tree, cancels each work item
//   [9] Slot9      (0x1400EAC60) — RemoveInvalid: validates tree entries, removes dead ones
//  [10] Slot10     (0x1400EB910) — GetWorkEntity: returns entity ptr from work's entity ref
//
// Too small to hook (documented only):
//   [1] VectorDtor (0x14002A300) — 8 bytes
//   [2] BaseDtor   (0x14002A2F0) — 7 bytes
//   [5] Execute    (0x1400F7540) — 3 bytes, returns false
//   [7] Slot7      (0x1400EAE90) — 9 bytes, thunk: add rcx,0x48 + jmp
//  [11] Slot11     (0x140029860) — 9 bytes, returns (this->field50 > 0)

#include "ForesterWork.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// ScalarDtor (0x14002A370) — passthrough
// Calls FUN_1400EBA00 (dtor body: sets vtables down chain,
// frees tree nodes at +0x48, then Work/Internal dtor).
// If (flags & 1), frees memory.
// ============================================================
typedef void* (*FW_ScalarDtor_t)(void* self, uint32_t flags);
static HookInfo g_hookScalarDtor;

static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    return ((FW_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

void InstallForesterWorkScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002A370);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[ForesterWork] ScalarDtor hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemoveForesterWorkScalarDtor() { RemoveHook(&g_hookScalarDtor); }

// ============================================================
// Serialize (0x1400EB970) — passthrough
// Calls Work::Serialize, creates version block, serializes
// tree-set at +0x48 (via FUN_1400296b0) and int at +0x58.
// ============================================================
typedef void (*FW_Serialize_t)(void* self, void* stream);
static HookInfo g_hookSerialize;

static void Reimpl_Serialize(void* self, void* stream) {
    ((FW_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

void InstallForesterWorkSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EB970);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[ForesterWork] Serialize hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveForesterWorkSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// Slot4 (0x1400EAEC0) — passthrough
// 599-instruction main work-assignment logic. Finds nearby trees,
// checks terrain, assigns forester to cut/plant tree tasks.
// Far too complex for reimpl.
// ============================================================
typedef uint64_t (*FW_Slot4_t)(void* self, void* param2, uint64_t param3, uint64_t param4);
static HookInfo g_hookSlot4;

static uint64_t Reimpl_Slot4(void* self, void* param2, uint64_t param3, uint64_t param4) {
    return ((FW_Slot4_t)g_hookSlot4.trampoline)(self, param2, param3, param4);
}

void InstallForesterWorkSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EAEC0);
    // Prologue: MOV [RSP+0x10],RDX(5) + MOV [RSP+0x8],RCX(5) + PUSH RBP(1) + PUSH R13(2) = 13
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[ForesterWork] Slot4 hooked at %p (passthrough, prologue=13)\n", (void*)target);
#endif
}
void RemoveForesterWorkSlot4() { RemoveHook(&g_hookSlot4); }

// ============================================================
// Slot6 (0x1400EAEA0) — NOT HOOKABLE
// 20-byte thunk with RIP-relative CALL rel32 at offset 8.
// Trampoline cannot relocate CALL rel32 — wrong target address.
// sub rsp,0x28; add rcx,0x48; call 0x140025520; xor al,al; add rsp,0x28; ret
// ============================================================

// ============================================================
// Slot8 / CancelAll (0x1400EB8A0) — passthrough
// Loops through tree at +0x48 while count (+0x50) > 0.
// For each rightmost node, cancels the work item via engine call.
// ============================================================
typedef void (*FW_Slot8_t)(void* self);
static HookInfo g_hookSlot8;

static void Reimpl_Slot8(void* self) {
    ((FW_Slot8_t)g_hookSlot8.trampoline)(self);
}

void InstallForesterWorkSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EB8A0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x20(4) + CMP [RCX+0x50],0(4) + MOV RDI,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[ForesterWork] Slot8 hooked at %p (passthrough, prologue=13)\n", (void*)target);
#endif
}
void RemoveForesterWorkSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Slot9 / RemoveInvalid (0x1400EAC60) — passthrough
// 164 instructions. Iterates tree, validates each entry via
// FUN_1400F7100, collects invalid ones, removes them from tree.
// ============================================================
typedef bool (*FW_Slot9_t)(void* self, void* param2);
static HookInfo g_hookSlot9;

static bool Reimpl_Slot9(void* self, void* param2) {
    return ((FW_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

void InstallForesterWorkSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EAC60);
    // Prologue: MOV RAX,RSP(3) + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) +
    //           PUSH R12(2) + PUSH R13(2) + PUSH R14(2) = 12
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 12);
#ifdef _DEBUG
    printf("[ForesterWork] Slot9 hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemoveForesterWorkSlot9() { RemoveHook(&g_hookSlot9); }

// ============================================================
// Slot10 / GetWorkEntity (0x1400EB910) — passthrough
// Looks up entity from this->_field10, gets sub-object at +0x48,
// returns it via output pointer (MSVC hidden return ptr pattern).
// ============================================================
typedef void* (*FW_Slot10_t)(void* self, void* outPtr);
static HookInfo g_hookSlot10;

static void* Reimpl_Slot10(void* self, void* outPtr) {
    return ((FW_Slot10_t)g_hookSlot10.trampoline)(self, outPtr);
}

void InstallForesterWorkSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EB910);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 14
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 14);
#ifdef _DEBUG
    printf("[ForesterWork] Slot10 hooked at %p (passthrough, prologue=14)\n", (void*)target);
#endif
}
void RemoveForesterWorkSlot10() { RemoveHook(&g_hookSlot10); }
