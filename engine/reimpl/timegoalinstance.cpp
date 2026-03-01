// engine/reimpl/timegoalinstance.cpp
// Reimplementation of TimeGoalInstance vtable methods from Application-x64.exe.
//
// TimeGoalInstance (0x28 bytes), vtable at 0x140160598.
// Extends Achievement::GoalInstance.
//
// Vtable layout (7 slots):
//   +0x00: 0x140026800 — Dtor(self, flags) (SHARED with TimeGoal Slot11)
//   +0x08: 0x140026660 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140026650 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400dad30 — Slot3/Serialize
//   +0x20: 0x1400daca0 — Slot4 (3b, unhookable: ret 0x0)
//   +0x28: 0x1400dac30 — Slot5/Startup
//   +0x30: 0x1400dacb0 — Slot6/Shutdown

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

// ============================================================
// Original function typedefs
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
typedef void  (*fn_Slot3_t)(void* self, void* stream);
typedef void  (*fn_Slot5_t)(void* self, void* param2);
typedef void  (*fn_Slot6_t)(void* self);

// ============================================================
// Passthrough wrappers
// ============================================================
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TimeGoalInstance::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

static void Reimpl_Slot3(void* self, void* stream) {
    g_lastHook = "TimeGoalInstance::Slot3";
    ((fn_Slot3_t)g_hookSlot3.trampoline)(self, stream);
}

static void Reimpl_Slot5(void* self, void* param2) {
    g_lastHook = "TimeGoalInstance::Slot5";
    ((fn_Slot5_t)g_hookSlot5.trampoline)(self, param2);
}

static void Reimpl_Slot6(void* self) {
    g_lastHook = "TimeGoalInstance::Slot6";
    ((fn_Slot6_t)g_hookSlot6.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTimeGoalInstanceDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026800);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[TimeGoalInstance] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTimeGoalInstanceDtor() { RemoveHook(&g_hookDtor); }

void InstallTimeGoalInstanceSlot3() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dad30);
    // Prologue: push rdi(2) + sub rsp,0x40(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot3, &g_hookSlot3, 15);
#ifdef _DEBUG
    printf("[TimeGoalInstance] Slot3 hooked at %p\n", (void*)target);
#endif
}
void RemoveTimeGoalInstanceSlot3() { RemoveHook(&g_hookSlot3); }

void InstallTimeGoalInstanceSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dac30);
    // Prologue: mov [rsp+0x10],rbx(5) + mov [rsp+0x18],rsi(5) + push rdi(2) + sub rsp,0x20(4) = 16 -> 15
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 15);
#ifdef _DEBUG
    printf("[TimeGoalInstance] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveTimeGoalInstanceSlot5() { RemoveHook(&g_hookSlot5); }

void InstallTimeGoalInstanceSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dacb0);
    // Prologue: mov [rsp+8],rbx(5) + mov [rsp+0x10],rbp(5) + mov [rsp+0x18],rsi(5) + push rdi(2) + sub rsp,0x20(4) = 21 -> 20
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 20);
#ifdef _DEBUG
    printf("[TimeGoalInstance] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveTimeGoalInstanceSlot6() { RemoveHook(&g_hookSlot6); }
