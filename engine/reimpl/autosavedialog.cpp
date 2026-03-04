// engine/reimpl/autosavedialog.cpp
// Reimplementation of AutoSaveDialog vtable methods from Application-x64.exe.
//
// AutoSaveDialog (0xD0 bytes), vtable at 0x14015b120.
// Extends DialogInstance (0xB8 base).
//
// Vtable overrides (exe-range functions):
//   +0x00 (Slot0):  0x140010780 — Dtor (scalar deleting destructor)
//   +0x20 (Slot4):  0x14005E110 — 5 bytes, JMP thunk (unhookable)
//   +0x30 (Slot6):  0x14005DFC0 — Draw: checks SaveLoad operation, toggles UI elements
//   +0x38 (Slot7):  0x14005E090 — Update: accumulates time-based rotation
//   +0x40 (Slot8):  0x14005E0F0 — 12 bytes, CMP+JZ+RET (unhookable)
//
// Unhookable (too small):
//   +0x08 (Slot1):  0x1400106B0 — 8 bytes, DtorVec
//   +0x10 (Slot2):  0x1400106A0 — 7 bytes, DtorBase
//   +0x20 (Slot4):  0x14005E110 — 5 bytes, JMP rel8 thunk
//   +0x40 (Slot8):  0x14005E0F0 — 12 bytes, CMP R8D,0x13; JZ; REP RET
//
// Thunks to Runtime (6-byte jmp, not hooked):
//   +0x18 (Slot3), +0x28 (Slot5), +0x48 (Slot9)

#include "ui/Dialogs.h"
#include "Dialog/ui_dialogs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot7;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot0 Dtor (0x140010780) — scalar deleting destructor
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "AutoSaveDialog::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, AutoSaveDialogAddr::Vtable);
    *(void**)self = s_vtable;
    fn_DialogInstance_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot6 (0x14005DFC0) — Draw: checks SaveLoad::GetOperation, toggles save/load UI — REIMPLEMENTED
typedef void (*fn_Slot6_t)(void* self);
static void Reimpl_Slot6(void* self) {
    g_lastHook = "AutoSaveDialog::Slot6";
    AutoSaveDialog_Slot6((AutoSaveDialog*)self);
}

// Slot7 (0x14005E090) — Update: time-based rotation accumulator — REIMPLEMENTED
typedef void (*fn_Slot7_t)(void* self, void* time);
static void Reimpl_Slot7(void* self, void* time) {
    g_lastHook = "AutoSaveDialog::Slot7";
    AutoSaveDialog_Slot7((AutoSaveDialog*)self, time);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallAutoSaveDialogDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140010780);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[AutoSaveDialog] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveAutoSaveDialogDtor() { RemoveHook(&g_hookDtor); }

void InstallAutoSaveDialogSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005DFC0);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 13);
#ifdef _DEBUG
    printf("[AutoSaveDialog] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveAutoSaveDialogSlot6() { RemoveHook(&g_hookSlot6); }

void InstallAutoSaveDialogSlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005E090);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 12);
#ifdef _DEBUG
    printf("[AutoSaveDialog] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveAutoSaveDialogSlot7() { RemoveHook(&g_hookSlot7); }
