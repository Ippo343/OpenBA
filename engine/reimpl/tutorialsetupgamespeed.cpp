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
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
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
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
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
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

void InstallTutorialSetupGameSpeedCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028a70);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[TutorialSetupGameSpeed] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupGameSpeedCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Slot8 ---
static HookInfo g_hookSlot8;
typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

void InstallTutorialSetupGameSpeedSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e2d60);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 17);
#ifdef _DEBUG
    printf("[TutorialSetupGameSpeed] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupGameSpeedSlot8() { RemoveHook(&g_hookSlot8); }

// --- Slot11: Ctor/Init ---
static HookInfo g_hookCtor;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    return ((fn_Ctor_t)g_hookCtor.trampoline)(self);
}

void InstallTutorialSetupGameSpeedCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028a20);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 15);
#ifdef _DEBUG
    printf("[TutorialSetupGameSpeed] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupGameSpeedCtor() { RemoveHook(&g_hookCtor); }
