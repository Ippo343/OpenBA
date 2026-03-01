// engine/reimpl/tutorialgoalcount.cpp
// Reimplementation of TutorialGoalCount vtable methods from Application-x64.exe.
//
// TutorialGoalCount (0x50 bytes), vtable at 0x140160950.
// Extends TutorialGoal. Ctor at 0x1400dfa60.
//
// Vtable layout (10 slots):
//   +0x00: 0x140027820 — Dtor (19b prologue)
//   +0x08: 0x1400277a0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140027790 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400df9f0 — GetDescription (6b, unhookable, returns 5)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400df900 — Serialize (15b prologue)
//   +0x30: 0x140027800 — Cleanup (15b prologue)
//   +0x38: 0x140001640 — Base dispatch (8b, unhookable)
//   +0x40: 0x1400df290 — Slot8 (14b prologue)
//   +0x48: 0x1400277f0 — Ctor/Init (12b prologue)

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

void InstallTutorialGoalCountDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027820);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[TutorialGoalCount] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalCountDtor() { RemoveHook(&g_hookDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

void InstallTutorialGoalCountSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400df900);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TutorialGoalCount] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalCountSerialize() { RemoveHook(&g_hookSerialize); }

// --- Slot6: Cleanup ---
static HookInfo g_hookCleanup;
typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

void InstallTutorialGoalCountCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027800);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[TutorialGoalCount] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalCountCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Slot8 ---
static HookInfo g_hookSlot8;
typedef void (*fn_Slot8_t)(void* self, void* param1, void* param2);
static void Reimpl_Slot8(void* self, void* param1, void* param2) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self, param1, param2);
}

void InstallTutorialGoalCountSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400df290);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 14);
#ifdef _DEBUG
    printf("[TutorialGoalCount] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalCountSlot8() { RemoveHook(&g_hookSlot8); }

// --- Slot9: Ctor/Init ---
static HookInfo g_hookCtor;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    return ((fn_Ctor_t)g_hookCtor.trampoline)(self);
}

void InstallTutorialGoalCountCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400277f0);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 12);
#ifdef _DEBUG
    printf("[TutorialGoalCount] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalCountCtor() { RemoveHook(&g_hookCtor); }
