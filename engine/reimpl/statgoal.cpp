// engine/reimpl/statgoal.cpp
// Reimplementation of StatGoal vtable methods from Application-x64.exe.
//
// StatGoal (0x80 bytes), vtable at 0x1401604c0.
// Extends Achievement::Goal.
//
// Vtable layout (11 slots):
//   +0x00: 0x140026540 — Dtor(self, flags)
//   +0x08: 0x1400264c0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x1400264b0 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400daa10 — Slot3 (6b, unhookable: mov eax,1; ret)
//   +0x20: 0x14003a912 — Slot4 base (6b JMP thunk, unhookable)
//   +0x28: 0x1400da8b0 — Slot5/Load
//   +0x30: 0x140026520 — Slot6/Cleanup
//   +0x38: 0x140001640 — Slot7 base (7b, unhookable)
//   +0x40: 0x1400da720 — Slot8/Check
//   +0x48: 0x14003ab5e — Slot9 base (not a function entry)
//   +0x50: 0x140026510 — Slot10/CreateInstance

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Goal/statgoal_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSlot5;
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot10;

// ============================================================
// Reimplementation wrappers — delegate to clean code
// ============================================================
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "StatGoal::Dtor";
    return StatGoal_Dtor((StatGoal*)self, flags);
}

static void Reimpl_Slot5(void* self, void* stream) {
    g_lastHook = "StatGoal::Slot5";
    StatGoal_Slot5((StatGoal*)self, stream);
}

static void Reimpl_Slot6(void* self) {
    g_lastHook = "StatGoal::Slot6";
    StatGoal_Slot6((StatGoal*)self);
}

static int Reimpl_Slot8(void* self) {
    g_lastHook = "StatGoal::Slot8";
    return StatGoal_Slot8((StatGoal*)self);
}

static void* Reimpl_Slot10(void* self) {
    g_lastHook = "StatGoal::Slot10";
    return StatGoal_Slot10(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallStatGoalDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026540);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[StatGoal] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveStatGoalDtor() { RemoveHook(&g_hookDtor); }

void InstallStatGoalSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400da8b0);
    // Prologue: push rdi(2) + sub rsp,0x40(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 15);
#ifdef _DEBUG
    printf("[StatGoal] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveStatGoalSlot5() { RemoveHook(&g_hookSlot5); }

void InstallStatGoalSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026520);
    // Prologue: push rbx(2) + sub rsp,0x20(4) + mov rax,[rcx](3) + xor edx,edx(2) + mov rbx,rcx(3) = 14
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 14);
#ifdef _DEBUG
    printf("[StatGoal] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveStatGoalSlot6() { RemoveHook(&g_hookSlot6); }

void InstallStatGoalSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400da720);
    // Prologue: push rdi(2) + sub rsp,0x40(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[StatGoal] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveStatGoalSlot8() { RemoveHook(&g_hookSlot8); }

void InstallStatGoalSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026510);
    // Prologue: test rcx,rcx(3) + jnz(6) + xor eax,eax(2) + ret(1) = 12
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 12);
#ifdef _DEBUG
    printf("[StatGoal] Slot10 hooked at %p\n", (void*)target);
#endif
}
void RemoveStatGoalSlot10() { RemoveHook(&g_hookSlot10); }
