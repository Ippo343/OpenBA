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
#include "TutorialSetups.h"
#include <cstdio>
#include <cstdint>
#include <windows.h>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialSetupRestrictPlayer::Dtor";
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
    g_lastHook = "TutorialSetupRestrictPlayer::Serialize";
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
    g_lastHook = "TutorialSetupRestrictPlayer::Cleanup";
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

// --- Slot8/Slot10/Ctor: vtable patch helpers ---
static void PatchVtableSlot_RestrictPlayer(size_t slotIndex, void* newFunc, void** outOld) {
    uintptr_t* entry = (uintptr_t*)(ResolveAddr(Module::Exe, TutorialSetupRestrictPlayerAddr::Vtable) + slotIndex * 8);
    DWORD oldProt;
    VirtualProtect(entry, 8, PAGE_READWRITE, &oldProt);
    if (outOld) *outOld = (void*)*entry;
    *entry = (uintptr_t)newFunc;
    VirtualProtect(entry, 8, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), entry, 8);
}

// --- Slot8: Slot8 ---
// RE-ENABLED: MOV R11,RSP at byte 0 breaks trampoline. Use vtable patch on slot 8.
// Sets player restriction bounds in the game camera/input state.
static void* g_savedSlot8 = nullptr;
typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    g_lastHook = "TutorialSetupRestrictPlayer::Slot8";
    ((fn_Slot8_t)g_savedSlot8)(self);
}

void InstallTutorialSetupRestrictPlayerSlot8() {
    PatchVtableSlot_RestrictPlayer(8, (void*)Reimpl_Slot8, &g_savedSlot8);
#ifdef _DEBUG
    printf("[TutorialSetupRestrictPlayer] Slot8 vtable-patched (slot 8)\n");
#endif
}
void RemoveTutorialSetupRestrictPlayerSlot8() {
    if (g_savedSlot8) {
        PatchVtableSlot_RestrictPlayer(8, g_savedSlot8, nullptr);
        g_savedSlot8 = nullptr;
    }
}

// --- Slot10: Slot10 ---
// RE-ENABLED: RIP-relative MOV at byte 3 breaks trampoline. Use vtable patch on slot 10.
// Clears player restriction state in the game camera/input state.
static void* g_savedSlot10 = nullptr;
typedef void (*fn_Slot10_t)(void* self);
static void Reimpl_Slot10(void* self) {
    g_lastHook = "TutorialSetupRestrictPlayer::Slot10";
    ((fn_Slot10_t)g_savedSlot10)(self);
}

void InstallTutorialSetupRestrictPlayerSlot10() {
    PatchVtableSlot_RestrictPlayer(10, (void*)Reimpl_Slot10, &g_savedSlot10);
#ifdef _DEBUG
    printf("[TutorialSetupRestrictPlayer] Slot10 vtable-patched (slot 10)\n");
#endif
}
void RemoveTutorialSetupRestrictPlayerSlot10() {
    if (g_savedSlot10) {
        PatchVtableSlot_RestrictPlayer(10, g_savedSlot10, nullptr);
        g_savedSlot10 = nullptr;
    }
}

// --- Slot11: Ctor/Init ---
// RE-ENABLED: near Jcc wrapper at 0x140028b80. Use vtable patch on slot 11.
static void* g_savedCtor = nullptr;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    g_lastHook = "TutorialSetupRestrictPlayer::Ctor";
    return ((fn_Ctor_t)g_savedCtor)(self);
}

void InstallTutorialSetupRestrictPlayerCtor() {
    PatchVtableSlot_RestrictPlayer(11, (void*)Reimpl_Ctor, &g_savedCtor);
#ifdef _DEBUG
    printf("[TutorialSetupRestrictPlayer] Ctor vtable-patched (slot 11)\n");
#endif
}
void RemoveTutorialSetupRestrictPlayerCtor() {
    if (g_savedCtor) {
        PatchVtableSlot_RestrictPlayer(11, g_savedCtor, nullptr);
        g_savedCtor = nullptr;
    }
}
