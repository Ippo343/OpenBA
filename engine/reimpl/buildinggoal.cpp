// engine/reimpl/buildinggoal.cpp
// Reimplementation of BuildingGoal vtable methods from Application-x64.exe.
//
// BuildingGoal (0x48 bytes), vtable at 0x140160300.
// Extends Achievement::Goal.
//
// Vtable layout (11 slots):
//   +0x00: 0x140025e80 — Dtor(self, flags)
//   +0x08: 0x140025d70 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140025d60 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400d7790 — Slot3/Serialize? (6b, unhookable: mov eax,2; ret)
//   +0x20: 0x14003a912 — Slot4 base (6b JMP thunk, unhookable)
//   +0x28: 0x1400d76a0 — Slot5/Load
//   +0x30: 0x140025e30 — Slot6/Cleanup
//   +0x38: 0x140001640 — Slot7 base (7b, unhookable)
//   +0x40: 0x1400d7580 — Slot8/Serialize
//   +0x48: 0x14003ab5e — Slot9 base (not a function entry)
//   +0x50: 0x140025de0 — Slot10/CreateInstance

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
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
// Original function typedefs
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
typedef void  (*fn_Slot5_t)(void* self, void* stream);
typedef void  (*fn_Slot6_t)(void* self);
typedef void  (*fn_Slot8_t)(void* self, void* stream);
typedef void  (*fn_Slot10_t)(void* self);

// ============================================================
// Passthrough wrappers
// ============================================================
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "BuildingGoal::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

static void Reimpl_Slot5(void* self, void* stream) {
    g_lastHook = "BuildingGoal::Slot5";
    ((fn_Slot5_t)g_hookSlot5.trampoline)(self, stream);
}

static void Reimpl_Slot6(void* self) {
    g_lastHook = "BuildingGoal::Slot6";
    ((fn_Slot6_t)g_hookSlot6.trampoline)(self);
}

static void Reimpl_Slot8(void* self, void* stream) {
    g_lastHook = "BuildingGoal::Slot8";
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self, stream);
}

static void Reimpl_Slot10(void* self) {
    g_lastHook = "BuildingGoal::Slot10";
    ((fn_Slot10_t)g_hookSlot10.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallBuildingGoalDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140025e80);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(1) + sub rsp,0x30(4) + mov qword,-2(9) = 19 -> 15
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[BuildingGoal] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildingGoalDtor() { RemoveHook(&g_hookDtor); }

void InstallBuildingGoalSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400d76a0);
    // Prologue: push rdi(2) + sub rsp,0x40(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 15);
#ifdef _DEBUG
    printf("[BuildingGoal] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildingGoalSlot5() { RemoveHook(&g_hookSlot5); }

void InstallBuildingGoalSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140025e30);
    // Prologue: push rbx(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 15);
#ifdef _DEBUG
    printf("[BuildingGoal] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildingGoalSlot6() { RemoveHook(&g_hookSlot6); }

void InstallBuildingGoalSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400d7580);
    // Prologue: mov r11,rsp(3) + push rbp(1) + push rsi(1) + push rdi(1) + push r14(2) + push r15(2) + sub rsp,0x40(4) = 14
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 14);
#ifdef _DEBUG
    printf("[BuildingGoal] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildingGoalSlot8() { RemoveHook(&g_hookSlot8); }

void InstallBuildingGoalSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140025de0);
    // Prologue: push rbx(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 15);
#ifdef _DEBUG
    printf("[BuildingGoal] Slot10 hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildingGoalSlot10() { RemoveHook(&g_hookSlot10); }
