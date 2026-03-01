// engine/reimpl/tutorialgoaldialog.cpp
// Reimplementation of TutorialGoalDialog vtable methods from Application-x64.exe.
//
// TutorialGoalDialog (0x40 bytes), vtable at 0x140160870.
// Extends Object::External (NOT DialogInstance).
// Object::External base ~0x20 bytes, subclass fields start at +0x20.
//
// Vtable layout (10 slots — Object::External base):
//   +0x00: 0x140027590 — ScalarDtor (hookable, 14b prologue)
//   +0x08: 0x140027430 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140027420 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400df080 — Slot3: MOV EAX,1; RET (6b, unhookable)
//   +0x20: 0x14003a912 — Runtime thunk (6b, unhookable)
//   +0x28: 0x1400defd0 — Shutdown: serialize/cleanup (hookable, 14b)
//   +0x30: 0x140027520 — OnEvent: handle goal dialog events (hookable, 14b)
//   +0x38: 0x140001640 — Delegator: MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX] (8b, unhookable)
//   +0x40: 0x1400deee0 — OnInit: initialize tutorial goal state (hookable, 13b)
//   +0x48: 0x1400274b0 — OnDestroy: cleanup tutorial goal dialog (hookable, 14b)

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
static HookInfo g_hookShutdown;
static HookInfo g_hookOnEvent;
static HookInfo g_hookOnInit;
static HookInfo g_hookOnDestroy;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot 0: ScalarDtor(this, flags)
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialGoalDialog::ScalarDtor";
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

// Slot 5: Shutdown(this, stream) — serialize/cleanup
typedef uint64_t (*fn_Shutdown_t)(void* self, void* stream);
static uint64_t Reimpl_Shutdown(void* self, void* stream) {
    g_lastHook = "TutorialGoalDialog::Shutdown";
    return ((fn_Shutdown_t)g_hookShutdown.trampoline)(self, stream);
}

// Slot 6: OnEvent(this, p2, p3, p4) — handle goal dialog events
typedef void* (*fn_OnEvent_t)(void* self, uint64_t p2, uint64_t p3, uint64_t p4);
static void* Reimpl_OnEvent(void* self, uint64_t p2, uint64_t p3, uint64_t p4) {
    g_lastHook = "TutorialGoalDialog::OnEvent";
    return ((fn_OnEvent_t)g_hookOnEvent.trampoline)(self, p2, p3, p4);
}

// Slot 8: OnInit(this) — initialize tutorial goal state
typedef uint64_t (*fn_OnInit_t)(void* self);
static uint64_t Reimpl_OnInit(void* self) {
    g_lastHook = "TutorialGoalDialog::OnInit";
    return ((fn_OnInit_t)g_hookOnInit.trampoline)(self);
}

// Slot 9: OnDestroy(this) — cleanup tutorial goal dialog
typedef void* (*fn_OnDestroy_t)(void* self);
static void* Reimpl_OnDestroy(void* self) {
    g_lastHook = "TutorialGoalDialog::OnDestroy";
    return ((fn_OnDestroy_t)g_hookOnDestroy.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallTutorialGoalDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027590);
    // Prologue: MOV [rsp+8],rcx(5) + PUSH RDI(1) + SUB RSP,0x30(4) + MOV qword,-2(9) = 19
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallTutorialGoalDialogShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400defd0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTutorialGoalDialogOnEvent() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027520);
    // Prologue: PUSH RBX(2) + SUB RSP,0x30(4) + MOV qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_OnEvent, &g_hookOnEvent, 15);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] OnEvent hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogOnEvent() { RemoveHook(&g_hookOnEvent); }

void InstallTutorialGoalDialogOnInit() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400deee0);
    InstallHook(target, (void*)Reimpl_OnInit, &g_hookOnInit, 13);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] OnInit hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogOnInit() { RemoveHook(&g_hookOnInit); }

void InstallTutorialGoalDialogOnDestroy() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400274b0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x30(4) + MOV qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_OnDestroy, &g_hookOnDestroy, 15);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] OnDestroy hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogOnDestroy() { RemoveHook(&g_hookOnDestroy); }
