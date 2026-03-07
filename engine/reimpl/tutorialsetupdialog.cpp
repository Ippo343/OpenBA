// engine/reimpl/tutorialsetupdialog.cpp
// Reimplementation of TutorialSetupDialog vtable methods from Application-x64.exe.
//
// TutorialSetupDialog (0x78 bytes), vtable at 0x140160d20.
// Extends Object::External (NOT DialogInstance).
// Object::External base ~0x20 bytes, subclass fields start at +0x20.
// Has Ctor at 0x1400e2cc0, intermediate vtable at 0x140160c30.
//
// Vtable layout (12 slots — Object::External base + extensions):
//   +0x00: 0x140028620 — ScalarDtor (hookable, 15b prologue)
//   +0x08: 0x1400285a0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140028590 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400e2be0 — Slot3: MOV EAX,6; RET (6b, unhookable)
//   +0x20: 0x14003a912 — Runtime thunk (6b, unhookable)
//   +0x28: 0x1400e2a60 — Shutdown: serialize/cleanup tutorial setup (hookable, 17b)
//   +0x30: 0x140028600 — OnEvent: delegate to vtable[0], then cleanup (hookable, 14b)
//   +0x38: 0x140001640 — Delegator: MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX] (8b, unhookable)
//   +0x40: 0x1400e25f0 — Slot8: rel CALL in prologue, hookable as full reimpl (15b)
//   +0x48: 0x1400e24c0 — Slot9: rel CALL in prologue, hookable as full reimpl (14b)
//   +0x50: 0x1400e2460 — Slot10: rel CALL in prologue, hookable as full reimpl (16b)
//   +0x58: 0x1400285f0 — Slot11: TEST+branch (12b, unhookable)

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
static HookInfo g_hookScalarDtor;
static HookInfo g_hookShutdown;
static HookInfo g_hookOnEvent;
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;
static HookInfo g_hookSlot10;

// ============================================================
// Reimplementation wrappers (delegate to code/Game/Dialog/tutorialsetupdialog.cpp)
// ============================================================

// Slot 0: ScalarDtor(this, flags) — REIMPLEMENTED
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialSetupDialog::ScalarDtor";
    return TutorialSetupDialog_ScalarDtor(self, flags);
}

// Slot 5: Shutdown(this, stream) — REIMPLEMENTED
static uint64_t Reimpl_Shutdown(void* self, void* stream) {
    g_lastHook = "TutorialSetupDialog::Shutdown";
    return TutorialSetupDialog_Shutdown(self, stream);
}

// Slot 6: OnEvent(this) — REIMPLEMENTED
static void Reimpl_OnEvent(void* self) {
    g_lastHook = "TutorialSetupDialog::OnEvent";
    TutorialSetupDialog_OnEvent(self);
}

// Slot8 (vtable +0x40): entity selection handler — REIMPLEMENTED
static void Reimpl_Slot8(void* self) {
    g_lastHook = "TutorialSetupDialog::Slot8";
    TutorialSetupDialog_Slot8(self);
}

// Slot9 (vtable +0x48): activate tutorial setup — REIMPLEMENTED
static void Reimpl_Slot9(void* self) {
    g_lastHook = "TutorialSetupDialog::Slot9";
    TutorialSetupDialog_Slot9(self);
}

// Slot10 (vtable +0x50): close current setup dialog — REIMPLEMENTED
static void Reimpl_Slot10(void* self) {
    g_lastHook = "TutorialSetupDialog::Slot10";
    TutorialSetupDialog_Slot10(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallTutorialSetupDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028620);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 15);
#ifdef _DEBUG
    printf("[TutorialSetupDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallTutorialSetupDialogShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e2a60);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 17);
#ifdef _DEBUG
    printf("[TutorialSetupDialog] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupDialogShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTutorialSetupDialogOnEvent() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028600);
    InstallHook(target, (void*)Reimpl_OnEvent, &g_hookOnEvent, 14);
#ifdef _DEBUG
    printf("[TutorialSetupDialog] OnEvent hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupDialogOnEvent() { RemoveHook(&g_hookOnEvent); }

// Slot8 (vtable +0x40, exe 0x1400e25f0): rel CALL at byte 10, land at byte 15
// Prologue: PUSH RBX(1) PUSH RSI(1) SUB RSP,0x28(4) MOV RSI,RCX(3) [CALL+5] = 15 bytes
void InstallTutorialSetupDialogSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e25f0);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[TutorialSetupDialog] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupDialogSlot8() { RemoveHook(&g_hookSlot8); }

// Slot9 (vtable +0x48, exe 0x1400e24c0): rel CALL at byte 9, land at byte 14
// Prologue: PUSH RBX(1) SUB RSP,0x20(4) MOV RBX,RCX(3) [CALL+5] = 14 bytes
void InstallTutorialSetupDialogSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e24c0);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 14);
#ifdef _DEBUG
    printf("[TutorialSetupDialog] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupDialogSlot9() { RemoveHook(&g_hookSlot9); }

// Slot10 (vtable +0x50, exe 0x1400e2460): rel CALL at byte 11, land at byte 16
// Prologue: PUSH RBX(1) SUB RSP,0x20(4) MOV DL,0x1(2) MOV RBX,RCX(3) [CALL+5] = 16 bytes
void InstallTutorialSetupDialogSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e2460);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 16);
#ifdef _DEBUG
    printf("[TutorialSetupDialog] Slot10 hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupDialogSlot10() { RemoveHook(&g_hookSlot10); }
