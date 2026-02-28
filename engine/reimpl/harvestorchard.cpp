// engine/reimpl/harvestorchard.cpp
// Reimplementation of HarvestOrchard methods from Application-x64.exe.
//
// HarvestOrchard — Orchard harvest work action (0x68 bytes).
// Inherits: Object::Internal -> Work -> AutoPickupWork -> HarvestOrchard
// Vtable at 0x140161520 (12 slots).
// Same field layout as HarvestArea (different vtable, different work logic).
//
// Hookable methods (all passthrough):
//   [0] ScalarDtor (0x14002A590) — calls dtor body 0x1400EDF60, conditional free
//   [3] Serialize  (0x1400EDEA0) — identical structure to HarvestArea::Serialize
//   [4] Slot4      (0x1400ED8C0) — 303 instructions, orchard harvest assignment logic
//   [6] Slot6      (0x1400ED880) — thunk: sub rsp + add rcx,0x48 + call + ret
//   [7] Slot7      (0x1400ED810) — tree node lookup/insertion
//   [8] Slot8      (0x1400EDDC0) — CancelAll
//   [9] Slot9      (0x1400ED5E0) — RemoveInvalid (167 instructions)
//  [10] Slot10     (0x1400EDE40) — GetWorkEntity
//
// Too small to hook:
//   [1] VectorDtor (0x14002A510) — 8 bytes
//   [2] BaseDtor   (0x14002A500) — 7 bytes
//   [5] Execute    (0x1400F7540) — 3 bytes, returns false
//  [11] Slot11     (0x140029860) — 9 bytes, returns (this->field50 > 0)

#include "HarvestOrchard.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// ScalarDtor (0x14002A590) — passthrough
// ============================================================
typedef void* (*HO_ScalarDtor_t)(void* self, uint32_t flags);
static HookInfo g_hookScalarDtor;

static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    return ((HO_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

void InstallHarvestOrchardScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002A590);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[HarvestOrchard] ScalarDtor hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardScalarDtor() { RemoveHook(&g_hookScalarDtor); }

// ============================================================
// Serialize (0x1400EDEA0) — passthrough
// Same structure as HarvestArea: tree-set + array + int fields.
// ============================================================
typedef void (*HO_Serialize_t)(void* self, void* stream);
static HookInfo g_hookSerialize;

static void Reimpl_Serialize(void* self, void* stream) {
    ((HO_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

void InstallHarvestOrchardSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EDEA0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[HarvestOrchard] Serialize hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// Slot4 (0x1400ED8C0) — passthrough
// 303 instructions. Orchard harvest assignment logic.
// ============================================================
typedef uint64_t (*HO_Slot4_t)(void* self, void* param2, uint64_t param3, uint64_t param4);
static HookInfo g_hookSlot4;

static uint64_t Reimpl_Slot4(void* self, void* param2, uint64_t param3, uint64_t param4) {
    return ((HO_Slot4_t)g_hookSlot4.trampoline)(self, param2, param3, param4);
}

void InstallHarvestOrchardSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ED8C0);
    // Prologue: PUSH RBP(1) + PUSH R13(2) + PUSH R14(2) + LEA RBP,[RSP-0x47](5) + SUB RSP,0xd0(7) = 17
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 17);
#ifdef _DEBUG
    printf("[HarvestOrchard] Slot4 hooked at %p (passthrough, prologue=17)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSlot4() { RemoveHook(&g_hookSlot4); }

// ============================================================
// Slot6 (0x1400ED880) — passthrough
// Thunk: sub rsp,0x28; mov rdx,[rcx+0x48]; add rcx,0x48; call + ret
// ============================================================
typedef uint8_t (*HO_Slot6_t)(void* self, void* param2);
static HookInfo g_hookSlot6;

static uint8_t Reimpl_Slot6(void* self, void* param2) {
    return ((HO_Slot6_t)g_hookSlot6.trampoline)(self, param2);
}

void InstallHarvestOrchardSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ED880);
    // Prologue: SUB RSP,0x28(4) + MOV RDX,[RCX+0x48](4) + ADD RCX,0x48(4) = 12
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 12);
#ifdef _DEBUG
    printf("[HarvestOrchard] Slot6 hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSlot6() { RemoveHook(&g_hookSlot6); }

// ============================================================
// Slot7 (0x1400ED810) — passthrough
// Tree node lookup/insertion with validation.
// ============================================================
typedef void (*HO_Slot7_t)(void* self, void* param2);
static HookInfo g_hookSlot7;

static void Reimpl_Slot7(void* self, void* param2) {
    ((HO_Slot7_t)g_hookSlot7.trampoline)(self, param2);
}

void InstallHarvestOrchardSlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ED810);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV RDX,[RCX+0x48](4) = 14
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 14);
#ifdef _DEBUG
    printf("[HarvestOrchard] Slot7 hooked at %p (passthrough, prologue=14)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSlot7() { RemoveHook(&g_hookSlot7); }

// ============================================================
// Slot8 / CancelAll (0x1400EDDC0) — passthrough
// ============================================================
typedef void (*HO_Slot8_t)(void* self);
static HookInfo g_hookSlot8;

static void Reimpl_Slot8(void* self) {
    ((HO_Slot8_t)g_hookSlot8.trampoline)(self);
}

void InstallHarvestOrchardSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EDDC0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x20(4) + CMP [RCX+0x50],0(4) + MOV RDI,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[HarvestOrchard] Slot8 hooked at %p (passthrough, prologue=13)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Slot9 / RemoveInvalid (0x1400ED5E0) — passthrough
// 167 instructions.
// ============================================================
typedef bool (*HO_Slot9_t)(void* self, void* param2);
static HookInfo g_hookSlot9;

static bool Reimpl_Slot9(void* self, void* param2) {
    return ((HO_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

void InstallHarvestOrchardSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ED5E0);
    // Prologue: MOV RAX,RSP(3) + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) +
    //           PUSH R12(2) + PUSH R13(2) + PUSH R14(2) = 12
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 12);
#ifdef _DEBUG
    printf("[HarvestOrchard] Slot9 hooked at %p (passthrough, prologue=12)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSlot9() { RemoveHook(&g_hookSlot9); }

// ============================================================
// Slot10 / GetWorkEntity (0x1400EDE40) — passthrough
// ============================================================
typedef void* (*HO_Slot10_t)(void* self, void* outPtr);
static HookInfo g_hookSlot10;

static void* Reimpl_Slot10(void* self, void* outPtr) {
    return ((HO_Slot10_t)g_hookSlot10.trampoline)(self, outPtr);
}

void InstallHarvestOrchardSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400EDE40);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 14
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 14);
#ifdef _DEBUG
    printf("[HarvestOrchard] Slot10 hooked at %p (passthrough, prologue=14)\n", (void*)target);
#endif
}
void RemoveHarvestOrchardSlot10() { RemoveHook(&g_hookSlot10); }
