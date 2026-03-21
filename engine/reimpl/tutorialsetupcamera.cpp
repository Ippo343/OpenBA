// engine/reimpl/tutorialsetupcamera.cpp
// Reimplementation of TutorialSetupCamera vtable methods from Application-x64.exe.
//
// TutorialSetupCamera (0x30 bytes), vtable at 0x140160ca0.
// Extends TutorialSetup.
//
// Vtable layout (12 slots):
//   +0x00: 0x140028530 — Dtor (19b prologue)
//   +0x08: 0x1400283f0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x1400283e0 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400e21b0 — GetDescription (6b, unhookable, returns 1)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400e2120 — Serialize (15b prologue)
//   +0x30: 0x1400284d0 — Cleanup (15b prologue)
//   +0x38: 0x140001640 — Base dispatch (8b, unhookable)
//   +0x40: 0x1400e20d0 — Slot8 (18b prologue)
//   +0x48: 0x1400e20c0 — Slot9 (3b, unhookable: RET 0)
//   +0x50: 0x1400e20b0 — Slot10 (3b, unhookable: RET 0)
//   +0x58: 0x140028470 — Ctor/Init (15b prologue)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "TutorialSetups.h"
#include "Tutorial/tutorialsetupcamera_funcs.h"
#include <cstdio>
#include <cstdint>
#include <windows.h>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialSetupCamera::Dtor";
    return TutorialSetupCamera_Dtor(self, flags);
}

void InstallTutorialSetupCameraDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028530);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[TutorialSetupCamera] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupCameraDtor() { RemoveHook(&g_hookDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "TutorialSetupCamera::Serialize";
    TutorialSetupCamera_Serialize(self, stream);
}

void InstallTutorialSetupCameraSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e2120);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TutorialSetupCamera] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupCameraSerialize() { RemoveHook(&g_hookSerialize); }

// --- Slot6: Cleanup ---
static HookInfo g_hookCleanup;
typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "TutorialSetupCamera::Cleanup";
    TutorialSetupCamera_Cleanup(self);
}

void InstallTutorialSetupCameraCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400284d0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[TutorialSetupCamera] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupCameraCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Slot8 / Apply ---
// RE-ENABLED: RIP-relative MOV at byte 4 in the 18-byte prologue. Use vtable patch.
// TutorialSetupCamera vtable slot 8 (+0x40): sets camera position in game state.
static void* g_savedSlot8 = nullptr;
typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    g_lastHook = "TutorialSetupCamera::Slot8";
    TutorialSetupCamera_Slot8(self);
}

static void PatchVtableSlot_Camera(size_t slotIndex, void* newFunc, void** outOld) {
    uintptr_t* entry = (uintptr_t*)(ResolveAddr(Module::Exe, TutorialSetupCameraAddr::Vtable) + slotIndex * 8);
    DWORD oldProt;
    VirtualProtect(entry, 8, PAGE_READWRITE, &oldProt);
    if (outOld) *outOld = (void*)*entry;
    *entry = (uintptr_t)newFunc;
    VirtualProtect(entry, 8, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), entry, 8);
}

void InstallTutorialSetupCameraSlot8() {
    PatchVtableSlot_Camera(8, (void*)Reimpl_Slot8, &g_savedSlot8);
#ifdef _DEBUG
    printf("[TutorialSetupCamera] Slot8 vtable-patched (slot 8)\n");
#endif
}
void RemoveTutorialSetupCameraSlot8() {
    if (g_savedSlot8) {
        PatchVtableSlot_Camera(8, g_savedSlot8, nullptr);
        g_savedSlot8 = nullptr;
    }
}

// --- Slot11: Ctor/Init ---
static HookInfo g_hookCtor;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    g_lastHook = "TutorialSetupCamera::Ctor";
    return TutorialSetupCamera_Ctor(self);
}

void InstallTutorialSetupCameraCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028470);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 15);
#ifdef _DEBUG
    printf("[TutorialSetupCamera] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupCameraCtor() { RemoveHook(&g_hookCtor); }
