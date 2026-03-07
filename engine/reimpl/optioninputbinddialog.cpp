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
static HookInfo g_hookLoad;
static HookInfo g_hookOnEvent;
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

// Slot 4: Startup(this) — REIMPLEMENTED
static void Reimpl_Startup(void* self) {
    g_lastHook = "OptionInputBindDialog::Startup";
    OptionInputBindDialog_Startup((OptionInputBindDialog*)self);
}

// Slot 7: Load(this) — REIMPLEMENTED
static void Reimpl_Load(void* self) {
    g_lastHook = "OptionInputBindDialog::Load";
    OptionInputBindDialog_Load((OptionInputBindDialog*)self);
}

// Slot 8: OnEvent(this, element, eventType, eventData) — REIMPLEMENTED
static void Reimpl_OnEvent(void* self, void* element, int eventType, int eventData) {
    g_lastHook = "OptionInputBindDialog::OnEvent";
    OptionInputBindDialog_OnEvent((OptionInputBindDialog*)self, element, eventType, eventData);
}

// Slot 10: OnOk(this) — REIMPLEMENTED
static void Reimpl_OnOk(void* self) {
    g_lastHook = "OptionInputBindDialog::OnOk";
    OptionInputBindDialog_OnOk((OptionInputBindDialog*)self);
}

// Slot 11: OnCancel(this) — REIMPLEMENTED
static void Reimpl_OnCancel(void* self) {
    g_lastHook = "OptionInputBindDialog::OnCancel";
    OptionInputBindDialog_OnCancel((OptionInputBindDialog*)self);
}

// Slot 12: OnButton2(this) — REIMPLEMENTED
static void Reimpl_OnButton2(void* self) {
    g_lastHook = "OptionInputBindDialog::OnButton2";
    OptionInputBindDialog_OnButton2((OptionInputBindDialog*)self);
}

// Slot 13: OnButton3(this) — REIMPLEMENTED
static void Reimpl_OnButton3(void* self) {
    g_lastHook = "OptionInputBindDialog::OnButton3";
    OptionInputBindDialog_OnButton3((OptionInputBindDialog*)self);
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

// OnEvent (0x14006d070) — branch at byte 4, full reimpl.
// Prologue: CMP R8d,0x12(4)+JNZ short(2)+TEST R9d,R9d(3)+JNZ short(2)+MOV RAX,[RCX](3) = 14 bytes
void InstallOptionInputBindDialogOnEvent() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006d070);
    InstallHook(target, (void*)Reimpl_OnEvent, &g_hookOnEvent, 14);
#ifdef _DEBUG
    printf("[OptionInputBindDialog] OnEvent hooked at %p\n", (void*)target);
#endif
}
void RemoveOptionInputBindDialogOnEvent() { RemoveHook(&g_hookOnEvent); }

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
