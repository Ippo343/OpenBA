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
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
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
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
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
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

void InstallTutorialSetupCameraCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400284d0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[TutorialSetupCamera] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupCameraCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Slot8 ---
static HookInfo g_hookSlot8;
typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

void InstallTutorialSetupCameraSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e20d0);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 18);
#ifdef _DEBUG
    printf("[TutorialSetupCamera] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupCameraSlot8() { RemoveHook(&g_hookSlot8); }

// --- Slot11: Ctor/Init ---
static HookInfo g_hookCtor;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    return ((fn_Ctor_t)g_hookCtor.trampoline)(self);
}

void InstallTutorialSetupCameraCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028470);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 15);
#ifdef _DEBUG
    printf("[TutorialSetupCamera] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupCameraCtor() { RemoveHook(&g_hookCtor); }
