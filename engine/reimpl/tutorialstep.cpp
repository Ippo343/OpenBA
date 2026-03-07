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
#include "TutorialStep.h"
#include <cstdio>
#include <cstdint>
#include <windows.h>

// --- Slot0: Dtor ---
static HookInfo g_hookTutorialStepDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialStep::Dtor";
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
    g_lastHook = "TutorialStep::Serialize";
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
    g_lastHook = "TutorialStep::Cleanup";
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
// RE-ENABLED: near Jcc wrapper at 0x140028c90. Use vtable patch on slot 8.
// Vtable slot 8 (+0x40) of TutorialStep — the Init/Ctor wrapper.
static void* g_savedTutorialStepInit = nullptr;
typedef void* (*fn_Init_t)(void* self);
static void* Reimpl_Init(void* self) {
    g_lastHook = "TutorialStep::Init";
    return ((fn_Init_t)g_savedTutorialStepInit)(self);
}

static void PatchVtableSlot_Step(size_t slotIndex, void* newFunc, void** outOld) {
    uintptr_t* entry = (uintptr_t*)(ResolveAddr(Module::Exe, TutorialStepAddr::Vtable) + slotIndex * 8);
    DWORD oldProt;
    VirtualProtect(entry, 8, PAGE_READWRITE, &oldProt);
    if (outOld) *outOld = (void*)*entry;
    *entry = (uintptr_t)newFunc;
    VirtualProtect(entry, 8, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), entry, 8);
}

void InstallTutorialStepInit() {
    PatchVtableSlot_Step(8, (void*)Reimpl_Init, &g_savedTutorialStepInit);
#ifdef _DEBUG
    printf("[TutorialStep] Init vtable-patched (slot 8)\n");
#endif
}
void RemoveTutorialStepInit() {
    if (g_savedTutorialStepInit) {
        PatchVtableSlot_Step(8, g_savedTutorialStepInit, nullptr);
        g_savedTutorialStepInit = nullptr;
    }
}
