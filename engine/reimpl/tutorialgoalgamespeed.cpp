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
#include "TutorialGoals.h"
#include "Tutorial/tutorialgoalgamespeed_funcs.h"
#include <cstdio>
#include <cstdint>
#include <windows.h>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialGoalGameSpeed::Dtor";
    return TutorialGoalGameSpeed_Dtor(self, flags);
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
    g_lastHook = "TutorialGoalGameSpeed::Serialize";
    TutorialGoalGameSpeed_Serialize(self, stream);
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
    g_lastHook = "TutorialGoalGameSpeed::Cleanup";
    TutorialGoalGameSpeed_Cleanup(self);
}

void InstallTutorialGoalGameSpeedCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400276e0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[TutorialGoalGameSpeed] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalGameSpeedCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Check ---
// RE-ENABLED: RIP-relative at byte 0 breaks trampoline. Use vtable patch instead.
// Vtable slot 8 (+0x40) of TutorialGoalGameSpeed.
// Now calls clean reimplementation (TutorialGoalGameSpeed_Check).
static void* g_savedSlot8 = nullptr;
typedef int (*fn_Check_t)(void* self);
static int Reimpl_Check(void* self) {
    g_lastHook = "TutorialGoalGameSpeed::Check";
    return TutorialGoalGameSpeed_Check((TutorialGoalGameSpeed*)self);
}

static void PatchVtableSlot_GameSpeed(size_t slotIndex, void* newFunc, void** outOld) {
    uintptr_t* entry = (uintptr_t*)(ResolveAddr(Module::Exe, TutorialGoalGameSpeedAddr::Vtable) + slotIndex * 8);
    DWORD oldProt;
    VirtualProtect(entry, 8, PAGE_READWRITE, &oldProt);
    if (outOld) *outOld = (void*)*entry;
    *entry = (uintptr_t)newFunc;
    VirtualProtect(entry, 8, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), entry, 8);
}

void InstallTutorialGoalGameSpeedSlot8() {
    PatchVtableSlot_GameSpeed(8, (void*)Reimpl_Check, &g_savedSlot8);
#ifdef _DEBUG
    printf("[TutorialGoalGameSpeed] Check vtable-patched (slot 8)\n");
#endif
}
void RemoveTutorialGoalGameSpeedSlot8() {
    if (g_savedSlot8) {
        PatchVtableSlot_GameSpeed(8, g_savedSlot8, nullptr);
        g_savedSlot8 = nullptr;
    }
}

// --- Slot9: Init ---
static HookInfo g_hookCtor;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    g_lastHook = "TutorialGoalGameSpeed::Init";
    return TutorialGoalGameSpeed_Init(self);
}

void InstallTutorialGoalGameSpeedCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027690);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 15);
#ifdef _DEBUG
    printf("[TutorialGoalGameSpeed] Init hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalGameSpeedCtor() { RemoveHook(&g_hookCtor); }
