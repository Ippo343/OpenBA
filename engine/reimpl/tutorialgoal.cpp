// engine/reimpl/tutorialgoal.cpp
// Reimplementation of TutorialGoal vtable methods from Application-x64.exe.
//
// TutorialGoal (ABSTRACT), vtable at 0x1401607a0.
// Extends Object::External.
//
// Vtable layout (9 slots):
//   +0x00: 0x140027220 — Dtor (19b prologue)
//   +0x08: 0x140027210 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140027200 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400dec70 — GetDescription (6b, unhookable, returns 1)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400dec30 — Serialize (13b prologue)
//   +0x30: 0x14003addc — pure virtual / no-op
//   +0x38: 0x14003addc — pure virtual / no-op
//   +0x40: 0x14003addc — pure virtual / no-op

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Tutorial/tutorialgoal_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookTutorialGoalDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialGoal::Dtor";
    return TutorialGoal_Dtor(self, flags);
}

void InstallTutorialGoalDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027220);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookTutorialGoalDtor, 19);
#ifdef _DEBUG
    printf("[TutorialGoal] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDtor() { RemoveHook(&g_hookTutorialGoalDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookTutorialGoalSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "TutorialGoal::Serialize";
    TutorialGoal_Serialize(self, stream);
}

void InstallTutorialGoalSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dec30);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookTutorialGoalSerialize, 13);
#ifdef _DEBUG
    printf("[TutorialGoal] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalSerialize() { RemoveHook(&g_hookTutorialGoalSerialize); }
