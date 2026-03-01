// engine/reimpl/tutorialsetupdialog.cpp
// Reimplementation of TutorialSetupDialog vtable methods from Application-x64.exe.
//
// TutorialSetupDialog (0x78 bytes), vtable at 0x140160d20.
// Extends Object::External (NOT DialogInstance).
// Object::External base ~0x20 bytes, subclass fields start at +0x20.
// Has Ctor at 0x1400e2cc0, intermediate vtable at 0x140160c30.
//
// Vtable layout (12 slots — Object::External base + extensions):
//   +0x00: 0x140028620 — ScalarDtor (hookable, 15b prologue)
//   +0x08: 0x1400285a0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140028590 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400e2be0 — Slot3: MOV EAX,6; RET (6b, unhookable)
//   +0x20: 0x14003a912 — Runtime thunk (6b, unhookable)
//   +0x28: 0x1400e2a60 — Shutdown: serialize/cleanup tutorial setup (hookable, 14b)
//   +0x30: 0x140028600 — OnEvent: delegate to vtable[0], then cleanup (hookable, 14b)
//   +0x38: 0x140001640 — Delegator: MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX] (8b, unhookable)
//   +0x40: 0x1400e25f0 — Slot8: rel CALL in prologue (unhookable)
//   +0x48: 0x1400e24c0 — Slot9: rel CALL in prologue (unhookable)
//   +0x50: 0x1400e2460 — Slot10: rel CALL in prologue (unhookable)
//   +0x58: 0x1400285f0 — Slot11: TEST+branch (12b, unhookable)

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

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot 0: ScalarDtor(this, flags)
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

// Slot 5: Shutdown(this, stream) — serialize/cleanup tutorial setup state
typedef uint64_t (*fn_Shutdown_t)(void* self, void* stream);
static uint64_t Reimpl_Shutdown(void* self, void* stream) {
    return ((fn_Shutdown_t)g_hookShutdown.trampoline)(self, stream);
}

// Slot 6: OnEvent(this) — delegate to vtable[0] dtor, then cleanup
typedef void (*fn_OnEvent_t)(void* self);
static void Reimpl_OnEvent(void* self) {
    ((fn_OnEvent_t)g_hookOnEvent.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallTutorialSetupDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028620);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 15);
#ifdef _DEBUG
    printf("[TutorialSetupDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallTutorialSetupDialogShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e2a60);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[TutorialSetupDialog] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupDialogShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTutorialSetupDialogOnEvent() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028600);
    InstallHook(target, (void*)Reimpl_OnEvent, &g_hookOnEvent, 14);
#ifdef _DEBUG
    printf("[TutorialSetupDialog] OnEvent hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupDialogOnEvent() { RemoveHook(&g_hookOnEvent); }
