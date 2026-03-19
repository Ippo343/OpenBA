// engine/reimpl/harvestarea.cpp
// Reimplementation of HarvestArea methods from Application-x64.exe.
//
// HarvestArea — Crop/field harvest work action (0x68 bytes).
// Inherits: Object::Internal -> Work -> AutoPickupWork -> HarvestArea
// Vtable at 0x1401614a0 (12 slots).
//
// Hookable methods (REIMPLEMENTED — clean C++ in code/Game/Work/harvestarea.cpp):
//   [0] ScalarDtor (0x14002A4D0) — calls dtor body 0x1400ED410, conditional free
//   [3] Serialize  (0x1400ED350) — Work::Serialize + tree-set + array + int serialization
//   [4] Slot4      (0x1400ECCD0) — 363 instructions, crop harvest assignment logic
//   [6] Slot6      (0x1400ECC20) — insertion into work tree structure
//   [7] Slot7      (0x1400ECB60) — tree node lookup/insertion with validation
//   [8] Slot8      (0x1400ED270) — CancelAll: loops tree, cancels each work item
//   [9] Slot9      (0x1400EC930) — RemoveInvalid (hook DISABLED, bug class 2)
//  [10] Slot10     (0x1400ED2F0) — GetWorkEntity: returns entity ptr from work's entity ref
//
// Too small to hook (documented only):
//   [1] VectorDtor (0x14002A450) — 8 bytes
//   [2] BaseDtor   (0x14002A440) — 7 bytes
//   [5] Execute    (0x1400F7540) — 3 bytes, returns false
//  [11] Slot11     (0x140029860) — 9 bytes, returns (this->field50 > 0)

#include "HarvestArea.h"
#include "Work/harvestarea_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// ScalarDtor (0x14002A4D0) — REIMPLEMENTED
// ============================================================
typedef void* (*HA_ScalarDtor_t)(void* self, uint32_t flags);
static HookInfo g_hookScalarDtor;

static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "HarvestArea::ScalarDtor";
    return HarvestArea_ScalarDtor_Impl((HarvestArea*)self, flags);
}

void InstallHarvestAreaScalarDtor() {
    HarvestArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002A4D0);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[HarvestArea] ScalarDtor hooked at %p (REIMPLEMENTED, prologue=12)\n", (void*)target);
#endif
}
void RemoveHarvestAreaScalarDtor() { RemoveHook(&g_hookScalarDtor); }

// ============================================================
// Serialize (0x1400ED350) — REIMPLEMENTED
// Work::Serialize + tree-set at +0x48 + array at +0x58 + int at +0x60.
// ============================================================
typedef void (*HA_Serialize_t)(void* self, void* stream);
static HookInfo g_hookSerialize;

static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "HarvestArea::Serialize";
    HarvestArea_Serialize_Impl((HarvestArea*)self, stream);
}

void InstallHarvestAreaSerialize() {
    HarvestArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ED350);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[HarvestArea] Serialize hooked at %p (REIMPLEMENTED, prologue=15)\n", (void*)target);
#endif
}
void RemoveHarvestAreaSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// Slot4 (0x1400ECCD0) — REIMPLEMENTED
// 363 instructions. Main crop harvest assignment logic.
// Uses int return type (bug class 7).
// ============================================================
typedef uint64_t (*HA_Slot4_t)(void* self, void* param2, uint64_t param3, uint64_t param4);
static HookInfo g_hookSlot4;

static uint64_t Reimpl_Slot4(void* self, void* param2, uint64_t param3, uint64_t param4) {
    g_lastHook = "HarvestArea::Slot4";
    return (uint64_t)HarvestArea_Slot4_Impl((HarvestArea*)self, param2, param3, param4);
}

void InstallHarvestAreaSlot4() {
    HarvestArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ECCD0);
    // Prologue: MOV [RSP+0x10],RDX(5) + PUSH RBP(1) + PUSH R14(2) + PUSH R15(2) + LEA RBP(5) = 15
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[HarvestArea] Slot4 hooked at %p (REIMPLEMENTED, prologue=15)\n", (void*)target);
#endif
}
void RemoveHarvestAreaSlot4() { RemoveHook(&g_hookSlot4); }

// ============================================================
// Slot6 (0x1400ECC20) — REIMPLEMENTED
// Inserts work item into tree structure at +0x48.
// Note: original takes 4 params (self, param2, param3, param4)
// but the hook was declared with only 2. We pass through correctly.
// ============================================================
typedef void (*HA_Slot6_t)(void* self, void* param2, void* param3, uint32_t param4);
static HookInfo g_hookSlot6;

static void Reimpl_Slot6(void* self, void* param2, void* param3, uint32_t param4) {
    g_lastHook = "HarvestArea::Slot6";
    HarvestArea_Slot6_Impl((HarvestArea*)self, param2, param3, param4);
}

void InstallHarvestAreaSlot6() {
    HarvestArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ECC20);
    // Prologue: MOV [RSP+8],RBX(5) + MOV [RSP+0x10],RSI(5) + PUSH RDI(1) + SUB RSP,0x20(4) = 15
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 15);
#ifdef _DEBUG
    printf("[HarvestArea] Slot6 hooked at %p (REIMPLEMENTED, prologue=15)\n", (void*)target);
#endif
}
void RemoveHarvestAreaSlot6() { RemoveHook(&g_hookSlot6); }

// ============================================================
// Slot7 (0x1400ECB60) — REIMPLEMENTED
// Tree node lookup/insertion with validation.
// ============================================================
typedef void (*HA_Slot7_t)(void* self, void* param2, void* param3, uint32_t param4);
static HookInfo g_hookSlot7;

static void Reimpl_Slot7(void* self, void* param2, void* param3, uint32_t param4) {
    g_lastHook = "HarvestArea::Slot7";
    HarvestArea_Slot7_Impl((HarvestArea*)self, param2, param3, param4);
}

void InstallHarvestAreaSlot7() {
    HarvestArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ECB60);
    // Prologue: MOV [RSP+8],RBX(5) + MOV [RSP+0x10],RSI(5) + MOV [RSP+0x18],RDI(5) = 15
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 15);
#ifdef _DEBUG
    printf("[HarvestArea] Slot7 hooked at %p (REIMPLEMENTED, prologue=15)\n", (void*)target);
#endif
}
void RemoveHarvestAreaSlot7() { RemoveHook(&g_hookSlot7); }

// ============================================================
// Slot8 / CancelAll (0x1400ED270) — REIMPLEMENTED
// Loops tree at +0x48, cancels each work item via engine call.
// ============================================================
typedef void (*HA_Slot8_t)(void* self);
static HookInfo g_hookSlot8;

static void Reimpl_Slot8(void* self) {
    g_lastHook = "HarvestArea::Slot8";
    HarvestArea_Slot8_Impl((HarvestArea*)self);
}

void InstallHarvestAreaSlot8() {
    HarvestArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ED270);
    // Prologue: PUSH RDI(2) + SUB RSP,0x20(4) + CMP [RCX+0x50],0(4) + MOV RDI,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[HarvestArea] Slot8 hooked at %p (REIMPLEMENTED, prologue=13)\n", (void*)target);
#endif
}
void RemoveHarvestAreaSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Slot9 / RemoveInvalid (0x1400EC930) — REIMPLEMENTED
// Hook DISABLED in hooks.cpp (bug class 2: MOV RAX,RSP in prologue).
// Clean C++ implemented for standalone exe.
// 167 instructions. Validates tree entries, removes dead ones.
// ============================================================
typedef int (*HA_Slot9_t)(void* self, void* param2);
static HookInfo g_hookSlot9;

static int Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "HarvestArea::Slot9";
    return HarvestArea_Slot9_Impl((HarvestArea*)self, param2);
}

void InstallHarvestAreaSlot9() {
    HarvestArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EC930);
    // Prologue: MOV RAX,RSP(3) + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) +
    //           PUSH R12(2) + PUSH R13(2) + PUSH R14(2) = 12
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 12);
#ifdef _DEBUG
    printf("[HarvestArea] Slot9 hooked at %p (REIMPLEMENTED, prologue=12)\n", (void*)target);
#endif
}
void RemoveHarvestAreaSlot9() { RemoveHook(&g_hookSlot9); }

// ============================================================
// Slot10 / GetWorkEntity (0x1400ED2F0) — REIMPLEMENTED
// Returns entity ptr from work's entity ref at +0x10.
// ============================================================
typedef void* (*HA_Slot10_t)(void* self, void* outPtr);
static HookInfo g_hookSlot10;

static void* Reimpl_Slot10(void* self, void* outPtr) {
    g_lastHook = "HarvestArea::Slot10";
    return HarvestArea_Slot10_Impl((HarvestArea*)self, outPtr);
}

void InstallHarvestAreaSlot10() {
    HarvestArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ED2F0);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 14
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 14);
#ifdef _DEBUG
    printf("[HarvestArea] Slot10 hooked at %p (REIMPLEMENTED, prologue=14)\n", (void*)target);
#endif
}
void RemoveHarvestAreaSlot10() { RemoveHook(&g_hookSlot10); }
