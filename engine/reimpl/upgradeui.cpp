// engine/reimpl/upgradeui.cpp
// Reimplementation of UpgradeUI vtable methods from Application-x64.exe.
//
// UpgradeUI (0x38 bytes), vtable at 0x14015df00.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140016d50 — ScalarDtor(self, flags) [19b prologue]
//   +0x20: 0x140086d90 — Serialize(self) [20b prologue]
//   +0x48: 0x140086c90 — OnRemove(self, p2, eventType, eventId) [15b prologue]
//
// Unhookable (too small or thunk):
//   +0x08: 0x140016b20 — GetDescription, 8 bytes
//   +0x10: 0x140016b10 — GetTypeId, 7 bytes
//   +0x18: 0x14003a95a — thunk (6b JMP)
//   +0x28: 0x14003a960 — thunk (6b JMP)
//   +0x30: 0x14003a966 — thunk (6b JMP)
//   +0x38: 0x140086d80 — Shutdown, 3 bytes (RET 0)
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
static HookInfo g_hookOnRemove;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "UpgradeUI::ScalarDtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, UpgradeUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

typedef void (*fn_Serialize_t)(void* self);
static void Reimpl_Serialize(void* self) {
    g_lastHook = "UpgradeUI::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self);
}

typedef void (*fn_OnRemove_t)(void* self, void* p2, int eventType, int eventId);
static void Reimpl_OnRemove(void* self, void* p2, int eventType, int eventId) {
    g_lastHook = "UpgradeUI::OnRemove";
    ((fn_OnRemove_t)g_hookOnRemove.trampoline)(self, p2, eventType, eventId);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallUpgradeUIScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140016d50);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[UpgradeUI] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveUpgradeUIScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallUpgradeUISerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140086d90);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 22);
#ifdef _DEBUG
    printf("[UpgradeUI] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveUpgradeUISerialize() { RemoveHook(&g_hookSerialize); }

void InstallUpgradeUIOnRemove() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140086c90);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 15);
#ifdef _DEBUG
    printf("[UpgradeUI] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveUpgradeUIOnRemove() { RemoveHook(&g_hookOnRemove); }
