// engine/reimpl/tutorialsetup.cpp
// Reimplementation of TutorialSetup vtable methods from Application-x64.exe.
//
// TutorialSetup (ABSTRACT), vtable at 0x140160c30.
// Extends Object::External.
//
// Vtable layout (11 slots):
//   +0x00: 0x140028390 — Dtor (19b prologue)
//   +0x08: 0x140028380 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140028370 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400e2040 — GetDescription (6b, unhookable, returns 1)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400e2000 — Serialize (13b prologue)
//   +0x30: 0x14003addc — pure virtual / no-op
//   +0x38: 0x14003addc — pure virtual / no-op
//   +0x40: 0x14003addc — pure virtual / no-op
//   +0x48: 0x14003addc — pure virtual / no-op
//   +0x50: 0x14003addc — pure virtual / no-op

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialSetup::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

void InstallTutorialSetupDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028390);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[TutorialSetup] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupDtor() { RemoveHook(&g_hookDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "TutorialSetup::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

void InstallTutorialSetupSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e2000);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 13);
#ifdef _DEBUG
    printf("[TutorialSetup] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialSetupSerialize() { RemoveHook(&g_hookSerialize); }
