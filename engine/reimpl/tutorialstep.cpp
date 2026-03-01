// engine/reimpl/tutorialstep.cpp
// Reimplementation of TutorialStep vtable methods from Application-x64.exe.
//
// TutorialStep (0x70 bytes), vtable at 0x140160e90.
// Extends Object::External. Ctor at 0x1400e3480.
//
// Vtable layout (9 slots):
//   +0x00: 0x140028cc0 — Dtor (12b prologue)
//   +0x08: 0x140028c40 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140028c30 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400e33e0 — GetDescription (6b, unhookable, returns 1)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400e32a0 — Serialize (17b prologue)
//   +0x30: 0x140028ca0 — Cleanup (14b prologue)
//   +0x38: 0x140001640 — Base dispatch (8b, unhookable)
//   +0x40: 0x140028c90 — Ctor/Init (12b prologue)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookTutorialStepDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookTutorialStepDtor.trampoline)(self, flags);
}

void InstallTutorialStepDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028cc0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookTutorialStepDtor, 12);
#ifdef _DEBUG
    printf("[TutorialStep] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialStepDtor() { RemoveHook(&g_hookTutorialStepDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookTutorialStepSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    ((fn_Serialize_t)g_hookTutorialStepSerialize.trampoline)(self, stream);
}

void InstallTutorialStepSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e32a0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookTutorialStepSerialize, 17);
#ifdef _DEBUG
    printf("[TutorialStep] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialStepSerialize() { RemoveHook(&g_hookTutorialStepSerialize); }

// --- Slot6: Cleanup ---
static HookInfo g_hookTutorialStepCleanup;
typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    ((fn_Cleanup_t)g_hookTutorialStepCleanup.trampoline)(self);
}

void InstallTutorialStepCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028ca0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookTutorialStepCleanup, 14);
#ifdef _DEBUG
    printf("[TutorialStep] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialStepCleanup() { RemoveHook(&g_hookTutorialStepCleanup); }

// --- Slot8: Ctor/Init ---
static HookInfo g_hookTutorialStepInit;
typedef void* (*fn_Init_t)(void* self);
static void* Reimpl_Init(void* self) {
    return ((fn_Init_t)g_hookTutorialStepInit.trampoline)(self);
}

void InstallTutorialStepInit() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028c90);
    InstallHook(target, (void*)Reimpl_Init, &g_hookTutorialStepInit, 12);
#ifdef _DEBUG
    printf("[TutorialStep] Init hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialStepInit() { RemoveHook(&g_hookTutorialStepInit); }
