// engine/reimpl/scenariodialog.cpp
// Reimplementation of ScenarioDialog vtable methods from Application-x64.exe.
//
// ScenarioDialog (0xE8 bytes), vtable at 0x14015d2d0.
// Extends DialogInstance (0xB8 base).
//
// Vtable overrides (exe-range functions):
//   +0x00 (Slot0):  0x140013AF0 — Dtor (scalar deleting destructor)
//   +0x20 (Slot4):  0x140078010 — Setup: populates scenario list table (348 instr)
//   +0x30 (Slot6):  0x140077BE0 — Draw: updates scenario entry visibility/state
//   +0x38 (Slot7):  0x140078000 — 3 bytes, ret 0 (no-op, unhookable)
//   +0x40 (Slot8):  0x140077F80 — OnCommand: scenario select/confirm/close handler
//
// Unhookable (too small):
//   +0x08 (Slot1):  0x140013960 — 8 bytes, DtorVec
//   +0x10 (Slot2):  0x140013950 — 7 bytes, DtorBase
//   +0x38 (Slot7):  0x140078000 — 3 bytes, ret 0
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
static HookInfo g_hookSlot4;
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot8;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot0 Dtor (0x140013AF0)
// Cleanup: free dynamic array at +0xD8, zero +0xD8/+0xE0
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "ScenarioDialog::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ScenarioDialogAddr::Vtable);
    *(void**)self = s_vtable;
    uintptr_t s = (uintptr_t)self;
    fn_MemFreeStatic(*(void**)(s + 0xD8));
    *(uint64_t*)(s + 0xD8) = 0;
    *(uint64_t*)(s + 0xE0) = 0;
    fn_DialogInstance_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot4 (0x140078010) — Setup: populates scenario list — REIMPLEMENTED
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "ScenarioDialog::Slot4";
    ScenarioDialog_Slot4((ScenarioDialog*)self);
}

// Slot6 (0x140077BE0) — REIMPLEMENTED (updates entry visibility/selection)
typedef void (*fn_Slot6_t)(void* self);
static void Reimpl_Slot6(void* self) {
    g_lastHook = "ScenarioDialog::Slot6";
    ScenarioDialog_Slot6((ScenarioDialog*)self);
}

// Slot8 (0x140077F80) — OnCommand: selection/confirm/close — REIMPLEMENTED
typedef void (*fn_Slot8_t)(void* self, void* p2, int msg, int id);
static void Reimpl_Slot8(void* self, void* p2, int msg, int id) {
    g_lastHook = "ScenarioDialog::Slot8";
    ScenarioDialog_Slot8((ScenarioDialog*)self, (uint64_t)p2, msg, id);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallScenarioDialogDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140013AF0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[ScenarioDialog] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveScenarioDialogDtor() { RemoveHook(&g_hookDtor); }

void InstallScenarioDialogSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140078010);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[ScenarioDialog] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveScenarioDialogSlot4() { RemoveHook(&g_hookSlot4); }

void InstallScenarioDialogSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140077BE0);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 12);
#ifdef _DEBUG
    printf("[ScenarioDialog] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveScenarioDialogSlot6() { RemoveHook(&g_hookSlot6); }

void InstallScenarioDialogSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140077F80);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[ScenarioDialog] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveScenarioDialogSlot8() { RemoveHook(&g_hookSlot8); }
