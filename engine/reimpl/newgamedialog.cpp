// engine/reimpl/newgamedialog.cpp
// Reimplementation of NewGameDialog vtable methods from Application-x64.exe.
//
// NewGameDialog (0xAE8 bytes), vtable at 0x14015c920.
// Extends DialogInstance (0xB8 base). Largest dialog class.
// Contains System::Random sub-object at +0x118.
// Has Ctor at 0x140071120.
//
// Vtable layout (10 slots):
//   +0x00: 0x140012cc0 — ScalarDtor (hookable, 14b prologue)
//   +0x08: 0x140012c60 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140012c50 — GetTypeId (7b, unhookable)
//   +0x18: 0x14003a948 — Runtime thunk (6b, unhookable)
//   +0x20: 0x1400706d0 — Startup: build new game UI, seed/terrain/size combos (hookable, 14b)
//   +0x28: 0x1400706c0 — Shutdown: RET 0 (3b, unhookable)
//   +0x30: 0x140070650 — Update: sync new game settings with UI (hookable, 15b)
//   +0x38: 0x14003a990 — Runtime thunk (6b, unhookable)
//   +0x40: 0x140070110 — OnEvent: branch-based handler (rel branch in prologue, unhookable)
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
    g_lastHook = "NewGameDialog::ScalarDtor";
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

// Slot 4: Startup(this) — build new game UI with combos, seed, terrain
typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    g_lastHook = "NewGameDialog::Startup";
    ((fn_Startup_t)g_hookStartup.trampoline)(self);
}

// Slot 6: Update(this) — sync new game settings
typedef void (*fn_Update_t)(void* self);
static void Reimpl_Update(void* self) {
    g_lastHook = "NewGameDialog::Update";
    ((fn_Update_t)g_hookUpdate.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallNewGameDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012cc0);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[NewGameDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveNewGameDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallNewGameDialogStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400706d0);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 16);
#ifdef _DEBUG
    printf("[NewGameDialog] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveNewGameDialogStartup() { RemoveHook(&g_hookStartup); }

void InstallNewGameDialogUpdate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140070650);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 15);
#ifdef _DEBUG
    printf("[NewGameDialog] Update hooked at %p\n", (void*)target);
#endif
}
void RemoveNewGameDialogUpdate() { RemoveHook(&g_hookUpdate); }
