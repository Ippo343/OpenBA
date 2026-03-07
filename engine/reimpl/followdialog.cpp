// engine/reimpl/followdialog.cpp
// Reimplementation of FollowDialog vtable methods from Application-x64.exe.
//
// FollowDialog (0xD8 bytes), vtable at 0x14015be00.
// Extends DialogInstance (0xB8 base).
//
// Vtable overrides (exe-range functions):
//   +0x00 (Slot0):  0x140011AF0 — Dtor (scalar deleting destructor)
//   +0x20 (Slot4):  0x140069340 — Setup: initializes follow camera UI elements
//   +0x28 (Slot5):  0x140069320 — 19 bytes, clears follow-active flag (REIMPLEMENTED)
//   +0x30 (Slot6):  0x140068F70 — Draw: updates follow camera display/text
//   +0x38 (Slot7):  0x140069250 — Update: checks followed entity state
//   +0x40 (Slot8):  0x140069230 — 18 bytes, OnCommand for buttonClose (REIMPLEMENTED)
//
// Note on Slot5/Slot8 hookability:
//   Slot5: prologue has RIP-relative MOV (bug class 2) — trampoline is broken,
//     but since reimpl is a full replacement (trampoline never called), safe to hook
//     with prologueSize=12 (minimum JMP size).
//   Slot8: prologue has JNZ rel8 at offset 4 (bug class 2) — same rationale.
//
// Unhookable (too small — trampoline needed or insufficient prologue bytes):
//   +0x08 (Slot1):  0x140011A10 — 8 bytes, DtorVec
//   +0x10 (Slot2):  0x140011A00 — 7 bytes, DtorBase
//
// Thunks to Runtime (6-byte jmp, not hooked):
//   +0x18 (Slot3), +0x48 (Slot9)

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
static HookInfo g_hookSlot5;
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot7;
static HookInfo g_hookSlot8;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot0 Dtor (0x140011AF0)
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "FollowDialog::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, FollowDialogAddr::Vtable);
    *(void**)self = s_vtable;
    fn_DialogInstance_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot4 (0x140069340) — Setup: initializes follow camera UI — REIMPLEMENTED
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "FollowDialog::Slot4";
    FollowDialog_Slot4((FollowDialog*)self);
}

// Slot5 (0x140069320) — Shutdown: clears follow-active flag — REIMPLEMENTED
// Note: prologue has RIP-relative MOV (trampoline broken), but trampoline never called.
typedef void (*fn_Slot5_t)(void* self);
static void Reimpl_Slot5(void* self) {
    g_lastHook = "FollowDialog::Slot5";
    FollowDialog_Slot5((FollowDialog*)self);
}

// Slot6 (0x140068F70) — Draw: updates profession and action text — REIMPLEMENTED
typedef void (*fn_Slot6_t)(void* self, void* p2, void* p3, void* p4);
static void Reimpl_Slot6(void* self, void* p2, void* p3, void* p4) {
    g_lastHook = "FollowDialog::Slot6";
    FollowDialog_Slot6((FollowDialog*)self, p2, p3, p4);
}

// Slot7 (0x140069250) — Update: checks followed entity state — REIMPLEMENTED
typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    g_lastHook = "FollowDialog::Slot7";
    FollowDialog_Slot7((FollowDialog*)self);
}

// Slot8 (0x140069230) — OnCommand: handle buttonClose click — REIMPLEMENTED
// Note: prologue has JNZ rel8 at offset 4 (trampoline broken), but trampoline never called.
typedef void (*fn_Slot8_t)(void* self, void* param2, int param3, int param4);
static void Reimpl_Slot8(void* self, void* param2, int param3, int param4) {
    g_lastHook = "FollowDialog::Slot8";
    FollowDialog_Slot8((FollowDialog*)self, param2, param3, param4);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallFollowDialogDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140011AF0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[FollowDialog] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveFollowDialogDtor() { RemoveHook(&g_hookDtor); }

void InstallFollowDialogSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140069340);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 18);
#ifdef _DEBUG
    printf("[FollowDialog] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveFollowDialogSlot4() { RemoveHook(&g_hookSlot4); }

void InstallFollowDialogSlot5() {
    // prologueSize=12 (minimum JMP): prologue has RIP-relative MOV, trampoline broken
    // but never called since this is a full reimpl.
    uintptr_t target = ResolveAddr(Module::Exe, 0x140069320);
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 12);
#ifdef _DEBUG
    printf("[FollowDialog] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveFollowDialogSlot5() { RemoveHook(&g_hookSlot5); }

void InstallFollowDialogSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140068F70);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 14);
#ifdef _DEBUG
    printf("[FollowDialog] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveFollowDialogSlot6() { RemoveHook(&g_hookSlot6); }

void InstallFollowDialogSlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140069250);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 13);
#ifdef _DEBUG
    printf("[FollowDialog] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveFollowDialogSlot7() { RemoveHook(&g_hookSlot7); }

void InstallFollowDialogSlot8() {
    // prologueSize=12 (minimum JMP): prologue has JNZ rel8 at offset 4, trampoline broken
    // but never called since this is a full reimpl.
    uintptr_t target = ResolveAddr(Module::Exe, 0x140069230);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 12);
#ifdef _DEBUG
    printf("[FollowDialog] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveFollowDialogSlot8() { RemoveHook(&g_hookSlot8); }
