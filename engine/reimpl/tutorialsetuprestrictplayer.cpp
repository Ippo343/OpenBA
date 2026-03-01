// engine/reimpl/tutorialsetuprestrictplayer.cpp
// Reimplementation of TutorialSetupRestrictPlayer vtable methods from Application-x64.exe.
//
// TutorialSetupRestrictPlayer (0x68 bytes), vtable at 0x140160e20.
// Extends TutorialSetup. Ctor at 0x1400e3220.
//
// Vtable layout (12 slots):
//   +0x00: 0x140028bb0 — Dtor (19b prologue)
//   +0x08: 0x140028b30 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140028b20 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400e31b0 — GetDescription (6b, unhookable, returns 4)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400e3090 — Serialize (15b prologue)
//   +0x30: 0x140028b90 — Cleanup (14b prologue)
//   +0x38: 0x140001640 — Base dispatch (8b, unhookable)
//   +0x40: 0x1400e2f20 — Slot8 (14b prologue)
//   +0x48: 0x1400e2f10 — Slot9 (3b, unhookable: RET 0)
//   +0x50: 0x1400e2e90 — Slot10 (13b prologue)
//   +0x58: 0x140028b80 — Ctor/Init (12b prologue)

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

void InstallTutorialSetupRestrictPlayerDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028bb0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[TutorialSetupRestrictPlayer] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupRestrictPlayerDtor() { RemoveHook(&g_hookDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

void InstallTutorialSetupRestrictPlayerSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e3090);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TutorialSetupRestrictPlayer] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupRestrictPlayerSerialize() { RemoveHook(&g_hookSerialize); }

// --- Slot6: Cleanup ---
static HookInfo g_hookCleanup;
typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

void InstallTutorialSetupRestrictPlayerCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028b90);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[TutorialSetupRestrictPlayer] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupRestrictPlayerCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Slot8 ---
static HookInfo g_hookSlot8;
typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

void InstallTutorialSetupRestrictPlayerSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e2f20);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 14);
#ifdef _DEBUG
    printf("[TutorialSetupRestrictPlayer] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupRestrictPlayerSlot8() { RemoveHook(&g_hookSlot8); }

// --- Slot10: Slot10 ---
static HookInfo g_hookSlot10;
typedef void (*fn_Slot10_t)(void* self);
static void Reimpl_Slot10(void* self) {
    ((fn_Slot10_t)g_hookSlot10.trampoline)(self);
}

void InstallTutorialSetupRestrictPlayerSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e2e90);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 13);
#ifdef _DEBUG
    printf("[TutorialSetupRestrictPlayer] Slot10 hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupRestrictPlayerSlot10() { RemoveHook(&g_hookSlot10); }

// --- Slot11: Ctor/Init ---
static HookInfo g_hookCtor;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    return ((fn_Ctor_t)g_hookCtor.trampoline)(self);
}

void InstallTutorialSetupRestrictPlayerCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028b80);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 12);
#ifdef _DEBUG
    printf("[TutorialSetupRestrictPlayer] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupRestrictPlayerCtor() { RemoveHook(&g_hookCtor); }
