// engine/reimpl/statusiconui.cpp
// Reimplementation of StatusIconUI vtable methods from Application-x64.exe.
//
// StatusIconUI (0x40 bytes), vtable at 0x14015d8a0.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140014370 — ScalarDtor(self, flags) [19b prologue]
//   +0x20: 0x14007b270 — Serialize(self) [13b prologue]
//   +0x38: 0x14007b0a0 — Startup(self) [13b prologue]
//
// Unhookable (too small or thunk):
//   +0x08: 0x140014170 — GetDescription, 8 bytes
//   +0x10: 0x140014160 — GetTypeId, 7 bytes
//   +0x18: 0x14003a95a — thunk (6b JMP)
//   +0x28: 0x14003a960 — thunk (6b JMP)
//   +0x30: 0x14003a966 — thunk (6b JMP)
//   +0x40: 0x14003a96c — thunk (6b JMP)
//   +0x48: 0x14003a972 — thunk (6b JMP)
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

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "StatusIconUI::ScalarDtor";
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

typedef void (*fn_Serialize_t)(void* self);
static void Reimpl_Serialize(void* self) {
    g_lastHook = "StatusIconUI::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self);
}

typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    g_lastHook = "StatusIconUI::Startup";
    ((fn_Startup_t)g_hookStartup.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallStatusIconUIScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140014370);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[StatusIconUI] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusIconUIScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallStatusIconUISerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007b270);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[StatusIconUI] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusIconUISerialize() { RemoveHook(&g_hookSerialize); }

void InstallStatusIconUIStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007b0a0);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 15);
#ifdef _DEBUG
    printf("[StatusIconUI] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusIconUIStartup() { RemoveHook(&g_hookStartup); }
