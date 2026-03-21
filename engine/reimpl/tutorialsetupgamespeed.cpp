// engine/reimpl/tutorialsetupgamespeed.cpp
// Reimplementation of TutorialSetupGameSpeed vtable methods from Application-x64.exe.
//
// TutorialSetupGameSpeed (0x28 bytes), vtable at 0x140160da0.
// Extends TutorialSetup.
//
// Vtable layout (12 slots):
//   +0x00: 0x140028ac0 — Dtor (19b prologue)
//   +0x08: 0x1400289b0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x1400289a0 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400e2df0 — GetDescription (6b, unhookable, returns 1)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400e2d90 — Serialize (15b prologue)
//   +0x30: 0x140028a70 — Cleanup (15b prologue)
//   +0x38: 0x140001640 — Base dispatch (8b, unhookable)
//   +0x40: 0x1400e2d60 — Slot8 (17b prologue)
//   +0x48: 0x1400e2d50 — Slot9 (3b, unhookable: RET 0)
//   +0x50: 0x1400e2d40 — Slot10 (3b, unhookable: RET 0)
//   +0x58: 0x140028a20 — Ctor/Init (15b prologue)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "TutorialSetups.h"
#include "Tutorial/tutorialsetupgamespeed_funcs.h"
#include <cstdio>
#include <cstdint>
#include <windows.h>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialSetupGameSpeed::Dtor";
    return TutorialSetupGameSpeed_Dtor(self, flags);
}

void InstallTutorialSetupGameSpeedDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028ac0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[TutorialSetupGameSpeed] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupGameSpeedDtor() { RemoveHook(&g_hookDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "TutorialSetupGameSpeed::Serialize";
    TutorialSetupGameSpeed_Serialize(self, stream);
}

void InstallTutorialSetupGameSpeedSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e2d90);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TutorialSetupGameSpeed] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupGameSpeedSerialize() { RemoveHook(&g_hookSerialize); }

// --- Slot6: Cleanup ---
static HookInfo g_hookCleanup;
typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "TutorialSetupGameSpeed::Cleanup";
    TutorialSetupGameSpeed_Cleanup(self);
}

void InstallTutorialSetupGameSpeedCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028a70);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[TutorialSetupGameSpeed] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupGameSpeedCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Apply game speed ---
// RE-ENABLED: RIP-relative MOV at byte 0 in the 17-byte prologue. Use vtable patch.
// Sets game speed in the update controller (g_scenario+0x120) from self->speed (+0x20).
static void* g_savedSlot8 = nullptr;
typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    g_lastHook = "TutorialSetupGameSpeed::Slot8";
    TutorialSetupGameSpeed_Slot8(self);
}

static void PatchVtableSlot_GSSpeed(size_t slotIndex, void* newFunc, void** outOld) {
    uintptr_t* entry = (uintptr_t*)(ResolveAddr(Module::Exe, TutorialSetupGameSpeedAddr::Vtable) + slotIndex * 8);
    DWORD oldProt;
    VirtualProtect(entry, 8, PAGE_READWRITE, &oldProt);
    if (outOld) *outOld = (void*)*entry;
    *entry = (uintptr_t)newFunc;
    VirtualProtect(entry, 8, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), entry, 8);
}

void InstallTutorialSetupGameSpeedSlot8() {
    PatchVtableSlot_GSSpeed(8, (void*)Reimpl_Slot8, &g_savedSlot8);
#ifdef _DEBUG
    printf("[TutorialSetupGameSpeed] Slot8 vtable-patched (slot 8)\n");
#endif
}
void RemoveTutorialSetupGameSpeedSlot8() {
    if (g_savedSlot8) {
        PatchVtableSlot_GSSpeed(8, g_savedSlot8, nullptr);
        g_savedSlot8 = nullptr;
    }
}

// --- Slot11: Ctor/Init ---
static HookInfo g_hookCtor;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    g_lastHook = "TutorialSetupGameSpeed::Ctor";
    return TutorialSetupGameSpeed_Ctor(self);
}

void InstallTutorialSetupGameSpeedCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028a20);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 15);
#ifdef _DEBUG
    printf("[TutorialSetupGameSpeed] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupGameSpeedCtor() { RemoveHook(&g_hookCtor); }
