// engine/reimpl/moduploaddialog.cpp
// Reimplementation of ModUploadDialog vtable methods from Application-x64.exe.
//
// ModUploadDialog (0xB8 bytes), vtable at 0x14015c6c0.
// Extends DialogInstance (0xB8 base). No extra fields.
//
// This is essentially a bare DialogInstance with no overrides beyond the
// standard dtor pattern. All vtable slots are either tiny accessors or
// Runtime thunks.
//
// Vtable overrides (exe-range functions):
//   +0x00 (Slot0):  0x140012840 — Dtor (scalar deleting destructor, 19+ bytes, hookable)
//
// Unhookable (too small):
//   +0x08 (Slot1):  0x140012830 — 8 bytes, DtorVec
//   +0x10 (Slot2):  0x140012820 — 7 bytes, DtorBase
//
// Thunks to Runtime (6-byte jmp, not hooked):
//   +0x18 (Slot3), +0x20 (Slot4), +0x28 (Slot5), +0x30 (Slot6),
//   +0x38 (Slot7), +0x40 (Slot8), +0x48 (Slot9)

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

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot0 Dtor (0x140012840) — scalar deleting destructor
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "ModUploadDialog::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallModUploadDialogDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012840);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[ModUploadDialog] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveModUploadDialogDtor() { RemoveHook(&g_hookDtor); }
