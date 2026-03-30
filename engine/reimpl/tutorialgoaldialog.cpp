// engine/reimpl/tutorialgoaldialog.cpp
// Reimplementation of TutorialGoalDialog vtable methods from Application-x64.exe.
//
// TutorialGoalDialog (0x40 bytes), vtable at 0x140160870.
// Extends Object::External (NOT DialogInstance).
//
// All 5 hookable slots delegate to clean C++ in code/Game/Tutorial/tutorialgoaldialog.cpp.
// All 5 functions are SHARED across multiple External subclass vtables.
// Must use vtable patching (not prologue hooks) to avoid corrupting other classes.

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Tutorial/tutorialgoaldialog_funcs.h"
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
// Reimpl wrappers — delegate to clean code
// ============================================================

static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialGoalDialog::ScalarDtor";
    return TutorialGoalDialog_ScalarDtor(self, flags);
}

static uint64_t Reimpl_Shutdown(void* self, void* stream) {
    g_lastHook = "TutorialGoalDialog::Shutdown";
    return TutorialGoalDialog_Shutdown(self, stream);
}

static void* Reimpl_OnEvent(void* self, uint64_t p2, uint64_t p3, uint64_t p4) {
    g_lastHook = "TutorialGoalDialog::OnEvent";
    return TutorialGoalDialog_OnEvent(self, p2, p3, p4);
}

static uint64_t Reimpl_OnInit(void* self) {
    g_lastHook = "TutorialGoalDialog::OnInit";
    return TutorialGoalDialog_OnInit(self);
}

static void* Reimpl_OnDestroy(void* self) {
    g_lastHook = "TutorialGoalDialog::OnDestroy";
    return TutorialGoalDialog_OnDestroy(self);
}

// ============================================================
// Install / Remove — vtable patching on 0x140160870
// ============================================================

void InstallTutorialGoalDialogScalarDtor() {
    InstallHookVTable(0x140160870, 0, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] ScalarDtor vtable-patched (slot 0)\n");
#endif
}
void RemoveTutorialGoalDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallTutorialGoalDialogShutdown() {
    InstallHookVTable(0x140160870, 5, (void*)Reimpl_Shutdown, &g_hookShutdown);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] Shutdown vtable-patched (slot 5)\n");
#endif
}
void RemoveTutorialGoalDialogShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTutorialGoalDialogOnEvent() {
    InstallHookVTable(0x140160870, 6, (void*)Reimpl_OnEvent, &g_hookOnEvent);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] OnEvent vtable-patched (slot 6)\n");
#endif
}
void RemoveTutorialGoalDialogOnEvent() { RemoveHook(&g_hookOnEvent); }

void InstallTutorialGoalDialogOnInit() {
    InstallHookVTable(0x140160870, 8, (void*)Reimpl_OnInit, &g_hookOnInit);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] OnInit vtable-patched (slot 8)\n");
#endif
}
void RemoveTutorialGoalDialogOnInit() { RemoveHook(&g_hookOnInit); }

void InstallTutorialGoalDialogOnDestroy() {
    InstallHookVTable(0x140160870, 9, (void*)Reimpl_OnDestroy, &g_hookOnDestroy);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] OnDestroy vtable-patched (slot 9)\n");
#endif
}
void RemoveTutorialGoalDialogOnDestroy() { RemoveHook(&g_hookOnDestroy); }
