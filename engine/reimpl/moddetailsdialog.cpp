// engine/reimpl/moddetailsdialog.cpp
// Reimplementation of ModDetailsDialog vtable methods from Application-x64.exe.
//
// ModDetailsDialog (0xC8 bytes), vtable at 0x14015c670.
// Extends DialogInstance (0xB8 base).
// Has a secondary vtable at Slot10-19 (DialogController-like interface).
//
// Vtable overrides (exe-range functions):
//   +0x00 (Slot0):  0x140012890 — Dtor (scalar deleting destructor)
//   +0x20 (Slot4):  0x14006D700 — Setup: populates mod details UI (720 instr, huge)
//   +0x40 (Slot8):  0x14006D6A0 — OnCommand: close/upload/delete handlers
//
// Unhookable (too small):
//   +0x08 (Slot1):  0x140012780 — 8 bytes, DtorVec
//   +0x10 (Slot2):  0x140012770 — 7 bytes, DtorBase
//   +0x50 (Slot10): 0x140012840 — shared dtor (used by ModUploadDialog too, hooked there if needed)
//   +0x58 (Slot11): 0x140012830 — 8 bytes, getter
//   +0x60 (Slot12): 0x140012820 — 7 bytes, getter
//
// Thunks to Runtime (6-byte jmp, not hooked):
//   +0x18 (Slot3), +0x28 (Slot5), +0x30 (Slot6), +0x38 (Slot7), +0x48 (Slot9)
//   Secondary vtable: +0x68-0x98 (Slot13-19) all thunks

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
static HookInfo g_hookSlot8;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot0 Dtor (0x140012890)
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "ModDetailsDialog::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ModDetailsDialogAddr::Vtable);
    *(void**)self = s_vtable;
    fn_DialogInstance_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot4 (0x14006D700) — Setup: populates mod name, author, description, image — REIMPLEMENTED
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "ModDetailsDialog::Slot4";
    ModDetailsDialog_Slot4((ModDetailsDialog*)self);
}

// Slot8 (0x14006D6A0) — OnCommand: close/upload/delete — REIMPLEMENTED
typedef void (*fn_Slot8_t)(void* self, void* p2, int msg, int id);
static void Reimpl_Slot8(void* self, void* p2, int msg, int id) {
    g_lastHook = "ModDetailsDialog::Slot8";
    ModDetailsDialog_Slot8((ModDetailsDialog*)self, (uint64_t)p2, msg, id);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallModDetailsDialogDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012890);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[ModDetailsDialog] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveModDetailsDialogDtor() { RemoveHook(&g_hookDtor); }

void InstallModDetailsDialogSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006D700);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[ModDetailsDialog] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveModDetailsDialogSlot4() { RemoveHook(&g_hookSlot4); }

void InstallModDetailsDialogSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006D6A0);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 14);
#ifdef _DEBUG
    printf("[ModDetailsDialog] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveModDetailsDialogSlot8() { RemoveHook(&g_hookSlot8); }
