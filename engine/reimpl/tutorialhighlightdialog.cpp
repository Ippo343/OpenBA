// engine/reimpl/tutorialhighlightdialog.cpp
// Reimplementation of TutorialHighlightDialog vtable methods from Application-x64.exe.
//
// TutorialHighlightDialog (0xF0 bytes), vtable at 0x1401609e0.
// Extends DialogInstance (0xB8 base).
// Has Ctor at 0x1400e0290.
//
// Vtable layout (10 slots):
//   +0x00: 0x140027910 — ScalarDtor (hookable, 14b prologue)
//   +0x08: 0x1400278b0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x1400278a0 — GetTypeId (7b, unhookable)
//   +0x18: 0x14003a948 — Runtime thunk (6b, unhookable)
//   +0x20: 0x1400e0020 — Startup: set up highlight overlay (hookable, 13b)
//   +0x28: 0x14003a94e — Runtime thunk (6b, unhookable)
//   +0x30: 0x1400dffe0 — Update: branch-based check (rel branch at byte 8, unhookable)
//   +0x38: 0x1400e0000 — Load: MOV+ADD+RET (10b, unhookable)
//   +0x40: 0x14003aa1a — Runtime thunk (6b, unhookable)
//   +0x48: 0x14003a954 — Runtime thunk (6b, unhookable)

#include "ui/Dialogs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookScalarDtor;
static HookInfo g_hookStartup;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot 0: ScalarDtor(this, flags)
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialHighlightDialog::ScalarDtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, TutorialHighlightDialogAddr::Vtable);
    *(void**)self = s_vtable;
    fn_DialogInstance_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot 4: Startup(this) — set up tutorial highlight overlay
typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    g_lastHook = "TutorialHighlightDialog::Startup";
    ((fn_Startup_t)g_hookStartup.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallTutorialHighlightDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027910);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[TutorialHighlightDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialHighlightDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallTutorialHighlightDialogStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e0020);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 17);
#ifdef _DEBUG
    printf("[TutorialHighlightDialog] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialHighlightDialogStartup() { RemoveHook(&g_hookStartup); }
