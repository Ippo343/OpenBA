// engine/reimpl/goaldialog.cpp
// Reimplementation of GoalDialog vtable methods from Application-x64.exe.
//
// GoalDialog (0xC8 bytes), vtable at 0x14015beb0.
// Extends DialogInstance (0xB8 base).
//
// Vtable overrides (exe-range functions):
//   +0x00 (Slot0):  0x140011C10 — Dtor (scalar deleting destructor)
//   +0x20 (Slot4):  0x140069810 — Setup: populates goal table UI (361 instr)
//   +0x30 (Slot6):  0x140069640 — Draw: updates goal progress display (71 instr)
//   +0x40 (Slot8):  0x1400697C0 — OnCommand: msg=0x12, id=100 -> close dialog
//
// Unhookable (too small):
//   +0x08 (Slot1):  0x140011B50 — 8 bytes, DtorVec
//   +0x10 (Slot2):  0x140011B40 — 7 bytes, DtorBase
//   +0x38 (Slot7):  0x140069800 — 3 bytes, ret 0 (no-op handler)
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

// Slot0 Dtor (0x140011C10)
// Cleanup: free dynamic array at +0xB8, zero +0xB8/+0xC0
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "GoalDialog::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, GoalDialogAddr::Vtable);
    *(void**)self = s_vtable;
    uintptr_t s = (uintptr_t)self;
    fn_MemFreeStatic(*(void**)(s + 0xB8));
    *(uint64_t*)(s + 0xB8) = 0;
    *(uint64_t*)(s + 0xC0) = 0;
    fn_DialogInstance_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot4 (0x140069810) — Setup: populates goal table with achievements/goals — REIMPLEMENTED
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "GoalDialog::Slot4";
    GoalDialog_Slot4((GoalDialog*)self);
}

// Slot6 (0x140069640) — Draw: updates goal progress indicators — REIMPLEMENTED
typedef void (*fn_Slot6_t)(void* self);
static void Reimpl_Slot6(void* self) {
    g_lastHook = "GoalDialog::Slot6";
    GoalDialog_Slot6((GoalDialog*)self);
}

// Slot8 (0x1400697C0) — OnCommand: msg=0x12, id=100 closes dialog
typedef void (*fn_Slot8_t)(void* self, void* p2, int msg, int id);
static void Reimpl_Slot8(void* self, void* p2, int msg, int id) {
    g_lastHook = "GoalDialog::Slot8";
    if (msg == 0x12 && id == 100) {
        uintptr_t s = (uintptr_t)self;
        uint8_t basePtr[16];
        fn_InternalBasePtr_Ctor(basePtr, self);
        fn_UI_CloseDialog(*(void**)(s + 0x10), basePtr);
    }
}

// ============================================================
// Install / Remove
// ============================================================

void InstallGoalDialogDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140011C10);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[GoalDialog] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveGoalDialogDtor() { RemoveHook(&g_hookDtor); }

void InstallGoalDialogSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140069810);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 17);
#ifdef _DEBUG
    printf("[GoalDialog] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveGoalDialogSlot4() { RemoveHook(&g_hookSlot4); }

void InstallGoalDialogSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140069640);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 17);
#ifdef _DEBUG
    printf("[GoalDialog] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveGoalDialogSlot6() { RemoveHook(&g_hookSlot6); }

void InstallGoalDialogSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400697C0);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 14);
#ifdef _DEBUG
    printf("[GoalDialog] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveGoalDialogSlot8() { RemoveHook(&g_hookSlot8); }
