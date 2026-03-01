// engine/reimpl/optionsdialog.cpp
// Reimplementation of OptionsDialog vtable methods from Application-x64.exe.
//
// OptionsDialog (0x1C0 bytes), vtable at 0x14015ceb0.
// Extends DialogInstance (0xB8 base).
// Has Ctor at 0x140075280.
//
// Vtable layout (10 slots):
//   +0x00: 0x140012f30 — ScalarDtor (hookable, 14b prologue)
//   +0x08: 0x140012de0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140012dd0 — GetTypeId (7b, unhookable)
//   +0x18: 0x14003a948 — Runtime thunk (6b, unhookable)
//   +0x20: 0x1400742d0 — Startup: build options UI with tabs/controls (hookable, 14b)
//   +0x28: 0x140073480 — Shutdown: RET 0 (3b, unhookable)
//   +0x30: 0x14003a98a — Runtime thunk (6b, unhookable)
//   +0x38: 0x14003a990 — Runtime thunk (6b, unhookable)
//   +0x40: 0x140072f20 — OnEvent: handle options changes (hookable, 15b)
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
static HookInfo g_hookOnEvent;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot 0: ScalarDtor(this, flags)
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "OptionsDialog::ScalarDtor";
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

// Slot 4: Startup(this) — build options UI with tabs/controls
typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    g_lastHook = "OptionsDialog::Startup";
    ((fn_Startup_t)g_hookStartup.trampoline)(self);
}

// Slot 8: OnEvent(this, element, eventType, eventData)
typedef void (*fn_OnEvent_t)(void* self, void* element, int eventType, int eventData);
static void Reimpl_OnEvent(void* self, void* element, int eventType, int eventData) {
    g_lastHook = "OptionsDialog::OnEvent";
    ((fn_OnEvent_t)g_hookOnEvent.trampoline)(self, element, eventType, eventData);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallOptionsDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012f30);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[OptionsDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveOptionsDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallOptionsDialogStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400742d0);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 15);
#ifdef _DEBUG
    printf("[OptionsDialog] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveOptionsDialogStartup() { RemoveHook(&g_hookStartup); }

void InstallOptionsDialogOnEvent() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140072f20);
    InstallHook(target, (void*)Reimpl_OnEvent, &g_hookOnEvent, 15);
#ifdef _DEBUG
    printf("[OptionsDialog] OnEvent hooked at %p\n", (void*)target);
#endif
}
void RemoveOptionsDialogOnEvent() { RemoveHook(&g_hookOnEvent); }
