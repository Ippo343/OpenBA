// engine/reimpl/optioninputbinddialog.cpp
// Reimplementation of OptionInputBindDialog vtable methods from Application-x64.exe.
//
// OptionInputBindDialog (0xC8 bytes), vtable at 0x14015ce40.
// Extends MessageDialog (NOT DialogInstance directly).
// MessageDialog base vtable at 0x14015c480.
//
// Vtable layout (14 slots — extended from MessageDialog's 10):
//   +0x00: 0x140012f80 — ScalarDtor (hookable, 14b prologue)
//   +0x08: 0x140012e50 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140012e40 — GetTypeId (7b, unhookable)
//   +0x18: 0x14003a948 — Runtime thunk (6b, unhookable)
//   +0x20: 0x14006d140 — Startup: set up input bind dialog UI (hookable, 13b)
//   +0x28: 0x14006d0c0 — Shutdown: RET 0 (3b, unhookable)
//   +0x30: 0x14003a98a — Runtime thunk (6b, unhookable)
//   +0x38: 0x140071c80 — Load: load input binding data (hookable, 15b)
//   +0x40: 0x14006d070 — OnEvent: branch-based handler (rel branch, unhookable)
//   +0x48: 0x14003a954 — Runtime thunk (6b, unhookable)
//   +0x50: 0x14006cff0 — OnOk: set binding value=1, close (hookable, 13b)
//   +0x58: 0x14006d030 — OnCancel: set binding value=0, close (hookable, 13b)
//   +0x60: 0x14006cfb0 — OnButton2: set binding value=2, close (hookable, 13b)
//   +0x68: 0x14006cf70 — OnButton3: set binding value=3, close (hookable, 13b)

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
static HookInfo g_hookLoad;
static HookInfo g_hookOnOk;
static HookInfo g_hookOnCancel;
static HookInfo g_hookOnButton2;
static HookInfo g_hookOnButton3;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot 0: ScalarDtor(this, flags)
// Sets MessageDialog vtable (parent class), no cleanup fields, calls DialogInstance dtor
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "OptionInputBindDialog::ScalarDtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, OptionInputBindDialogAddr::BaseVtable);
    *(void**)self = s_vtable;
    fn_DialogInstance_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot 4: Startup(this) — set up input bind dialog UI
typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    g_lastHook = "OptionInputBindDialog::Startup";
    ((fn_Startup_t)g_hookStartup.trampoline)(self);
}

// Slot 7: Load(this) — load input binding data
typedef void (*fn_Load_t)(void* self);
static void Reimpl_Load(void* self) {
    g_lastHook = "OptionInputBindDialog::Load";
    ((fn_Load_t)g_hookLoad.trampoline)(self);
}

// Slot 10: OnOk(this) — set binding value=1, close dialog
typedef void (*fn_OnOk_t)(void* self);
static void Reimpl_OnOk(void* self) {
    g_lastHook = "OptionInputBindDialog::OnOk";
    ((fn_OnOk_t)g_hookOnOk.trampoline)(self);
}

// Slot 11: OnCancel(this) — set binding value=0, close dialog
typedef void (*fn_OnCancel_t)(void* self);
static void Reimpl_OnCancel(void* self) {
    g_lastHook = "OptionInputBindDialog::OnCancel";
    ((fn_OnCancel_t)g_hookOnCancel.trampoline)(self);
}

// Slot 12: OnButton2(this) — set binding value=2, close dialog
typedef void (*fn_OnButton2_t)(void* self);
static void Reimpl_OnButton2(void* self) {
    g_lastHook = "OptionInputBindDialog::OnButton2";
    ((fn_OnButton2_t)g_hookOnButton2.trampoline)(self);
}

// Slot 13: OnButton3(this) — set binding value=3, close dialog
typedef void (*fn_OnButton3_t)(void* self);
static void Reimpl_OnButton3(void* self) {
    g_lastHook = "OptionInputBindDialog::OnButton3";
    ((fn_OnButton3_t)g_hookOnButton3.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallOptionInputBindDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012f80);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[OptionInputBindDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveOptionInputBindDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallOptionInputBindDialogStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006d140);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 19);
#ifdef _DEBUG
    printf("[OptionInputBindDialog] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveOptionInputBindDialogStartup() { RemoveHook(&g_hookStartup); }

void InstallOptionInputBindDialogLoad() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140071c80);
    InstallHook(target, (void*)Reimpl_Load, &g_hookLoad, 15);
#ifdef _DEBUG
    printf("[OptionInputBindDialog] Load hooked at %p\n", (void*)target);
#endif
}
void RemoveOptionInputBindDialogLoad() { RemoveHook(&g_hookLoad); }

void InstallOptionInputBindDialogOnOk() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006cff0);
    InstallHook(target, (void*)Reimpl_OnOk, &g_hookOnOk, 16);
#ifdef _DEBUG
    printf("[OptionInputBindDialog] OnOk hooked at %p\n", (void*)target);
#endif
}
void RemoveOptionInputBindDialogOnOk() { RemoveHook(&g_hookOnOk); }

void InstallOptionInputBindDialogOnCancel() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006d030);
    InstallHook(target, (void*)Reimpl_OnCancel, &g_hookOnCancel, 16);
#ifdef _DEBUG
    printf("[OptionInputBindDialog] OnCancel hooked at %p\n", (void*)target);
#endif
}
void RemoveOptionInputBindDialogOnCancel() { RemoveHook(&g_hookOnCancel); }

void InstallOptionInputBindDialogOnButton2() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006cfb0);
    InstallHook(target, (void*)Reimpl_OnButton2, &g_hookOnButton2, 16);
#ifdef _DEBUG
    printf("[OptionInputBindDialog] OnButton2 hooked at %p\n", (void*)target);
#endif
}
void RemoveOptionInputBindDialogOnButton2() { RemoveHook(&g_hookOnButton2); }

void InstallOptionInputBindDialogOnButton3() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006cf70);
    InstallHook(target, (void*)Reimpl_OnButton3, &g_hookOnButton3, 16);
#ifdef _DEBUG
    printf("[OptionInputBindDialog] OnButton3 hooked at %p\n", (void*)target);
#endif
}
void RemoveOptionInputBindDialogOnButton3() { RemoveHook(&g_hookOnButton3); }
