// engine/reimpl/tutorialui.cpp
// Reimplementation of TutorialUI vtable methods from Application-x64.exe.
//
// TutorialUI (0x60 bytes), vtable at 0x140160f30.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140029470 — ScalarDtor(self, flags) [19b prologue]
//   +0x20: 0x1400e37a0 — Serialize(self) [15b prologue]
//   +0x38: 0x1400e3700 — Startup(self) [13b prologue]
//   +0x48: 0x1400e36e0 — OnRemove(self, ?, eventType, eventId) [13b prologue]
//
// Unhookable (too small or thunk):
//   +0x08: 0x1400293f0 — GetDescription, 8 bytes
//   +0x10: 0x1400293e0 — GetTypeId, 7 bytes
//   +0x18: 0x1400e3aa0 — Shutdown, 3 bytes (RET 0)
//   +0x28: 0x14003a960 — thunk (6b JMP)
//   +0x30: 0x14003a966 — thunk (6b JMP)
//   +0x40: 0x14003a96c — thunk (6b JMP)
//   +0x50: 0x14003a978 — thunk (6b JMP)

#include "ui/Panels.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookScalarDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookStartup;
static HookInfo g_hookOnRemove;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialUI::ScalarDtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, TutorialUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize — REIMPLEMENTED (finds named elements, sets button IDs, hides close)
#include "Dialog/ui_panels.h"
static void Reimpl_Serialize(void* self) {
    g_lastHook = "TutorialUI::Serialize";
    TutorialUI_Serialize((TutorialUI*)self);
}

// Startup — REIMPLEMENTED (toggles button visibility)
static void Reimpl_Startup(void* self) {
    g_lastHook = "TutorialUI::Startup";
    TutorialUI_Startup((TutorialUI*)self);
}

// OnRemove — REIMPLEMENTED (quitButton: destroy SaveLoad, transition to main menu)
static void Reimpl_OnRemove(void* self, void* param2, int eventType, int eventId) {
    g_lastHook = "TutorialUI::OnRemove";
    TutorialUI_OnRemove((TutorialUI*)self, param2, eventType, eventId);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTutorialUIScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140029470);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[TutorialUI] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialUIScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallTutorialUISerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e37a0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 22);
#ifdef _DEBUG
    printf("[TutorialUI] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialUISerialize() { RemoveHook(&g_hookSerialize); }

void InstallTutorialUIStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e3700);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 13);
#ifdef _DEBUG
    printf("[TutorialUI] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialUIStartup() { RemoveHook(&g_hookStartup); }

void InstallTutorialUIOnRemove() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e36e0);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 13);
#ifdef _DEBUG
    printf("[TutorialUI] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialUIOnRemove() { RemoveHook(&g_hookOnRemove); }
