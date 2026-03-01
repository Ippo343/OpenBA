// engine/reimpl/mainmenudialog.cpp
// Reimplementation of MainMenuDialog vtable methods from Application-x64.exe.
//
// MainMenuDialog (0xC8 bytes), vtable at 0x14015f550.
// Extends DialogInstance (0xB8 base).
// Has a secondary vtable at Slot10-19 (DialogController-like interface).
//
// Vtable overrides (exe-range functions):
//   +0x00 (Slot0):  0x140020DA0 — Dtor (scalar deleting destructor)
//   +0x20 (Slot4):  0x1400B3CD0 — Setup: initializes main menu buttons/events (445 instr)
//   +0x28 (Slot5):  0x1400B3C60 — Shutdown: unpauses audio scenes
//   +0x40 (Slot8):  0x1400B3B50 — OnCommand: handles Resume/New/Options/Load/etc
//   +0x50 (Slot10): 0x140020CB0 — Secondary dtor (DialogInstance::~DialogInstance)
//
// Unhookable (too small):
//   +0x08 (Slot1):  0x140020D00 — 8 bytes, DtorVec
//   +0x10 (Slot2):  0x140020CF0 — 7 bytes, DtorBase
//
// Thunks to Runtime (6-byte jmp, not hooked):
//   +0x18 (Slot3), +0x30 (Slot6), +0x38 (Slot7), +0x48 (Slot9)
//   Secondary vtable: +0x58-0x98 (Slot11-19) all thunks

#include "ui/Dialogs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSlot4;
static HookInfo g_hookSlot5;
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot10;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot0 Dtor (0x140020DA0)
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

// Slot4 (0x1400B3CD0) — Setup: initializes main menu UI, buttons, events
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

// Slot5 (0x1400B3C60) — Shutdown: unpauses audio scenes (channels 2,3,4)
typedef void (*fn_Slot5_t)(void* self);
static void Reimpl_Slot5(void* self) {
    ((fn_Slot5_t)g_hookSlot5.trampoline)(self);
}

// Slot8 (0x1400B3B50) — OnCommand: dispatches menu button clicks
typedef void (*fn_Slot8_t)(void* self, void* p2, int msg, int id);
static void Reimpl_Slot8(void* self, void* p2, int msg, int id) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self, p2, msg, id);
}

// Slot10 (0x140020CB0) — Secondary dtor
typedef void* (*fn_Slot10_t)(void* self, uint32_t flags);
static void* Reimpl_Slot10(void* self, uint32_t flags) {
    return ((fn_Slot10_t)g_hookSlot10.trampoline)(self, flags);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallMainMenuDialogDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140020DA0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[MainMenuDialog] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveMainMenuDialogDtor() { RemoveHook(&g_hookDtor); }

void InstallMainMenuDialogSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B3CD0);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 12);
#ifdef _DEBUG
    printf("[MainMenuDialog] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveMainMenuDialogSlot4() { RemoveHook(&g_hookSlot4); }

void InstallMainMenuDialogSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B3C60);
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 12);
#ifdef _DEBUG
    printf("[MainMenuDialog] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveMainMenuDialogSlot5() { RemoveHook(&g_hookSlot5); }

void InstallMainMenuDialogSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B3B50);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[MainMenuDialog] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveMainMenuDialogSlot8() { RemoveHook(&g_hookSlot8); }

void InstallMainMenuDialogSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140020CB0);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 12);
#ifdef _DEBUG
    printf("[MainMenuDialog] Slot10 hooked at %p\n", (void*)target);
#endif
}
void RemoveMainMenuDialogSlot10() { RemoveHook(&g_hookSlot10); }
