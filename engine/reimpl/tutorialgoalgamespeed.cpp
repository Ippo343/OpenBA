// engine/reimpl/tutorialgoalgamespeed.cpp
// Reimplementation of TutorialGoalGameSpeed vtable methods from Application-x64.exe.
//
// TutorialGoalGameSpeed (0x28 bytes), vtable at 0x1401608e0.
// Extends TutorialGoal.
//
// Vtable layout (10 slots):
//   +0x00: 0x140027730 — Dtor (19b prologue)
//   +0x08: 0x140027620 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140027610 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400df1f0 — GetDescription (6b, unhookable, returns 1)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400df170 — Serialize (15b prologue)
//   +0x30: 0x1400276e0 — Cleanup (15b prologue)
//   +0x38: 0x140001640 — Base dispatch (8b, unhookable)
//   +0x40: 0x1400df150 — Slot8 (17b prologue)
//   +0x48: 0x140027690 — Ctor/Init (15b prologue)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

void InstallTutorialGoalGameSpeedDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027730);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[TutorialGoalGameSpeed] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalGameSpeedDtor() { RemoveHook(&g_hookDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

void InstallTutorialGoalGameSpeedSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400df170);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TutorialGoalGameSpeed] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalGameSpeedSerialize() { RemoveHook(&g_hookSerialize); }

// --- Slot6: Cleanup ---
static HookInfo g_hookCleanup;
typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

void InstallTutorialGoalGameSpeedCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400276e0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[TutorialGoalGameSpeed] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalGameSpeedCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Slot8 ---
static HookInfo g_hookSlot8;
typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

void InstallTutorialGoalGameSpeedSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400df150);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 17);
#ifdef _DEBUG
    printf("[TutorialGoalGameSpeed] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalGameSpeedSlot8() { RemoveHook(&g_hookSlot8); }

// --- Slot9: Ctor/Init ---
static HookInfo g_hookCtor;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    return ((fn_Ctor_t)g_hookCtor.trampoline)(self);
}

void InstallTutorialGoalGameSpeedCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027690);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 15);
#ifdef _DEBUG
    printf("[TutorialGoalGameSpeed] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalGameSpeedCtor() { RemoveHook(&g_hookCtor); }
