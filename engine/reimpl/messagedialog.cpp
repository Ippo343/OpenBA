// engine/reimpl/messagedialog.cpp
// Reimplementation of MessageDialog vtable methods from Application-x64.exe.
//
// MessageDialog (0xC0 bytes), vtable at 0x14015c480.
// Extends DialogInstance (0xB8 base).
// Has extra slots 10-13 for button result handlers (Yes/No/Ok/Cancel).
//
// Vtable overrides (exe-range functions):
//   +0x00 (Slot0):  0x140012660 — Dtor (scalar deleting destructor)
//   +0x20 (Slot4):  0x14006D140 — Setup: finds buttonCancel/buttonOk/etc elements (93 instr)
//   +0x28 (Slot5):  0x14006D0C0 — 3 bytes, ret 0 (no-op, unhookable)
//   +0x40 (Slot8):  0x14006D070 — OnCommand: dispatches button presses (not a function in Ghidra, unhookable)
//   +0x50 (Slot10): 0x14006CFF0 — ButtonOk: sets result=1, closes dialog
//   +0x58 (Slot11): 0x14006D030 — ButtonCancel: sets result=0, closes dialog
//   +0x60 (Slot12): 0x14006CFB0 — ButtonYes: sets result=2, closes dialog
//   +0x68 (Slot13): 0x14006CF70 — ButtonNo: sets result=3, closes dialog
//
// Unhookable (too small):
//   +0x08 (Slot1):  0x1400125C0 — 8 bytes, DtorVec
//   +0x10 (Slot2):  0x1400125B0 — 7 bytes, DtorBase
//   +0x28 (Slot5):  0x14006D0C0 — 3 bytes, ret 0
//   +0x40 (Slot8):  0x14006D070 — inline dispatch, not recognized as function
//
// Thunks to Runtime (6-byte jmp, not hooked):
//   +0x18 (Slot3), +0x30 (Slot6), +0x38 (Slot7), +0x48 (Slot9)

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
static HookInfo g_hookSlot10;
static HookInfo g_hookSlot11;
static HookInfo g_hookSlot12;
static HookInfo g_hookSlot13;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot0 Dtor (0x140012660)
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

// Slot4 (0x14006D140) — Setup: finds button elements by name
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

// Slot10 (0x14006CFF0) — ButtonOk: result=1, close
typedef void (*fn_ButtonResult_t)(void* self);
static void Reimpl_Slot10(void* self) {
    ((fn_ButtonResult_t)g_hookSlot10.trampoline)(self);
}

// Slot11 (0x14006D030) — ButtonCancel: result=0, close
static void Reimpl_Slot11(void* self) {
    ((fn_ButtonResult_t)g_hookSlot11.trampoline)(self);
}

// Slot12 (0x14006CFB0) — ButtonYes: result=2, close
static void Reimpl_Slot12(void* self) {
    ((fn_ButtonResult_t)g_hookSlot12.trampoline)(self);
}

// Slot13 (0x14006CF70) — ButtonNo: result=3, close
static void Reimpl_Slot13(void* self) {
    ((fn_ButtonResult_t)g_hookSlot13.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallMessageDialogDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012660);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[MessageDialog] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveMessageDialogDtor() { RemoveHook(&g_hookDtor); }

void InstallMessageDialogSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006D140);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 19);
#ifdef _DEBUG
    printf("[MessageDialog] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveMessageDialogSlot4() { RemoveHook(&g_hookSlot4); }

void InstallMessageDialogSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006CFF0);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 16);
#ifdef _DEBUG
    printf("[MessageDialog] Slot10 (ButtonOk) hooked at %p\n", (void*)target);
#endif
}
void RemoveMessageDialogSlot10() { RemoveHook(&g_hookSlot10); }

void InstallMessageDialogSlot11() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006D030);
    InstallHook(target, (void*)Reimpl_Slot11, &g_hookSlot11, 16);
#ifdef _DEBUG
    printf("[MessageDialog] Slot11 (ButtonCancel) hooked at %p\n", (void*)target);
#endif
}
void RemoveMessageDialogSlot11() { RemoveHook(&g_hookSlot11); }

void InstallMessageDialogSlot12() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006CFB0);
    InstallHook(target, (void*)Reimpl_Slot12, &g_hookSlot12, 16);
#ifdef _DEBUG
    printf("[MessageDialog] Slot12 (ButtonYes) hooked at %p\n", (void*)target);
#endif
}
void RemoveMessageDialogSlot12() { RemoveHook(&g_hookSlot12); }

void InstallMessageDialogSlot13() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006CF70);
    InstallHook(target, (void*)Reimpl_Slot13, &g_hookSlot13, 16);
#ifdef _DEBUG
    printf("[MessageDialog] Slot13 (ButtonNo) hooked at %p\n", (void*)target);
#endif
}
void RemoveMessageDialogSlot13() { RemoveHook(&g_hookSlot13); }
