// engine/reimpl/saveloaddialog.cpp
// Reimplementation of SaveLoadDialog vtable methods from Application-x64.exe.
//
// SaveLoadDialog (0x160 bytes), vtable at 0x14015f900.
// Extends DialogInstance (0xB8 base).
// Contains UI::Sprite sub-object at +0xD0.
// Has Ctor at 0x1400b81c0.
//
// Vtable layout (10 slots):
//   +0x00: 0x1400211b0 — ScalarDtor (hookable, 15b prologue)
//   +0x08: 0x140021150 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140021140 — GetTypeId (7b, unhookable)
//   +0x18: 0x14003a948 — Runtime thunk (6b, unhookable)
//   +0x20: 0x1400b7d00 — Startup: build save/load UI, populate file list (hookable, 14b)
//   +0x28: 0x14003a94e — Runtime thunk (6b, unhookable)
//   +0x30: 0x1400b6880 — Update: refresh file list, update preview (hookable, 15b)
//   +0x38: 0x1400b79b0 — Load: load save file metadata, timestamps (hookable, 14b)
//   +0x40: 0x1400b67e0 — OnEvent: branch-based handler (rel branch in prologue, unhookable)
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
static HookInfo g_hookLoad;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot 0: ScalarDtor(this, flags)
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

// Slot 4: Startup(this) — build save/load UI, populate file list
typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    ((fn_Startup_t)g_hookStartup.trampoline)(self);
}

// Slot 6: Update(this) — refresh file list, update preview
typedef void (*fn_Update_t)(void* self);
static void Reimpl_Update(void* self) {
    ((fn_Update_t)g_hookUpdate.trampoline)(self);
}

// Slot 7: Load(this, time, loaderLoop, param4)
typedef void (*fn_Load_t)(void* self, void* time, void** loaderLoop, uint64_t param4);
static void Reimpl_Load(void* self, void* time, void** loaderLoop, uint64_t param4) {
    ((fn_Load_t)g_hookLoad.trampoline)(self, time, loaderLoop, param4);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallSaveLoadDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400211b0);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 15);
#ifdef _DEBUG
    printf("[SaveLoadDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveSaveLoadDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallSaveLoadDialogStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400b7d00);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 15);
#ifdef _DEBUG
    printf("[SaveLoadDialog] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveSaveLoadDialogStartup() { RemoveHook(&g_hookStartup); }

void InstallSaveLoadDialogUpdate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400b6880);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 15);
#ifdef _DEBUG
    printf("[SaveLoadDialog] Update hooked at %p\n", (void*)target);
#endif
}
void RemoveSaveLoadDialogUpdate() { RemoveHook(&g_hookUpdate); }

void InstallSaveLoadDialogLoad() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400b79b0);
    InstallHook(target, (void*)Reimpl_Load, &g_hookLoad, 14);
#ifdef _DEBUG
    printf("[SaveLoadDialog] Load hooked at %p\n", (void*)target);
#endif
}
void RemoveSaveLoadDialogLoad() { RemoveHook(&g_hookLoad); }
