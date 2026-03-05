// engine/reimpl/eventnotifydialog.cpp
// Reimplementation of EventNotifyDialog vtable methods from Application-x64.exe.
//
// EventNotifyDialog (0xF0 bytes), vtable at 0x14015bca0.
// Extends DialogInstance (0xB8 base).
// Has Ctor at 0x140068b60.
//
// Vtable layout (10 slots):
//   +0x00: 0x140011550 — ScalarDtor (hookable, 14b prologue)
//   +0x08: 0x1400114f0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x1400114e0 — GetTypeId (7b, unhookable)
//   +0x18: 0x14003a948 — Runtime thunk (6b, unhookable)
//   +0x20: 0x1400686d0 — Startup: find UI elements, build notification array (hookable, 14b)
//   +0x28: 0x1400686c0 — Shutdown: RET 0 (3b, unhookable)
//   +0x30: 0x1400680c0 — Update: position ribbon, set sprites/flags per notification (hookable, 13b)
//   +0x38: 0x140067c30 — Load: process events, handle button clicks (hookable, 14b)
//   +0x40: 0x140068680 — OnEvent: branch-based handler (no prologue, unhookable)
//   +0x48: 0x14003a954 — Runtime thunk (6b, unhookable)

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
static HookInfo g_hookStartup;
static HookInfo g_hookUpdate;
static HookInfo g_hookLoad;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot 0: ScalarDtor(this, flags)
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "EventNotifyDialog::ScalarDtor";
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

// Slot 4: Startup(this) — find UI elements, build notification array — REIMPLEMENTED
typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    g_lastHook = "EventNotifyDialog::Startup";
    EventNotifyDialog_Slot4((EventNotifyDialog*)self);
}

// Slot 6: Update(this) — position ribbon, set sprites/flags
typedef void (*fn_Update_t)(void* self);
static void Reimpl_Update(void* self) {
    g_lastHook = "EventNotifyDialog::Update";
    ((fn_Update_t)g_hookUpdate.trampoline)(self);
}

// Slot 7: Load(this, param2) — REIMPLEMENTED (event processing, notification management)
typedef void (*fn_Load_t)(void* self, void* param2);
static void Reimpl_Load(void* self, void* param2) {
    g_lastHook = "EventNotifyDialog::Load";
    EventNotifyDialog_Slot7((EventNotifyDialog*)self, param2);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallEventNotifyDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140011550);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[EventNotifyDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveEventNotifyDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallEventNotifyDialogStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400686d0);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 14);
#ifdef _DEBUG
    printf("[EventNotifyDialog] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveEventNotifyDialogStartup() { RemoveHook(&g_hookStartup); }

void InstallEventNotifyDialogUpdate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400680c0);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);
#ifdef _DEBUG
    printf("[EventNotifyDialog] Update hooked at %p\n", (void*)target);
#endif
}
void RemoveEventNotifyDialogUpdate() { RemoveHook(&g_hookUpdate); }

void InstallEventNotifyDialogLoad() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140067c30);
    InstallHook(target, (void*)Reimpl_Load, &g_hookLoad, 14);
#ifdef _DEBUG
    printf("[EventNotifyDialog] Load hooked at %p\n", (void*)target);
#endif
}
void RemoveEventNotifyDialogLoad() { RemoveHook(&g_hookLoad); }
