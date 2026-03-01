// engine/reimpl/mapparamsdialog.cpp
// Reimplementation of MapParamsDialog vtable methods from Application-x64.exe.
//
// MapParamsDialog (0xF8 bytes), vtable at 0x14015c180.
// Extends DialogInstance (0xB8 base).
// Has Ctor at 0x14006be20.
//
// Vtable layout (10 slots):
//   +0x00: 0x140012150 — ScalarDtor (hookable, 14b prologue)
//   +0x08: 0x1400120f0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x1400120e0 — GetTypeId (7b, unhookable)
//   +0x18: 0x14003a948 — Runtime thunk (6b, unhookable)
//   +0x20: 0x14006b810 — Startup: find UI elements, set up map params UI (hookable, 14b)
//   +0x28: 0x14006b800 — Shutdown: RET 0 (3b, unhookable)
//   +0x30: 0x14006b790 — Update: sync map parameters with UI state (hookable, 15b)
//   +0x38: 0x14003a990 — Runtime thunk (6b, unhookable)
//   +0x40: 0x14006b5f0 — OnEvent: branch-based handler (rel branch at byte 4, unhookable)
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
static HookInfo g_hookUpdate;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot 0: ScalarDtor(this, flags)
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

// Slot 4: Startup(this) — find UI elements, populate map params
typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    ((fn_Startup_t)g_hookStartup.trampoline)(self);
}

// Slot 6: Update(this) — sync map parameters with UI
typedef void (*fn_Update_t)(void* self);
static void Reimpl_Update(void* self) {
    ((fn_Update_t)g_hookUpdate.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallMapParamsDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012150);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 14);
#ifdef _DEBUG
    printf("[MapParamsDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveMapParamsDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallMapParamsDialogStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006b810);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 14);
#ifdef _DEBUG
    printf("[MapParamsDialog] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveMapParamsDialogStartup() { RemoveHook(&g_hookStartup); }

void InstallMapParamsDialogUpdate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006b790);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 15);
#ifdef _DEBUG
    printf("[MapParamsDialog] Update hooked at %p\n", (void*)target);
#endif
}
void RemoveMapParamsDialogUpdate() { RemoveHook(&g_hookUpdate); }
