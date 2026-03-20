// engine/reimpl/tradework.cpp
// Reimplementation of TradeWork vtable methods from Application-x64.exe.
//
// TradeWork is a Work subclass (0x58 bytes) for trade post operations.
// Has an extended BST at +0x48 and a count at +0x50.
//
// Vtable at 0x140161a10, 12 slots:
//   +0x00: ScalarDtor (0x14002c660)   — REIMPLEMENTED
//   +0x08: VectorDtor (0x14002c610)   — tiny, not hooked (type info)
//   +0x10: BaseDtor   (0x14002c600)   — tiny, not hooked (type info)
//   +0x18: Serialize  (0x1400f6cb0)   — REIMPLEMENTED
//   +0x20: Slot4      (0x1400f5d40)   — DISABLED (bug class 3), passthrough
//   +0x28: Execute    (0x1400f7540)   — 3 bytes, unhookable (clean code only)
//   +0x30: Slot6      (0x1400f5d20)   — DISABLED (relative CALL), passthrough
//   +0x38: Slot7      (0x1400f5d10)   — too small, clean code only
//   +0x40: Slot8      (0x1400f6be0)   — REIMPLEMENTED (CancelAll)
//   +0x48: Slot9      (0x1400f5ae0)   — DISABLED (bug class 3), passthrough
//   +0x50: Slot10     (0x1400f6c50)   — REIMPLEMENTED (GetWorkEntity)
//   +0x58: Slot11     (0x140029860)   — 9 bytes, unhookable (clean code only)

#include "TradeWork.h"
#include "Work/tradework_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookScalarDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookSlot4;
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;
static HookInfo g_hookSlot10;

// ============================================================
// ScalarDtor (0x14002c660) — REIMPLEMENTED
// ============================================================
typedef void* (*TrW_ScalarDtor_t)(void* self, uint32_t flags);

static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "TradeWork::ScalarDtor";
    return TradeWork_ScalarDtor_Impl((TradeWork*)self, flags);
}

void InstallTradeWorkScalarDtor() {
    TradeWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002c660);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[TradeWork] ScalarDtor hooked at %p (REIMPLEMENTED, prologue=12)\n", (void*)target);
#endif
}
void RemoveTradeWorkScalarDtor() { RemoveHook(&g_hookScalarDtor); }

// ============================================================
// Serialize (0x1400f6cb0) — REIMPLEMENTED
// ============================================================
typedef void (*TrW_Serialize_t)(void* self, void* stream);

static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "TradeWork::Serialize";
    TradeWork_Serialize_Impl((TradeWork*)self, stream);
}

void InstallTradeWorkSerialize() {
    TradeWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f6cb0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TradeWork] Serialize hooked at %p (REIMPLEMENTED, prologue=15)\n", (void*)target);
#endif
}
void RemoveTradeWorkSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// Slot4 (0x1400f5d40) — DISABLED (bug class 3), passthrough
// Complex trade assignment logic (~700 lines decompiled).
// ============================================================
typedef uint32_t (*TrW_Slot4_t)(void* self, void* param2, void* param3);

static uint32_t Reimpl_Slot4(void* self, void* param2, void* param3) {
    g_lastHook = "TradeWork::Slot4";
    return ((TrW_Slot4_t)g_hookSlot4.trampoline)(self, param2, param3);
}

void InstallTradeWorkSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f5d40);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 16);
#ifdef _DEBUG
    printf("[TradeWork] Slot4 hooked at %p (passthrough/DISABLED)\n", (void*)target);
#endif
}
void RemoveTradeWorkSlot4() { RemoveHook(&g_hookSlot4); }

// ============================================================
// Slot6 (0x1400f5d20) — DISABLED (relative CALL), passthrough
// Clean code in tradework.cpp for standalone exe.
// ============================================================
typedef uint8_t (*TrW_Slot6_t)(void* self, void* param2);

static uint8_t Reimpl_Slot6(void* self, void* param2) {
    g_lastHook = "TradeWork::Slot6";
    return ((TrW_Slot6_t)g_hookSlot6.trampoline)(self, param2);
}

void InstallTradeWorkSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f5d20);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 13);
#ifdef _DEBUG
    printf("[TradeWork] Slot6 hooked at %p (passthrough/DISABLED)\n", (void*)target);
#endif
}
void RemoveTradeWorkSlot6() { RemoveHook(&g_hookSlot6); }

// ============================================================
// Slot8 / CancelAll (0x1400f6be0) — REIMPLEMENTED
// ============================================================
typedef int (*TrW_Slot8_t)(void* self);

static int Reimpl_Slot8(void* self) {
    g_lastHook = "TradeWork::Slot8";
    return TradeWork_Slot8_Impl((TradeWork*)self);
}

void InstallTradeWorkSlot8() {
    TradeWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f6be0);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[TradeWork] Slot8 hooked at %p (REIMPLEMENTED, prologue=15)\n", (void*)target);
#endif
}
void RemoveTradeWorkSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Slot9 / RemoveInvalid (0x1400f5ae0) — DISABLED (bug class 3)
// Clean code ready in tradework.cpp for standalone exe.
// ============================================================
typedef uint64_t (*TrW_Slot9_t)(void* self, void* param2);

static uint64_t Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "TradeWork::Slot9";
    return ((TrW_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

void InstallTradeWorkSlot9() {
    TradeWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f5ae0);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 18);
#ifdef _DEBUG
    printf("[TradeWork] Slot9 hooked at %p (passthrough/DISABLED)\n", (void*)target);
#endif
}
void RemoveTradeWorkSlot9() { RemoveHook(&g_hookSlot9); }

// ============================================================
// Slot10 / GetWorkEntity (0x1400f6c50) — REIMPLEMENTED
// ============================================================
typedef void* (*TrW_Slot10_t)(void* self, void* param2);

static void* Reimpl_Slot10(void* self, void* param2) {
    g_lastHook = "TradeWork::Slot10";
    return TradeWork_Slot10_Impl((TradeWork*)self, param2);
}

void InstallTradeWorkSlot10() {
    TradeWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f6c50);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 17);
#ifdef _DEBUG
    printf("[TradeWork] Slot10 hooked at %p (REIMPLEMENTED, prologue=17)\n", (void*)target);
#endif
}
void RemoveTradeWorkSlot10() { RemoveHook(&g_hookSlot10); }
