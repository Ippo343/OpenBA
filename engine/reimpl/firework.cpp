// engine/reimpl/firework.cpp
// Reimplementation of FireWork vtable methods from Application-x64.exe.
//
// FireWork is a Work subclass (0x60 bytes) for fire suppression work.
// Has a BST/map at +0x50 and a field at +0x58. Note: +0x48 is NOT initialized
// by the constructor (gap from Work base alignment).
//
// Vtable at 0x1401612a0, 12 slots:
//   +0x00: ScalarDtor (0x14002a230)   — tiny, not hooked
//   +0x08: VectorDtor (0x14002a1d0)   — tiny, not hooked
//   +0x10: BaseDtor   (0x14002a1c0)   — tiny, not hooked
//   +0x18: Serialize  (0x1400e9a50)   — HOOKED (full reimpl)
//   +0x20: Slot4      (0x1400e9620)   — HOOKED (passthrough, complex fire logic)
//   +0x28: Execute    (0x1400f7540)   — 3 bytes, unhookable
//   +0x30: Slot6      (0x1400e9600)   — 20 bytes total, HOOKED (passthrough)
//   +0x38: Slot7      (0x1400e95f0)   — not found as function in Ghidra
//   +0x40: Slot8      (0x1400e99a0)   — HOOKED (passthrough, cleanup loop)
//   +0x48: Slot9      (0x1400e93c0)   — HOOKED (passthrough, complex fire AI)
//   +0x50: Slot10     (0x1400e9a30)   — not found as function in Ghidra
//   +0x58: Slot11     (0x14002a220)   — 9 bytes, unhookable (checks +0x58 > 0)

#include "FireWork.h"
#include "Work/firework_funcs.h"
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

// ============================================================
// Reimplementations — delegate to clean game logic layer
// ============================================================

// Serialize (0x1400e9a50) — fully reimplemented in code/Game/Work/firework.cpp
static void Reimpl_Serialize(FireWork* self, void* stream) {
    g_lastHook = "FireWork::Serialize";
    FireWork_Serialize_Impl(self, stream);
}

// Slot4 (0x1400e9620) — fire suppression logic, delegates to clean layer
// Clean layer currently delegates to original exe code for complex sub-calls.
static int Reimpl_Slot4(FireWork* self, void* param2, void* param3) {
    g_lastHook = "FireWork::Slot4";
    return FireWork_Slot4_Impl(self, param2, param3);
}

// Slot6 (0x1400e9600) — tree remove, fully reimplemented in clean layer
typedef void (*fn_Slot6_t)(void* self, void* param2);
static void Reimpl_Slot6(void* self, void* param2) {
    g_lastHook = "FireWork::Slot6";
    // Slot6 hook is disabled (bug class 2), but reimpl exists for standalone exe.
    // When hook is active, delegate to clean code; when disabled, trampoline is used.
    ((fn_Slot6_t)g_hookSlot6.trampoline)(self, param2);
}

// Slot8 (0x1400e99a0) — cleanup loop, delegates to clean layer
static void Reimpl_Slot8(FireWork* self) {
    g_lastHook = "FireWork::Slot8";
    FireWork_Slot8_Impl(self);
}

// Slot9 (0x1400e93c0) — validation, delegates to clean layer
// Clean layer currently delegates to original exe code.
typedef uint64_t (*fn_Slot9_t)(void* self, void* param2);
static uint64_t Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "FireWork::Slot9";
    return ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallFireWorkSerialize() {
    // Resolve all clean-layer function pointers
    if (!FireWork_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[FireWork] ERROR: failed to resolve clean layer funcs, skipping Serialize hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9a50);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[FireWork] Serialize hooked at %p\n", (void*)target);
#endif
}

void RemoveFireWorkSerialize() { RemoveHook(&g_hookSerialize); }

void InstallFireWorkSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9620);
    // Prologue: MOV [RSP+0x18],R8(5) + MOV [RSP+0x10],RDX(5) + PUSH RBP(2) + PUSH RBX(2) = 14 -> corrected to 15
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[FireWork] Slot4 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFireWorkSlot4() { RemoveHook(&g_hookSlot4); }

void InstallFireWorkSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9600);
    // Prologue: SUB RSP,0x28(4) + ADD RCX,0x50(4) + CALL(5) = 13
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 13);
#ifdef _DEBUG
    printf("[FireWork] Slot6 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFireWorkSlot6() { RemoveHook(&g_hookSlot6); }

void InstallFireWorkSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e99a0);
    // Prologue: MOV [RSP+0x8],RBX(5) + PUSH RDI(2) + SUB RSP,0x20(4) + MOV RDI,RCX(3) = 14 -> corrected to 16
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 16);
#ifdef _DEBUG
    printf("[FireWork] Slot8 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFireWorkSlot8() { RemoveHook(&g_hookSlot8); }

void InstallFireWorkSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e93c0);
    // Prologue: MOV RAX,RSP(3) + PUSH RBP(2) + PUSH RSI(2) + PUSH RDI(2) + PUSH R12(2) + PUSH R13(2) = 13
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 14);
#ifdef _DEBUG
    printf("[FireWork] Slot9 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFireWorkSlot9() { RemoveHook(&g_hookSlot9); }
