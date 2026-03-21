// engine/reimpl/tutorialgoalbutton.cpp
// Reimplementation of TutorialGoalButton vtable methods from Application-x64.exe.
//
// TutorialGoalButton (0x30 bytes), vtable at 0x140160800.
// Extends TutorialGoal.
//
// Vtable layout (10 slots):
//   +0x00: 0x1400273b0 — Dtor (19b prologue)
//   +0x08: 0x140027280 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140027270 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400dee30 — GetDescription (6b, unhookable, returns 2)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400deda0 — Serialize (15b prologue)
//   +0x30: 0x140027350 — Cleanup (15b prologue)
//   +0x38: 0x140001640 — Base dispatch (8b, unhookable)
//   +0x40: 0x1400dece0 — Slot8 (13b prologue)
//   +0x48: 0x140027300 — Ctor/Init (15b prologue)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Tutorial/tutorialgoalbutton_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialGoalButton::Dtor";
    return TutorialGoalButton_Dtor(self, flags);
}

void InstallTutorialGoalButtonDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400273b0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[TutorialGoalButton] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalButtonDtor() { RemoveHook(&g_hookDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "TutorialGoalButton::Serialize";
    TutorialGoalButton_Serialize(self, stream);
}

void InstallTutorialGoalButtonSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400deda0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TutorialGoalButton] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalButtonSerialize() { RemoveHook(&g_hookSerialize); }

// --- Slot6: Cleanup ---
static HookInfo g_hookCleanup;
typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "TutorialGoalButton::Cleanup";
    TutorialGoalButton_Cleanup(self);
}

void InstallTutorialGoalButtonCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027350);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[TutorialGoalButton] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalButtonCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Check ---
static HookInfo g_hookSlot8;
typedef int (*fn_Check_t)(void* self);
static int Reimpl_Check(void* self) {
    g_lastHook = "TutorialGoalButton::Check";
    return TutorialGoalButton_Check(self);
}

void InstallTutorialGoalButtonSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dece0);
    InstallHook(target, (void*)Reimpl_Check, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[TutorialGoalButton] Check hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalButtonSlot8() { RemoveHook(&g_hookSlot8); }

// --- Slot9: Init ---
static HookInfo g_hookCtor;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    g_lastHook = "TutorialGoalButton::Init";
    return TutorialGoalButton_Init(self);
}

void InstallTutorialGoalButtonCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027300);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 15);
#ifdef _DEBUG
    printf("[TutorialGoalButton] Init hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalButtonCtor() { RemoveHook(&g_hookCtor); }
