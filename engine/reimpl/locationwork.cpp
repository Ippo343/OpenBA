// engine/reimpl/locationwork.cpp
// Reimplementation of LocationWork vtable methods from Application-x64.exe.
//
// LocationWork is a Work subclass (0x50 bytes) with one extra field at +0x48
// (an InternalBasePtr to a target entity).
//
// Vtable at 0x1401616a0, 12 slots:
//   +0x00: ScalarDtor (0x14002a850)   — tiny, not hooked
//   +0x08: VectorDtor (0x14002a800)   — tiny, not hooked
//   +0x10: BaseDtor   (0x14002a7f0)   — tiny, not hooked
//   +0x18: Serialize  (0x1400efba0)   — HOOKED (REIMPLEMENTED)
//   +0x20: Slot4      (0x1400ef880)   — HOOKED (REIMPLEMENTED)
//   +0x28: Execute    (0x1400f7540)   — 3 bytes (XOR AL,AL; RET), unhookable
//   +0x30: Slot6      (0x1400ef850)   — HOOKED (REIMPLEMENTED)
//   +0x38: Slot7      (0x1400ef820)   — HOOKED (REIMPLEMENTED)
//   +0x40: Slot8      (0x1400efaf0)   — HOOKED (REIMPLEMENTED)
//   +0x48: Slot9      (0x1400ef7c0)   — HOOKED (REIMPLEMENTED)
//   +0x50: Slot10     (0x1400efb40)   — HOOKED (REIMPLEMENTED)
//   +0x58: Slot11     (0x1400295a0)   — 10 bytes, unhookable (checks +0x48 != 0)

#include "LocationWork.h"
#include "Work/locationwork_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookSlot4;
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot7;
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;
static HookInfo g_hookSlot10;

// ============================================================
// Serialize (0x1400efba0) — REIMPLEMENTED
// ============================================================
static void Reimpl_Serialize(LocationWork* self, void* stream) {
    g_lastHook = "LocationWork::Serialize";
    LocationWork_Serialize_Impl(self, stream);
}

// ============================================================
// Slot4 (0x1400ef880) — REIMPLEMENTED
// ============================================================
static int Reimpl_Slot4(void* self, void* param2, void* param3, void* param4) {
    g_lastHook = "LocationWork::Slot4";
    return LocationWork_Slot4_Impl((LocationWork*)self, param2, (uint64_t)param3, (uint64_t)param4);
}

// ============================================================
// Slot6 (0x1400ef850) — REIMPLEMENTED
// ============================================================
static uint64_t Reimpl_Slot6(void* self) {
    g_lastHook = "LocationWork::Slot6";
    return LocationWork_Slot6_Impl((LocationWork*)self) ? 1 : 0;
}

// ============================================================
// Slot7 (0x1400ef820) — REIMPLEMENTED
// ============================================================
static int Reimpl_Slot7(void* self) {
    g_lastHook = "LocationWork::Slot7";
    return LocationWork_Slot7_Impl((LocationWork*)self);
}

// ============================================================
// Slot8 (0x1400efaf0) — REIMPLEMENTED
// ============================================================
static int Reimpl_Slot8(void* self) {
    g_lastHook = "LocationWork::Slot8";
    return LocationWork_Slot8_Impl((LocationWork*)self);
}

// ============================================================
// Slot9 (0x1400ef7c0) — REIMPLEMENTED
// ============================================================
static uint64_t Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "LocationWork::Slot9";
    return LocationWork_Slot9_Impl((LocationWork*)self, param2) ? 1 : 0;
}

// ============================================================
// Slot10 (0x1400efb40) — REIMPLEMENTED
// ============================================================
static void* Reimpl_Slot10(void* self, void* param2) {
    g_lastHook = "LocationWork::Slot10";
    return LocationWork_Slot10_Impl((LocationWork*)self, param2);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallLocationWorkSerialize() {
    LocationWork_ResolveFuncs();
#ifdef _DEBUG
    printf("[LocationWork] Serialize REIMPLEMENTED\n");
#endif
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400efba0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
}

void RemoveLocationWorkSerialize() { RemoveHook(&g_hookSerialize); }

void InstallLocationWorkSlot4() {
    LocationWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ef880);
    // Prologue: PUSH RBP(2) + PUSH R12(2) + PUSH R14(2) + MOV RBP,RSP(3) + SUB RSP,0x70(4) = 13
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[LocationWork] Slot4 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemoveLocationWorkSlot4() { RemoveHook(&g_hookSlot4); }

void InstallLocationWorkSlot6() {
    LocationWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ef850);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + MOV RBX,RCX(3) + LEA RCX,[RSP+0x30](5) = 14
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 14);
#ifdef _DEBUG
    printf("[LocationWork] Slot6 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemoveLocationWorkSlot6() { RemoveHook(&g_hookSlot6); }

void InstallLocationWorkSlot7() {
    LocationWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ef820);
    // Same prologue as Slot6: 14 bytes
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 14);
#ifdef _DEBUG
    printf("[LocationWork] Slot7 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemoveLocationWorkSlot7() { RemoveHook(&g_hookSlot7); }

void InstallLocationWorkSlot8() {
    LocationWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400efaf0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + CMP [RCX+0x48],0(5) + LEA RBX,[RCX+0x48](4) = 15
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[LocationWork] Slot8 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemoveLocationWorkSlot8() { RemoveHook(&g_hookSlot8); }

void InstallLocationWorkSlot9() {
    LocationWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ef7c0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + MOV RAX,[RCX+0x48](4) + MOV RBX,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 13);
#ifdef _DEBUG
    printf("[LocationWork] Slot9 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemoveLocationWorkSlot9() { RemoveHook(&g_hookSlot9); }

void InstallLocationWorkSlot10() {
    LocationWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400efb40);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(2) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 15
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 17);
#ifdef _DEBUG
    printf("[LocationWork] Slot10 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemoveLocationWorkSlot10() { RemoveHook(&g_hookSlot10); }
