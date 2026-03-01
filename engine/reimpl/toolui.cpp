// engine/reimpl/toolui.cpp
// Reimplementation of ToolUI vtable methods from Application-x64.exe.
//
// ToolUI (0x58 bytes), vtable at 0x14015da70.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x1400145f0 — ScalarDtor(self, flags) [19b prologue]
//   +0x20: 0x14007bd70 — Serialize(self) [15b prologue]
//   +0x40: 0x14007b9b0 — OnCreate(self) [13b prologue]
//   +0x48: 0x14007bc30 — OnRemove(self, ?, eventType, eventId, p5, p6) [13b prologue]
//
// Unhookable (too small or thunk):
//   +0x08: 0x140014570 — GetDescription, 8 bytes
//   +0x10: 0x140014560 — GetTypeId, 7 bytes
//   +0x18: 0x14007c070 — Shutdown, 3 bytes (RET 0)
//   +0x28: 0x14003a960 — thunk (6b JMP)
//   +0x30: 0x14003a966 — thunk (6b JMP)
//   +0x38: 0x14003a97e — thunk (6b JMP)
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
static HookInfo g_hookOnCreate;
static HookInfo g_hookOnRemove;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "ToolUI::ScalarDtor";
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

typedef void (*fn_Serialize_t)(void* self);
static void Reimpl_Serialize(void* self) {
    g_lastHook = "ToolUI::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self);
}

typedef void (*fn_OnCreate_t)(void* self);
static void Reimpl_OnCreate(void* self) {
    g_lastHook = "ToolUI::OnCreate";
    ((fn_OnCreate_t)g_hookOnCreate.trampoline)(self);
}

typedef void (*fn_OnRemove_t)(void* self, void* param2, int eventType, int eventId, int p5, int p6);
static void Reimpl_OnRemove(void* self, void* param2, int eventType, int eventId, int p5, int p6) {
    g_lastHook = "ToolUI::OnRemove";
    ((fn_OnRemove_t)g_hookOnRemove.trampoline)(self, param2, eventType, eventId, p5, p6);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallToolUIScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400145f0);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[ToolUI] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveToolUIScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallToolUISerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007bd70);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 16);
#ifdef _DEBUG
    printf("[ToolUI] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveToolUISerialize() { RemoveHook(&g_hookSerialize); }

void InstallToolUIOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007b9b0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 15);
#ifdef _DEBUG
    printf("[ToolUI] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveToolUIOnCreate() { RemoveHook(&g_hookOnCreate); }

void InstallToolUIOnRemove() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007bc30);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 13);
#ifdef _DEBUG
    printf("[ToolUI] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveToolUIOnRemove() { RemoveHook(&g_hookOnRemove); }
