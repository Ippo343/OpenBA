// engine/reimpl/titledesctooltipui.cpp
// Reimplementation of TitleDescToolTipUI vtable methods from Application-x64.exe.
//
// TitleDescToolTipUI (0x30 bytes), vtable at 0x14015d9f0.
// Extends DialogController (0x30 base). No subclass fields.
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140014510 — ScalarDtor(self, flags) [19b prologue]
//   +0x20: 0x14007b730 — Serialize(self) [13b prologue]
//
// Unhookable (too small or thunk):
//   +0x08: 0x1400144b0 — GetDescription, 8 bytes
//   +0x10: 0x1400144a0 — GetTypeId, 7 bytes
//   +0x18: 0x14007b8a0 — Shutdown, 3 bytes (RET 0)
//   +0x28: 0x14003a960 — thunk (6b JMP)
//   +0x30: 0x14003a966 — thunk (6b JMP)
//   +0x38: 0x14003a97e — thunk (6b JMP)
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

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

typedef void (*fn_Serialize_t)(void* self);
static void Reimpl_Serialize(void* self) {
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTitleDescToolTipUIScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140014510);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[TitleDescToolTipUI] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTitleDescToolTipUIScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallTitleDescToolTipUISerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007b730);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 13);
#ifdef _DEBUG
    printf("[TitleDescToolTipUI] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTitleDescToolTipUISerialize() { RemoveHook(&g_hookSerialize); }
