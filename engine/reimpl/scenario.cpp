// engine/reimpl/scenario.cpp
// Reimplementation of Scenario vtable methods from Application-x64.exe.
//
// Scenario (0x90 bytes), vtable at 0x14015f020.
// Extends Achievement::Achievement.
//
// Vtable layout (12 slots):
//   +0x00: 0x14001eb40 — Dtor(self, flags)
//   +0x08: 0x14001e9e0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x14001e9d0 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400a9460 — Slot3/Serialize (20b total, hookable)
//   +0x20: 0x14003a912 — Slot4 base (6b JMP thunk, unhookable)
//   +0x28: 0x1400a9370 — Slot5/Load
//   +0x30: 0x14001ead0 — Slot6/Cleanup
//   +0x38: 0x140001640 — Slot7 base (7b, unhookable)
//   +0x40: 0x14001ea70 — Slot8/Serialize2
//   +0x48: NULL         — Slot9 (null, skip)
//   +0x50: 0x14003addc — Slot10 base (shared, large)
//   +0x58: 0x14003addc — Slot11 base (same as slot10)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSlot3;
static HookInfo g_hookSlot5;
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot8;

// ============================================================
// Original function typedefs
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
typedef uint32_t (*fn_Slot3_t)(void* self);
typedef void  (*fn_Slot5_t)(void* self);
typedef void  (*fn_Slot6_t)(void* self);
typedef void  (*fn_Slot8_t)(void* self);

// ============================================================
// Passthrough wrappers
// ============================================================
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

static uint32_t Reimpl_Slot3(void* self) {
    return ((fn_Slot3_t)g_hookSlot3.trampoline)(self);
}

static void Reimpl_Slot5(void* self) {
    ((fn_Slot5_t)g_hookSlot5.trampoline)(self);
}

static void Reimpl_Slot6(void* self) {
    ((fn_Slot6_t)g_hookSlot6.trampoline)(self);
}

static void Reimpl_Slot8(void* self) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallScenarioDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001eb40);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 15);
#ifdef _DEBUG
    printf("[Scenario] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveScenarioDtor() { RemoveHook(&g_hookDtor); }

void InstallScenarioSlot3() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400a9460);
    // Prologue: sub rsp,0x28(4) + call [rip+xx](6) + or eax,0x600(5) = 15
    InstallHook(target, (void*)Reimpl_Slot3, &g_hookSlot3, 15);
#ifdef _DEBUG
    printf("[Scenario] Slot3 hooked at %p\n", (void*)target);
#endif
}
void RemoveScenarioSlot3() { RemoveHook(&g_hookSlot3); }

void InstallScenarioSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400a9370);
    // Prologue: push rdi(2) + sub rsp,0x40(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 15);
#ifdef _DEBUG
    printf("[Scenario] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveScenarioSlot5() { RemoveHook(&g_hookSlot5); }

void InstallScenarioSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001ead0);
    // Prologue: push rbx(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 15);
#ifdef _DEBUG
    printf("[Scenario] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveScenarioSlot6() { RemoveHook(&g_hookSlot6); }

void InstallScenarioSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001ea70);
    // Prologue: push rbx(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[Scenario] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveScenarioSlot8() { RemoveHook(&g_hookSlot8); }
