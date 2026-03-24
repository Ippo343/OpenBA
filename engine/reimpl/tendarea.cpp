// engine/reimpl/tendarea.cpp
// Reimplementation of TendArea vtable methods from Application-x64.exe.
//
// TendArea is a Work subclass (0x60 bytes) for area tending work (crops, orchards, etc).
// Has a BST at +0x48, a count at +0x50, and a serialized int at +0x58 (tendMode).
//
// Vtable at 0x140161990, 12 slots:
//   +0x00: ScalarDtor (0x14002c5d0)   — REIMPLEMENTED
//   +0x08: VectorDtor (0x14002c580)   — tiny, not hooked (type info)
//   +0x10: BaseDtor   (0x14002c570)   — tiny, not hooked (type info)
//   +0x18: Serialize  (0x1400f5900)   — REIMPLEMENTED
//   +0x20: Slot4      (0x1400f5490)   — REIMPLEMENTED (complex tending logic)
//   +0x28: Execute    (0x1400f7540)   — 3 bytes, unhookable (clean code only)
//   +0x30: Slot6      (0x1400f5470)   — DISABLED (relative CALL), clean code only
//   +0x38: Slot7      (0x1400f5460)   — too small, clean code only
//   +0x40: Slot8      (0x1400f5830)   — REIMPLEMENTED (CancelAll)
//   +0x48: Slot9      (0x1400f5230)   — DISABLED (bug class 3), clean code only
//   +0x50: Slot10     (0x1400f58a0)   — REIMPLEMENTED (GetWorkEntity)
//   +0x58: Slot11     (0x140029860)   — 9 bytes, unhookable (clean code only)

#include "TendArea.h"
#include "Work/tendarea_funcs.h"
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
// ScalarDtor (0x14002c5d0) — REIMPLEMENTED
// ============================================================
typedef void* (*TAW_ScalarDtor_t)(void* self, uint32_t flags);

static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "TendArea::ScalarDtor";
    return TendArea_ScalarDtor_Impl((TendArea*)self, flags);
}

void InstallTendAreaScalarDtor() {
    TendArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002c5d0);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV EBX,EDX(2) = 12
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[TendArea] ScalarDtor hooked at %p (REIMPLEMENTED, prologue=12)\n", (void*)target);
#endif
}
void RemoveTendAreaScalarDtor() { RemoveHook(&g_hookScalarDtor); }

// ============================================================
// Serialize (0x1400f5900) — REIMPLEMENTED
// ============================================================
typedef void (*TAW_Serialize_t)(void* self, void* stream);

static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "TendArea::Serialize";
    TendArea_Serialize_Impl((TendArea*)self, stream);
}

void InstallTendAreaSerialize() {
    TendArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f5900);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TendArea] Serialize hooked at %p (REIMPLEMENTED, prologue=15)\n", (void*)target);
#endif
}
void RemoveTendAreaSerialize() { RemoveHook(&g_hookSerialize); }

// ============================================================
// Slot4 (0x1400f5490) — REIMPLEMENTED
// Complex tending assignment logic with area/orchard branching.
// ============================================================
typedef uint32_t (*TAW_Slot4_t)(void* self, void* param2, void* param3);

static uint32_t Reimpl_Slot4(void* self, void* param2, void* param3) {
    g_lastHook = "TendArea::Slot4";
    return (uint32_t)TendArea_Slot4_Impl((TendArea*)self, param2, 0, 0);
}

void InstallTendAreaSlot4() {
    TendArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f5490);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 21);
#ifdef _DEBUG
    printf("[TendArea] Slot4 hooked at %p (REIMPLEMENTED, prologue=21)\n", (void*)target);
#endif
}
void RemoveTendAreaSlot4() { RemoveHook(&g_hookSlot4); }

// ============================================================
// Slot6 (0x1400f5470) — DISABLED (relative CALL in prologue)
// Clean code in tendarea.cpp for standalone exe.
// Keeping passthrough for when hook is re-enabled.
// ============================================================
typedef uint8_t (*TAW_Slot6_t)(void* self, void* param2);

static uint8_t Reimpl_Slot6(void* self, void* param2) {
    g_lastHook = "TendArea::Slot6";
    return ((TAW_Slot6_t)g_hookSlot6.trampoline)(self, param2);
}

void InstallTendAreaSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f5470);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 13);
#ifdef _DEBUG
    printf("[TendArea] Slot6 hooked at %p (passthrough/DISABLED)\n", (void*)target);
#endif
}
void RemoveTendAreaSlot6() { RemoveHook(&g_hookSlot6); }

// ============================================================
// Slot8 / CancelAll (0x1400f5830) — REIMPLEMENTED
// ============================================================
typedef int (*TAW_Slot8_t)(void* self);

static int Reimpl_Slot8(void* self) {
    g_lastHook = "TendArea::Slot8";
    return TendArea_Slot8_Impl((TendArea*)self);
}

void InstallTendAreaSlot8() {
    TendArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f5830);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[TendArea] Slot8 hooked at %p (REIMPLEMENTED, prologue=15)\n", (void*)target);
#endif
}
void RemoveTendAreaSlot8() { RemoveHook(&g_hookSlot8); }

// ============================================================
// Slot9 / RemoveInvalid (0x1400f5230) — DISABLED (bug class 3)
// Clean code ready in tendarea.cpp for standalone exe.
// Keeping passthrough for when hook is re-enabled.
// ============================================================
typedef uint64_t (*TAW_Slot9_t)(void* self, void* param2);

static uint64_t Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "TendArea::Slot9";
    return ((TAW_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

void InstallTendAreaSlot9() {
    TendArea_ResolveFuncs();
    // VTable hook: MOV RAX,RSP in prologue breaks with RSP compensation.
    // vtable at 0x140161990, Slot9 = slot 9 (+0x48)
    InstallHookVTable(0x140161990, 9, (void*)Reimpl_Slot9, &g_hookSlot9);
    RegisterHookInfo("TendArea::Slot9", &g_hookSlot9);
#ifdef _DEBUG
    printf("[TendArea] Slot9 vtable-hooked (original=%p)\n", g_hookSlot9.trampoline);
#endif
}
void RemoveTendAreaSlot9() { RemoveHookVTable(&g_hookSlot9); }

// ============================================================
// Slot10 / GetWorkEntity (0x1400f58a0) — REIMPLEMENTED
// ============================================================
typedef void* (*TAW_Slot10_t)(void* self, void* param2);

static void* Reimpl_Slot10(void* self, void* param2) {
    g_lastHook = "TendArea::Slot10";
    return TendArea_Slot10_Impl((TendArea*)self, param2);
}

void InstallTendAreaSlot10() {
    TendArea_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f58a0);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 17);
#ifdef _DEBUG
    printf("[TendArea] Slot10 hooked at %p (REIMPLEMENTED, prologue=17)\n", (void*)target);
#endif
}
void RemoveTendAreaSlot10() { RemoveHook(&g_hookSlot10); }
