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
#include "TutorialGoals.h"
#include <cstdio>
#include <cstdint>
#include <windows.h>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialGoalCount::Dtor";
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
    g_lastHook = "TutorialGoalCount::Serialize";
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
    g_lastHook = "TutorialGoalCount::Cleanup";
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

void InstallTutorialGoalCountCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027800);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 16);
#ifdef _DEBUG
    printf("[TutorialGoalCount] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalCountCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Slot8 ---
// RE-ENABLED: MOV R11,RSP at byte 0 breaks trampoline. Use vtable patch instead.
// Vtable slot 8 (+0x40) of TutorialGoalCount — stores g_savedSlot8 for removal.
static void* g_savedSlot8 = nullptr;
typedef void (*fn_Slot8_t)(void* self, void* param1, void* param2);
static void Reimpl_Slot8(void* self, void* param1, void* param2) {
    g_lastHook = "TutorialGoalCount::Slot8";
    ((fn_Slot8_t)g_savedSlot8)(self, param1, param2);
}

static void PatchVtableSlot_GoalCount(size_t slotIndex, void* newFunc, void** outOld) {
    uintptr_t* entry = (uintptr_t*)(ResolveAddr(Module::Exe, TutorialGoalCountAddr::Vtable) + slotIndex * 8);
    DWORD oldProt;
    VirtualProtect(entry, 8, PAGE_READWRITE, &oldProt);
    if (outOld) *outOld = (void*)*entry;
    *entry = (uintptr_t)newFunc;
    VirtualProtect(entry, 8, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), entry, 8);
}

void InstallTutorialGoalCountSlot8() {
    PatchVtableSlot_GoalCount(8, (void*)Reimpl_Slot8, &g_savedSlot8);
#ifdef _DEBUG
    printf("[TutorialGoalCount] Slot8 vtable-patched (slot 8)\n");
#endif
}
void RemoveTutorialGoalCountSlot8() {
    if (g_savedSlot8) {
        PatchVtableSlot_GoalCount(8, g_savedSlot8, nullptr);
        g_savedSlot8 = nullptr;
    }
}

// --- Slot9: Ctor/Init ---
// RE-ENABLED: near Jcc wrapper at 0x1400277f0. Hook the real Ctor body instead.
// Vtable slot 9 (+0x48) — stores g_savedCtor for removal.
static void* g_savedCtor = nullptr;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    g_lastHook = "TutorialGoalCount::Ctor";
    return ((fn_Ctor_t)g_savedCtor)(self);
}

void InstallTutorialGoalCountCtor() {
    PatchVtableSlot_GoalCount(9, (void*)Reimpl_Ctor, &g_savedCtor);
#ifdef _DEBUG
    printf("[TutorialGoalCount] Ctor vtable-patched (slot 9)\n");
#endif
}
void RemoveTutorialGoalCountCtor() {
    if (g_savedCtor) {
        PatchVtableSlot_GoalCount(9, g_savedCtor, nullptr);
        g_savedCtor = nullptr;
    }
}
