// engine/reimpl/herbalistwork.cpp
// Reimplementation of HerbalistWork methods from Application-x64.exe.
//
// HerbalistWork — Herbalist gathering work action (0x60 bytes).
// Inherits: Object::Internal -> Work -> AutoPickupWork -> HerbalistWork
// Vtable at 0x1401615a0 (12 slots).
// Same field layout as ForesterWork (different vtable, different work logic).
//
// REIMPLEMENTED methods (clean C++ in code/Game/Work/herbalistwork.cpp):
//   [0] ScalarDtor (0x14002A640) — calls dtor body 0x1400EEC40, conditional free
//   [3] Serialize  (0x1400EEBB0) — Work::Serialize + tree-set + int
//   [4] Slot4      (0x1400EE3B0) — 457 instructions, herb gathering assignment logic
//   [8] Slot8      (0x1400EEAE0) — CancelAll
//   [9] Slot9      (0x1400EE150) — RemoveInvalid (164 instructions, hook DISABLED)
//  [10] Slot10     (0x1400EEB50) — GetWorkEntity
//
// NOT HOOKABLE (clean code in herbalistwork.cpp only, no hook wrapper):
//   [6] Slot6      (0x1400EE390) — thunk: sub rsp,0x28 + add rcx,0x48 + call + ret
//
// Too small to hook (clean code in herbalistwork.cpp only):
//   [1] VectorDtor (0x14002A5D0) — 8 bytes
//   [2] BaseDtor   (0x14002A5C0) — 7 bytes
//   [5] Execute    (0x1400F7540) — 3 bytes, returns false
//   [7] Slot7      (0x1400EE380) — 9 bytes, thunk: add rcx,0x48 + jmp
//  [11] Slot11     (0x140029860) — 9 bytes, returns (this->field50 > 0)

#include "HerbalistWork.h"
#include "Work/herbalistwork_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// ScalarDtor (0x14002A640) — REIMPLEMENTED
// ============================================================
typedef void* (*HW_ScalarDtor_t)(void* self, uint32_t flags);
static HookInfo g_hookScalarDtor;

static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "HerbalistWork::ScalarDtor";
    return HerbalistWork_ScalarDtor_Impl((HerbalistWork*)self, flags);
}

void InstallHerbalistWorkScalarDtor() {
    HerbalistWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002A640);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[HerbalistWork] ScalarDtor hooked at %p (REIMPLEMENTED, prologue=12)\n", (void*)target);
#endif
}
void RemoveHerbalistWorkScalarDtor() { RemoveHook(&g_hookScalarDtor); }

// ============================================================
// Serialize (0x1400EEBB0) — REIMPLEMENTED
// ============================================================
typedef void (*HW_Serialize_t)(void* self, void* stream);
static HookInfo g_hookSerialize;

static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "HerbalistWork::Serialize";
    HerbalistWork_Serialize_Impl((HerbalistWork*)self, stream);
}

void InstallHerbalistWorkSerialize() {
    HerbalistWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EEBB0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[HerbalistWork] Serialize hooked at %p (REIMPLEMENTED, prologue=15)\n", (void*)target);
#endif
}
void RemoveHerbalistWorkSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// Slot4 (0x1400EE3B0) — REIMPLEMENTED (clean C++)
// 457-instruction herb gathering assignment logic.
// Uses int return type (bug class 7).
// ============================================================
typedef uint64_t (*HW_Slot4_t)(void* self, void* param2, uint64_t param3, uint64_t param4);
static HookInfo g_hookSlot4;

static uint64_t Reimpl_Slot4(void* self, void* param2, uint64_t param3, uint64_t param4) {
    g_lastHook = "HerbalistWork::Slot4";
    return (uint64_t)HerbalistWork_Slot4_Impl((HerbalistWork*)self, param2, param3, param4);
}

void InstallHerbalistWorkSlot4() {
    HerbalistWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EE3B0);
    // Prologue: MOV [RSP+0x10],RDX(5) + MOV [RSP+0x8],RCX(5) + PUSH RBP(1) + PUSH RDI(1) = 12
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 12);
#ifdef _DEBUG
    printf("[HerbalistWork] Slot4 hooked at %p (REIMPLEMENTED, prologue=12)\n", (void*)target);
#endif
}
void RemoveHerbalistWorkSlot4() { RemoveHook(&g_hookSlot4); }

// ============================================================
// Slot6 (0x1400EE390) — NOT HOOKABLE
// 20-byte thunk with RIP-relative CALL rel32 at offset 8.
// Trampoline cannot relocate CALL rel32 — wrong target address.
// sub rsp,0x28; add rcx,0x48; call 0x140025520; xor al,al; add rsp,0x28; ret
// Clean implementation in code/Game/Work/herbalistwork.cpp only.
// ============================================================

// ============================================================
// Slot8 / CancelAll (0x1400EEAE0) — REIMPLEMENTED
// ============================================================
typedef int (*HW_Slot8_t)(void* self);
static HookInfo g_hookSlot8;

static int Reimpl_Slot8(void* self) {
    g_lastHook = "HerbalistWork::Slot8";
    return HerbalistWork_Slot8_Impl((HerbalistWork*)self);
}

void InstallHerbalistWorkSlot8() {
    HerbalistWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EEAE0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x20(4) + CMP [RCX+0x50],0(4) + MOV RDI,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[HerbalistWork] Slot8 hooked at %p (REIMPLEMENTED, prologue=13)\n", (void*)target);
#endif
}
void RemoveHerbalistWorkSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Slot9 / RemoveInvalid (0x1400EE150) — REIMPLEMENTED
// 164 instructions. Hook DISABLED (bug class 2 — MOV RAX,RSP).
// Clean code ready for when hook becomes usable or standalone exe.
// ============================================================
typedef bool (*HW_Slot9_t)(void* self, void* param2);
static HookInfo g_hookSlot9;

static bool Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "HerbalistWork::Slot9";
    return HerbalistWork_Slot9_Impl((HerbalistWork*)self, param2);
}

void InstallHerbalistWorkSlot9() {
    HerbalistWork_ResolveFuncs();
    // VTable hook: MOV RAX,RSP in prologue breaks with RSP compensation.
    // vtable at 0x1401615a0, Slot9 = slot 9 (+0x48)
    InstallHookVTable(ResolveAddr(Module::Exe, 0x1401615a0), 9, (void*)Reimpl_Slot9, &g_hookSlot9);
    RegisterHookInfo("HerbalistWork::Slot9", &g_hookSlot9);
#ifdef _DEBUG
    printf("[HerbalistWork] Slot9 vtable-hooked (original=%p)\n", g_hookSlot9.trampoline);
#endif
}
void RemoveHerbalistWorkSlot9() { RemoveHookVTable(&g_hookSlot9); }

// ============================================================
// Slot10 / GetWorkEntity (0x1400EEB50) — REIMPLEMENTED
// ============================================================
typedef void* (*HW_Slot10_t)(void* self, void* outPtr);
static HookInfo g_hookSlot10;

static void* Reimpl_Slot10(void* self, void* outPtr) {
    g_lastHook = "HerbalistWork::Slot10";
    return HerbalistWork_Slot10_Impl((HerbalistWork*)self, outPtr);
}

void InstallHerbalistWorkSlot10() {
    HerbalistWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EEB50);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 14
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 14);
#ifdef _DEBUG
    printf("[HerbalistWork] Slot10 hooked at %p (REIMPLEMENTED, prologue=14)\n", (void*)target);
#endif
}
void RemoveHerbalistWorkSlot10() { RemoveHook(&g_hookSlot10); }
