// engine/reimpl/tutorial.cpp
// Reimplementation of Tutorial vtable methods from Application-x64.exe.
//
// Tutorial (0x40 bytes), vtable at 0x140160740.
// Extends Object::External.
//
// Vtable layout (9 slots):
//   +0x00: 0x140026f50 — Dtor (19b prologue)
//   +0x08: 0x140026e40 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140026e30 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400deb80 — GetDescription (6b, unhookable, returns 2)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400dead0 — Serialize (15b prologue)
//   +0x30: 0x140026f00 — Cleanup (15b prologue)
//   +0x38: 0x140001640 — Base dispatch (8b, unhookable)
//   +0x40: 0x140026eb0 — Ctor (15b prologue)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookTutorialDtor;
typedef void* (*fn_TutorialDtor_t)(void* self, uint32_t flags);
static void* Reimpl_TutorialDtor(void* self, uint32_t flags) {
    g_lastHook = "Tutorial::TutorialDtor";
    return ((fn_TutorialDtor_t)g_hookTutorialDtor.trampoline)(self, flags);
}

void InstallTutorialDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026f50);
    InstallHook(target, (void*)Reimpl_TutorialDtor, &g_hookTutorialDtor, 19);
#ifdef _DEBUG
    printf("[Tutorial] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialDtor() { RemoveHook(&g_hookTutorialDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookTutorialSerialize;
typedef void (*fn_TutorialSerialize_t)(void* self, void* stream);
static void Reimpl_TutorialSerialize(void* self, void* stream) {
    g_lastHook = "Tutorial::TutorialSerialize";
    ((fn_TutorialSerialize_t)g_hookTutorialSerialize.trampoline)(self, stream);
}

void InstallTutorialSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dead0);
    InstallHook(target, (void*)Reimpl_TutorialSerialize, &g_hookTutorialSerialize, 15);
#ifdef _DEBUG
    printf("[Tutorial] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSerialize() { RemoveHook(&g_hookTutorialSerialize); }

// --- Slot6: Cleanup ---
static HookInfo g_hookTutorialCleanup;
typedef void (*fn_TutorialCleanup_t)(void* self);
static void Reimpl_TutorialCleanup(void* self) {
    g_lastHook = "Tutorial::TutorialCleanup";
    ((fn_TutorialCleanup_t)g_hookTutorialCleanup.trampoline)(self);
}

void InstallTutorialCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026f00);
    InstallHook(target, (void*)Reimpl_TutorialCleanup, &g_hookTutorialCleanup, 15);
#ifdef _DEBUG
    printf("[Tutorial] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialCleanup() { RemoveHook(&g_hookTutorialCleanup); }

// --- Slot8: Ctor ---
static HookInfo g_hookTutorialCtor;
typedef void* (*fn_TutorialCtor_t)(void* self);
static void* Reimpl_TutorialCtor(void* self) {
    g_lastHook = "Tutorial::TutorialCtor";
    return ((fn_TutorialCtor_t)g_hookTutorialCtor.trampoline)(self);
}

void InstallTutorialCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026eb0);
    InstallHook(target, (void*)Reimpl_TutorialCtor, &g_hookTutorialCtor, 15);
#ifdef _DEBUG
    printf("[Tutorial] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialCtor() { RemoveHook(&g_hookTutorialCtor); }
