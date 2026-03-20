// engine/reimpl/marketwork.cpp
// Reimplementation of MarketWork vtable methods from Application-x64.exe.
//
// MarketWork is a Work subclass (0x58 bytes) with a BST/map at +0x48 and
// a count at +0x50. Used for market trading operations.
//
// Vtable at 0x140161710, 12 slots:
//   +0x00: ScalarDtor (0x14002a910)   — tiny, not hooked
//   +0x08: VectorDtor (0x14002a8c0)   — tiny, not hooked
//   +0x10: BaseDtor   (0x14002a8b0)   — tiny, not hooked
//   +0x18: Serialize  (0x1400f1d70)   — HOOKED -> REIMPLEMENTED
//   +0x20: Slot4      (0x1400eff80)   — HOOKED (passthrough, DISABLED — MOV RAX,RSP)
//   +0x28: Execute    (0x1400f7540)   — 3 bytes, unhookable
//   +0x30: Slot6      (0x1400eff60)   — HOOKED (passthrough, DISABLED — relative CALL)
//   +0x38: Slot7      (0x1400eff50)   — not a function in Ghidra, skip
//   +0x40: Slot8      (0x1400f1ca0)   — HOOKED (passthrough, DISABLED — short Jcc)
//   +0x48: Slot9      (0x1400efd20)   — HOOKED (passthrough, DISABLED — MOV RAX,RSP)
//   +0x50: Slot10     (0x1400f1d10)   — HOOKED -> REIMPLEMENTED
//   +0x58: Slot11     (0x140029860)   — 9 bytes, unhookable

#include "MarketWork.h"
#include "Work/marketwork_funcs.h"
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
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;
static HookInfo g_hookSlot10;

// ============================================================
// Serialize (0x1400f1d70) — REIMPLEMENTED
// ============================================================
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "MarketWork::Serialize";
    MarketWork_Serialize_Impl((MarketWork*)self, stream);
}

// ============================================================
// Slot4 (0x1400eff80) — passthrough (DISABLED hook — MOV RAX,RSP)
// Clean reimpl in code/Game/Work/marketwork.cpp for standalone exe.
// ============================================================
typedef uint32_t (*fn_Slot4_t)(void* self, void* param2, void* param3);
static uint32_t Reimpl_Slot4(void* self, void* param2, void* param3) {
    g_lastHook = "MarketWork::Slot4";
    return ((fn_Slot4_t)g_hookSlot4.trampoline)(self, param2, param3);
}

// ============================================================
// Slot6 (0x1400eff60) — passthrough (DISABLED hook — relative CALL)
// Clean reimpl in code/Game/Work/marketwork.cpp for standalone exe.
// ============================================================
typedef uint8_t (*fn_Slot6_t)(void* self, void* param2);
static uint8_t Reimpl_Slot6(void* self, void* param2) {
    g_lastHook = "MarketWork::Slot6";
    return ((fn_Slot6_t)g_hookSlot6.trampoline)(self, param2);
}

// ============================================================
// Slot8 (0x1400f1ca0) — passthrough (DISABLED hook — short Jcc)
// Clean reimpl in code/Game/Work/marketwork.cpp for standalone exe.
// ============================================================
typedef int (*fn_Slot8_t)(void* self);
static int Reimpl_Slot8(void* self) {
    g_lastHook = "MarketWork::Slot8";
    return ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

// ============================================================
// Slot9 (0x1400efd20) — passthrough (DISABLED hook — MOV RAX,RSP)
// Clean reimpl in code/Game/Work/marketwork.cpp for standalone exe.
// ============================================================
typedef bool (*fn_Slot9_t)(void* self, void* param2);
static bool Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "MarketWork::Slot9";
    return ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

// ============================================================
// Slot10 (0x1400f1d10) — REIMPLEMENTED
// ============================================================
typedef void* (*fn_Slot10_t)(void* self, void* param2);
static void* Reimpl_Slot10(void* self, void* param2) {
    g_lastHook = "MarketWork::Slot10";
    return MarketWork_Slot10_Impl((MarketWork*)self, param2);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallMarketWorkSerialize() {
    MarketWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f1d70);
    // Prologue: PUSH RDI(2) + SUB RSP,0x60(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[MarketWork] Serialize hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemoveMarketWorkSerialize() { RemoveHook(&g_hookSerialize); }

void InstallMarketWorkSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400eff80);
    // Prologue: complex, check first safe boundary
    // MOV [RSP+0x18],R8(5) + MOV [RSP+0x10],RDX(5) + PUSH RBP(2) + PUSH RBX(2) = 14
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 14);
#ifdef _DEBUG
    printf("[MarketWork] Slot4 hooked at %p (passthrough, DISABLED)\n", (void*)target);
#endif
}

void RemoveMarketWorkSlot4() { RemoveHook(&g_hookSlot4); }

void InstallMarketWorkSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400eff60);
    // Small function but >= 12 bytes
    // SUB RSP,0x28(4) + ... check size
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 13);
#ifdef _DEBUG
    printf("[MarketWork] Slot6 hooked at %p (passthrough, DISABLED)\n", (void*)target);
#endif
}

void RemoveMarketWorkSlot6() { RemoveHook(&g_hookSlot6); }

void InstallMarketWorkSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f1ca0);
    // Prologue: MOV [RSP+0x8],RBX(5) + PUSH RDI(2) + SUB RSP,0x20(4) + MOV RDI,RCX(3) = 14 -> corrected to 15
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[MarketWork] Slot8 hooked at %p (passthrough, DISABLED)\n", (void*)target);
#endif
}

void RemoveMarketWorkSlot8() { RemoveHook(&g_hookSlot8); }

void InstallMarketWorkSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400efd20);
    // Prologue needs checking - use safe 15
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 18);
#ifdef _DEBUG
    printf("[MarketWork] Slot9 hooked at %p (passthrough, DISABLED)\n", (void*)target);
#endif
}

void RemoveMarketWorkSlot9() { RemoveHook(&g_hookSlot9); }

void InstallMarketWorkSlot10() {
    MarketWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f1d10);
    // Same pattern as LocationWork::Slot10: MOV [RSP+0x10],RBX(5) + PUSH RDI(2) + SUB RSP,0x20(4) + LEA(4) = 15
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 17);
#ifdef _DEBUG
    printf("[MarketWork] Slot10 hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}

void RemoveMarketWorkSlot10() { RemoveHook(&g_hookSlot10); }
