// engine/reimpl/finishtutorialgoal.cpp
// Reimplementation of FinishTutorialGoal vtable methods from Application-x64.exe.
//
// FinishTutorialGoal (0x38 bytes), vtable at 0x140160420.
// Extends Achievement::Goal.
//
// Vtable layout (12 slots):
//   +0x00: 0x140026260 — Dtor(self, flags)
//   +0x08: 0x1400260e0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x1400260d0 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400d79d0 — Slot3 (6b, unhookable: mov eax,1; ret)
//   +0x20: 0x14003a912 — Slot4 base (6b JMP thunk, unhookable)
//   +0x28: 0x1400d7970 — Slot5/Load
//   +0x30: 0x1400261a0 — Slot6/Cleanup
//   +0x38: 0x140001640 — Slot7 base (7b, unhookable)
//   +0x40: 0x1400d7960 — Slot8 (3b, unhookable: xor eax,eax; ret)
//   +0x48: 0x1400261f0 — Slot9 (12b, unhookable: mov+mov+mov+ret = too short to safely hook)
//   +0x50: 0x140026150 — Slot10/CreateInstance
//   +0x58: 0x140026200 — Slot11

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Tutorial/finishtutorialgoal_funcs.h"
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
// Reimpl wrappers — delegate to clean code in code/Game/Goal/
// ============================================================
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "FinishTutorialGoal::Dtor";
    return FinishTutorialGoal_Dtor((FinishTutorialGoal*)self, flags);
}

static int Reimpl_Slot5(void* self, void* stream) {
    g_lastHook = "FinishTutorialGoal::Slot5";
    return FinishTutorialGoal_Slot5((FinishTutorialGoal*)self, stream);
}

static void* Reimpl_Slot6(void* self) {
    g_lastHook = "FinishTutorialGoal::Slot6";
    return FinishTutorialGoal_Slot6((FinishTutorialGoal*)self);
}

static void* Reimpl_Slot10(void* self) {
    g_lastHook = "FinishTutorialGoal::Slot10";
    return FinishTutorialGoal_CreateInstance((FinishTutorialGoal*)self);
}

static void* Reimpl_Slot11(void* self, uint32_t flags) {
    g_lastHook = "FinishTutorialGoal::Slot11";
    return FinishTutorialGoal_Slot11(self, flags);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallFinishTutorialGoalDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026260);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[FinishTutorialGoal] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveFinishTutorialGoalDtor() { RemoveHook(&g_hookDtor); }

void InstallFinishTutorialGoalSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400d7970);
    // Prologue: push rdi(2) + sub rsp,0x40(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 15);
#ifdef _DEBUG
    printf("[FinishTutorialGoal] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveFinishTutorialGoalSlot5() { RemoveHook(&g_hookSlot5); }

void InstallFinishTutorialGoalSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400261a0);
    // Prologue: push rbx(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 15);
#ifdef _DEBUG
    printf("[FinishTutorialGoal] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveFinishTutorialGoalSlot6() { RemoveHook(&g_hookSlot6); }

void InstallFinishTutorialGoalSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026150);
    // Prologue: push rbx(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 15);
#ifdef _DEBUG
    printf("[FinishTutorialGoal] Slot10 hooked at %p\n", (void*)target);
#endif
}
void RemoveFinishTutorialGoalSlot10() { RemoveHook(&g_hookSlot10); }

void InstallFinishTutorialGoalSlot11() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026200);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot11, &g_hookSlot11, 19);
#ifdef _DEBUG
    printf("[FinishTutorialGoal] Slot11 hooked at %p\n", (void*)target);
#endif
}
void RemoveFinishTutorialGoalSlot11() { RemoveHook(&g_hookSlot11); }
