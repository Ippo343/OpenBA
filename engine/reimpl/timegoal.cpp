// engine/reimpl/timegoal.cpp
// Reimplementation of TimeGoal vtable methods from Application-x64.exe.
//
// TimeGoal (0x38 bytes), vtable at 0x140160540.
// Extends Achievement::Goal.
//
// Vtable layout (12 slots):
//   +0x00: 0x140026850 — Dtor(self, flags)
//   +0x08: 0x1400266e0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x1400266d0 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400daba0 — Slot3 (6b, unhookable: mov eax,1; ret)
//   +0x20: 0x14003a912 — Slot4 base (6b JMP thunk, unhookable)
//   +0x28: 0x1400dab20 — Slot5/Load
//   +0x30: 0x1400267a0 — Slot6/Cleanup
//   +0x38: 0x140001640 — Slot7 base (7b, unhookable)
//   +0x40: 0x1400dab10 — Slot8 (3b, unhookable: xor eax,eax; ret)
//   +0x48: 0x1400267f0 — Slot9 (12b, unhookable: mov+mov+mov+ret)
//   +0x50: 0x140026750 — Slot10/CreateInstance
//   +0x58: 0x140026800 — Slot11

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Goal/timegoal_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSlot5;
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot10;
static HookInfo g_hookSlot11;

// ============================================================
// Reimplementation wrappers — delegate to clean code
// ============================================================
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TimeGoal::Dtor";
    return TimeGoal_Dtor((TimeGoal*)self, flags);
}

static void Reimpl_Slot5(void* self, void* stream) {
    g_lastHook = "TimeGoal::Slot5";
    TimeGoal_Slot5((TimeGoal*)self, stream);
}

static void Reimpl_Slot6(void* self) {
    g_lastHook = "TimeGoal::Slot6";
    TimeGoal_Slot6((TimeGoal*)self);
}

static void* Reimpl_Slot10(void* self) {
    g_lastHook = "TimeGoal::Slot10";
    return TimeGoal_Slot10(self);
}

static void* Reimpl_Slot11(void* self, uint32_t flags) {
    g_lastHook = "TimeGoal::Slot11";
    return TimeGoal_Slot11(self, flags);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTimeGoalDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026850);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[TimeGoal] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTimeGoalDtor() { RemoveHook(&g_hookDtor); }

void InstallTimeGoalSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dab20);
    // Prologue: push rdi(2) + sub rsp,0x40(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 15);
#ifdef _DEBUG
    printf("[TimeGoal] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveTimeGoalSlot5() { RemoveHook(&g_hookSlot5); }

void InstallTimeGoalSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400267a0);
    // Prologue: push rbx(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 15);
#ifdef _DEBUG
    printf("[TimeGoal] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveTimeGoalSlot6() { RemoveHook(&g_hookSlot6); }

void InstallTimeGoalSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026750);
    // Prologue: push rbx(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 15);
#ifdef _DEBUG
    printf("[TimeGoal] Slot10 hooked at %p\n", (void*)target);
#endif
}
void RemoveTimeGoalSlot10() { RemoveHook(&g_hookSlot10); }

void InstallTimeGoalSlot11() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026800);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot11, &g_hookSlot11, 19);
#ifdef _DEBUG
    printf("[TimeGoal] Slot11 hooked at %p\n", (void*)target);
#endif
}
void RemoveTimeGoalSlot11() { RemoveHook(&g_hookSlot11); }
