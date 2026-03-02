// engine/reimpl/followdialog.cpp
// Reimplementation of FollowDialog vtable methods from Application-x64.exe.
//
// FollowDialog (0xD8 bytes), vtable at 0x14015be00.
// Extends DialogInstance (0xB8 base).
//
// Vtable overrides (exe-range functions):
//   +0x00 (Slot0):  0x140011AF0 — Dtor (scalar deleting destructor)
//   +0x20 (Slot4):  0x140069340 — Setup: initializes follow camera UI elements
//   +0x28 (Slot5):  0x140069320 — 19 bytes, sets flag on followed entity (unhookable pattern)
//   +0x30 (Slot6):  0x140068F70 — Draw: updates follow camera display/text
//   +0x38 (Slot7):  0x140069250 — Update: checks followed entity state
//   +0x40 (Slot8):  0x140069230 — 18 bytes, CMP+JNZ+CMP+JZ+RET (unhookable)
//
// Unhookable (too small):
//   +0x08 (Slot1):  0x140011A10 — 8 bytes, DtorVec
//   +0x10 (Slot2):  0x140011A00 — 7 bytes, DtorBase
//   +0x28 (Slot5):  0x140069320 — 19 bytes but sets flag and returns (no clean hook point)
//   +0x40 (Slot8):  0x140069230 — 18 bytes, CMP/JNZ/CMP/JZ/RET pattern
//
// Thunks to Runtime (6-byte jmp, not hooked):
//   +0x18 (Slot3), +0x48 (Slot9)

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
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot7;

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

// Slot4 (0x140069340) — Setup: initializes follow camera UI
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "FollowDialog::Slot4";
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

// Slot6 (0x140068F70) — Draw: updates follow camera display
typedef void (*fn_Slot6_t)(void* self, void* p2, void* p3, void* p4);
static void Reimpl_Slot6(void* self, void* p2, void* p3, void* p4) {
    g_lastHook = "FollowDialog::Slot6";
    ((fn_Slot6_t)g_hookSlot6.trampoline)(self, p2, p3, p4);
}

// Slot7 (0x140069250) — Update: checks followed entity state
typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    g_lastHook = "FollowDialog::Slot7";
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self);
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
