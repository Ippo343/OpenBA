// engine/reimpl/tutorialinstance.cpp
// Reimplementation of TutorialInstance vtable methods from Application-x64.exe.
//
// TutorialInstance (0x38 bytes), vtable at 0x140160bf8.
// Extends Object::Internal.
//
// Vtable layout (4 slots):
//   +0x00: 0x1400281a0 — Dtor (19b prologue)
//   +0x08: 0x140028110 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140028100 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400e1520 — Serialize (15b prologue)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookTutorialInstanceDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialInstance::Dtor";
    return ((fn_Dtor_t)g_hookTutorialInstanceDtor.trampoline)(self, flags);
}

void InstallTutorialInstanceDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400281a0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookTutorialInstanceDtor, 19);
#ifdef _DEBUG
    printf("[TutorialInstance] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialInstanceDtor() { RemoveHook(&g_hookTutorialInstanceDtor); }

// --- Slot3: Serialize ---
static HookInfo g_hookTutorialInstanceSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "TutorialInstance::Serialize";
    ((fn_Serialize_t)g_hookTutorialInstanceSerialize.trampoline)(self, stream);
}

void InstallTutorialInstanceSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e1520);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookTutorialInstanceSerialize, 15);
#ifdef _DEBUG
    printf("[TutorialInstance] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialInstanceSerialize() { RemoveHook(&g_hookTutorialInstanceSerialize); }
