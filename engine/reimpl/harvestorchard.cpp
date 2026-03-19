// engine/reimpl/harvestorchard.cpp
// Reimplementation of HarvestOrchard methods from Application-x64.exe.
//
// HarvestOrchard — Orchard harvest work action (0x68 bytes).
// Inherits: Object::Internal -> Work -> AutoPickupWork -> HarvestOrchard
// Vtable at 0x140161520 (12 slots).
// Same field layout as HarvestArea (different vtable, different work logic).
//
// Hookable methods (REIMPLEMENTED — clean C++ in code/Game/Work/harvestorchard.cpp):
//   [0] ScalarDtor (0x14002A590) — calls dtor body 0x1400EDF60, conditional free
//   [3] Serialize  (0x1400EDEA0) — identical structure to HarvestArea::Serialize
//   [4] Slot4      (0x1400ED8C0) — 303 instructions, orchard harvest assignment logic
//   [6] Slot6      (0x1400ED880) — tree insert/remove (single int key)
//   [7] Slot7      (0x1400ED810) — tree node lookup/insertion
//   [8] Slot8      (0x1400EDDC0) — CancelAll
//   [9] Slot9      (0x1400ED5E0) — RemoveInvalid (hook DISABLED, bug class 2)
//  [10] Slot10     (0x1400EDE40) — GetWorkEntity
//
// Too small to hook:
//   [1] VectorDtor (0x14002A510) — 8 bytes
//   [2] BaseDtor   (0x14002A500) — 7 bytes
//   [5] Execute    (0x1400F7540) — 3 bytes, returns false
//  [11] Slot11     (0x140029860) — 9 bytes, returns (this->field50 > 0)

#include "HarvestOrchard.h"
#include "Work/harvestorchard_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// ScalarDtor (0x14002A590) — REIMPLEMENTED
// ============================================================
typedef void* (*HO_ScalarDtor_t)(void* self, uint32_t flags);
static HookInfo g_hookScalarDtor;

static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "HarvestOrchard::ScalarDtor";
    return HarvestOrchard_ScalarDtor_Impl((HarvestOrchard*)self, flags);
}

void InstallHarvestOrchardScalarDtor() {
    HarvestOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002A590);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[HarvestOrchard] ScalarDtor hooked at %p (REIMPLEMENTED, prologue=12)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardScalarDtor() { RemoveHook(&g_hookScalarDtor); }

// ============================================================
// Serialize (0x1400EDEA0) — REIMPLEMENTED
// Same structure as HarvestArea: tree-set + array + int fields.
// ============================================================
typedef void (*HO_Serialize_t)(void* self, void* stream);
static HookInfo g_hookSerialize;

static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "HarvestOrchard::Serialize";
    HarvestOrchard_Serialize_Impl((HarvestOrchard*)self, stream);
}

void InstallHarvestOrchardSerialize() {
    HarvestOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EDEA0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[HarvestOrchard] Serialize hooked at %p (REIMPLEMENTED, prologue=15)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// Slot4 (0x1400ED8C0) — REIMPLEMENTED
// 303 instructions. Orchard harvest assignment logic.
// Uses int return type (bug class 7).
// ============================================================
typedef uint64_t (*HO_Slot4_t)(void* self, void* param2, uint64_t param3, uint64_t param4);
static HookInfo g_hookSlot4;

static uint64_t Reimpl_Slot4(void* self, void* param2, uint64_t param3, uint64_t param4) {
    g_lastHook = "HarvestOrchard::Slot4";
    return (uint64_t)HarvestOrchard_Slot4_Impl((HarvestOrchard*)self, param2, param3, param4);
}

void InstallHarvestOrchardSlot4() {
    HarvestOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ED8C0);
    // Prologue: PUSH RBP(1) + PUSH R13(2) + PUSH R14(2) + LEA RBP,[RSP-0x47](5) + SUB RSP,0xd0(7) = 17
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 18);
#ifdef _DEBUG
    printf("[HarvestOrchard] Slot4 hooked at %p (REIMPLEMENTED, prologue=17)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSlot4() { RemoveHook(&g_hookSlot4); }

// ============================================================
// Slot6 (0x1400ED880) — REIMPLEMENTED
// Tree insert/remove with single int key.
// ============================================================
typedef void (*HO_Slot6_t)(void* self, void* param2, void* param3, int param4);
static HookInfo g_hookSlot6;

static void Reimpl_Slot6(void* self, void* param2, void* param3, int param4) {
    g_lastHook = "HarvestOrchard::Slot6";
    HarvestOrchard_Slot6_Impl((HarvestOrchard*)self, param2, param3, param4);
}

void InstallHarvestOrchardSlot6() {
    HarvestOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ED880);
    // Prologue: SUB RSP,0x28(4) + MOV RDX,[RCX+0x48](4) + ADD RCX,0x48(4) = 12
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 12);
#ifdef _DEBUG
    printf("[HarvestOrchard] Slot6 hooked at %p (REIMPLEMENTED, prologue=12)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSlot6() { RemoveHook(&g_hookSlot6); }

// ============================================================
// Slot7 (0x1400ED810) — REIMPLEMENTED
// Tree node lookup/insertion with validation.
// ============================================================
typedef void (*HO_Slot7_t)(void* self, void* param2, void* param3, int param4);
static HookInfo g_hookSlot7;

static void Reimpl_Slot7(void* self, void* param2, void* param3, int param4) {
    g_lastHook = "HarvestOrchard::Slot7";
    HarvestOrchard_Slot7_Impl((HarvestOrchard*)self, param2, param3, param4);
}

void InstallHarvestOrchardSlot7() {
    HarvestOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ED810);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV RDX,[RCX+0x48](4) = 14
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 14);
#ifdef _DEBUG
    printf("[HarvestOrchard] Slot7 hooked at %p (REIMPLEMENTED, prologue=14)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSlot7() { RemoveHook(&g_hookSlot7); }

// ============================================================
// Slot8 / CancelAll (0x1400EDDC0) — REIMPLEMENTED
// ============================================================
typedef void (*HO_Slot8_t)(void* self);
static HookInfo g_hookSlot8;

static void Reimpl_Slot8(void* self) {
    g_lastHook = "HarvestOrchard::Slot8";
    HarvestOrchard_Slot8_Impl((HarvestOrchard*)self);
}

void InstallHarvestOrchardSlot8() {
    HarvestOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EDDC0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x20(4) + CMP [RCX+0x50],0(4) + MOV RDI,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[HarvestOrchard] Slot8 hooked at %p (REIMPLEMENTED, prologue=13)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Slot9 / RemoveInvalid (0x1400ED5E0) — REIMPLEMENTED
// Hook DISABLED in hooks.cpp (bug class 2: MOV RAX,RSP in prologue).
// Clean C++ implemented for standalone exe.
// ============================================================
typedef int (*HO_Slot9_t)(void* self, void* param2);
static HookInfo g_hookSlot9;

static int Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "HarvestOrchard::Slot9";
    return HarvestOrchard_Slot9_Impl((HarvestOrchard*)self, param2);
}

void InstallHarvestOrchardSlot9() {
    HarvestOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ED5E0);
    // Prologue: MOV RAX,RSP(3) + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) +
    //           PUSH R12(2) + PUSH R13(2) + PUSH R14(2) = 12
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 12);
#ifdef _DEBUG
    printf("[HarvestOrchard] Slot9 hooked at %p (REIMPLEMENTED, prologue=12)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSlot9() { RemoveHook(&g_hookSlot9); }

// ============================================================
// Slot10 / GetWorkEntity (0x1400EDE40) — REIMPLEMENTED
// ============================================================
typedef void* (*HO_Slot10_t)(void* self, void* outPtr);
static HookInfo g_hookSlot10;

static void* Reimpl_Slot10(void* self, void* outPtr) {
    g_lastHook = "HarvestOrchard::Slot10";
    return HarvestOrchard_Slot10_Impl((HarvestOrchard*)self, outPtr);
}

void InstallHarvestOrchardSlot10() {
    HarvestOrchard_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EDE40);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 14
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 14);
#ifdef _DEBUG
    printf("[HarvestOrchard] Slot10 hooked at %p (REIMPLEMENTED, prologue=14)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSlot10() { RemoveHook(&g_hookSlot10); }
