// engine/reimpl/pickup.cpp
// Reimplementation of Pickup vtable methods from Application-x64.exe.
//
// Pickup is a Work subclass (0x60 bytes) for item pickup work actions.
// Fields: status (+0x48), citizenIBP (+0x50), candidateList (+0x58).
//
// NOTE: Pickup has its OWN Execute override (0x1400f38f0), unlike other Work classes
// which share the base Execute (0x1400f7540 = XOR AL,AL; RET).
//
// Vtable at 0x140161810, 12 slots:
//   +0x00: ScalarDtor (0x14002c010)   -- tiny, not hooked
//   +0x08: VectorDtor (0x14002bfb0)   -- tiny, not hooked
//   +0x10: BaseDtor   (0x14002bfa0)   -- tiny, not hooked
//   +0x18: Serialize  (0x1400f3aa0)   -- REIMPLEMENTED (clean code)
//   +0x20: Slot4      (0x1400f2f00)   -- REIMPLEMENTED (calls original, complex)
//   +0x28: Execute    (0x1400f38f0)   -- REIMPLEMENTED (clean code, unique to Pickup)
//   +0x30: Slot6      (0x1400f2ec0)   -- REIMPLEMENTED (clean code)
//   +0x38: Slot7      (0x1400f2e80)   -- REIMPLEMENTED (clean code)
//   +0x40: Slot8      (0x1400f39f0)   -- REIMPLEMENTED (clean code)
//   +0x48: Slot9      (0x1400f2e20)   -- REIMPLEMENTED (clean code)
//   +0x50: Slot10     (0x1400f3a50)   -- REIMPLEMENTED (clean code)
//   +0x58: Slot11     (0x14002c000)   -- 10 bytes, unhookable (clean code only)

#include "Pickup.h"
#include "Work/pickup_funcs.h"
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
static HookInfo g_hookExecute;
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot7;
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;
static HookInfo g_hookSlot10;

// ============================================================
// Serialize (0x1400f3aa0) -- REIMPLEMENTED
// ============================================================
static void Reimpl_Serialize(Pickup* self, void* stream) {
    g_lastHook = "Pickup::Serialize";
    Pickup_Serialize_Impl(self, stream);
}

// ============================================================
// Slot4 / CreateWork (0x1400f2f00) -- REIMPLEMENTED
// Complex pickup target assignment (calls through to original).
// Uses int return type (bug class 7).
// ============================================================
static uint64_t Reimpl_Slot4(void* self, void* param2, uint64_t param3, uint64_t param4) {
    g_lastHook = "Pickup::Slot4";
    return (uint64_t)Pickup_Slot4_Impl((Pickup*)self, param2, param3, param4);
}

// ============================================================
// Execute (0x1400f38f0) -- REIMPLEMENTED
// Unique to Pickup, handles re-targeting.
// ============================================================
static uint64_t Reimpl_Execute(void* self, void* param2, void* param3) {
    g_lastHook = "Pickup::Execute";
    return (uint64_t)Pickup_Execute_Impl((Pickup*)self, param2, param3);
}

// ============================================================
// Slot6 / OnComplete (0x1400f2ec0) -- REIMPLEMENTED
// ============================================================
static uint64_t Reimpl_Slot6(void* self, void* param2) {
    g_lastHook = "Pickup::Slot6";
    return (uint64_t)Pickup_Slot6_Impl((Pickup*)self) ? 1 : 0;
}

// ============================================================
// Slot7 / OnCancel (0x1400f2e80) -- REIMPLEMENTED
// ============================================================
static int Reimpl_Slot7(void* self) {
    g_lastHook = "Pickup::Slot7";
    return Pickup_Slot7_Impl((Pickup*)self);
}

// ============================================================
// Slot8 / CancelAll (0x1400f39f0) -- REIMPLEMENTED
// ============================================================
static int Reimpl_Slot8(void* self) {
    g_lastHook = "Pickup::Slot8";
    return Pickup_Slot8_Impl((Pickup*)self);
}

// ============================================================
// Slot9 / Validate (0x1400f2e20) -- REIMPLEMENTED
// ============================================================
static uint64_t Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "Pickup::Slot9";
    return Pickup_Slot9_Impl((Pickup*)self, param2) ? 1 : 0;
}

// ============================================================
// Slot10 / GetTarget (0x1400f3a50) -- REIMPLEMENTED
// ============================================================
static void* Reimpl_Slot10(void* self, void* param2) {
    g_lastHook = "Pickup::Slot10";
    return Pickup_Slot10_Impl((Pickup*)self, param2);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallPickupSerialize() {
    Pickup_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f3aa0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[Pickup] Serialize hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemovePickupSerialize() { RemoveHook(&g_hookSerialize); }

void InstallPickupSlot4() {
    Pickup_ResolveFuncs();
    // VTable hook: MOV RAX,RSP in prologue breaks with RSP compensation.
    // vtable at 0x140161810, Slot4 = slot 4 (+0x20)
    InstallHookVTable(0x140161810, 4, (void*)Reimpl_Slot4, &g_hookSlot4);
    RegisterHookInfo("Pickup::Slot4", &g_hookSlot4);
#ifdef _DEBUG
    printf("[Pickup] Slot4 vtable-hooked (original=%p)\n", g_hookSlot4.trampoline);
#endif
}

void RemovePickupSlot4() { RemoveHookVTable(&g_hookSlot4); }

void InstallPickupExecute() {
    Pickup_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f38f0);
    // Prologue: MOV [RSP+0x20],RBP(5) + PUSH RSI(2) + PUSH R14(2) + PUSH R15(2) + SUB RSP,0x20(4) = 15
    InstallHook(target, (void*)Reimpl_Execute, &g_hookExecute, 18);
#ifdef _DEBUG
    printf("[Pickup] Execute hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemovePickupExecute() { RemoveHook(&g_hookExecute); }

void InstallPickupSlot6() {
    Pickup_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f2ec0);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 14);
#ifdef _DEBUG
    printf("[Pickup] Slot6 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemovePickupSlot6() { RemoveHook(&g_hookSlot6); }

void InstallPickupSlot7() {
    Pickup_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f2e80);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 14);
#ifdef _DEBUG
    printf("[Pickup] Slot7 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemovePickupSlot7() { RemoveHook(&g_hookSlot7); }

void InstallPickupSlot8() {
    Pickup_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f39f0);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[Pickup] Slot8 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemovePickupSlot8() { RemoveHook(&g_hookSlot8); }

void InstallPickupSlot9() {
    Pickup_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f2e20);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 16);
#ifdef _DEBUG
    printf("[Pickup] Slot9 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemovePickupSlot9() { RemoveHook(&g_hookSlot9); }

void InstallPickupSlot10() {
    Pickup_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f3a50);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 18);
#ifdef _DEBUG
    printf("[Pickup] Slot10 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemovePickupSlot10() { RemoveHook(&g_hookSlot10); }
