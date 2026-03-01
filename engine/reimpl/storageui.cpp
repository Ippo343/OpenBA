// engine/reimpl/storageui.cpp
// Reimplementation of StorageUI vtable methods from Application-x64.exe.
//
// StorageUI (0x40 bytes), vtable at 0x14015f970.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140021390 — ScalarDtor(self, flags) [19b prologue]
//   +0x20: 0x1400b8820 — Serialize(self) [15b prologue]
//   +0x38: 0x1400b8450 — Startup(self) [17b prologue]
//
// Unhookable (too small or thunk):
//   +0x08: 0x140021320 — GetDescription, 8 bytes
//   +0x10: 0x140021310 — GetTypeId, 7 bytes
//   +0x18: 0x1400b8ca0 — Shutdown, 3 bytes (RET 0)
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
    g_lastHook = "StorageUI::ScalarDtor";
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

typedef void (*fn_Serialize_t)(void* self);
static void Reimpl_Serialize(void* self) {
    g_lastHook = "StorageUI::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self);
}

typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    g_lastHook = "StorageUI::Startup";
    ((fn_Startup_t)g_hookStartup.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallStorageUIScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140021390);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[StorageUI] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveStorageUIScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallStorageUISerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400b8820);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 18);
#ifdef _DEBUG
    printf("[StorageUI] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveStorageUISerialize() { RemoveHook(&g_hookSerialize); }

void InstallStorageUIStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400b8450);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 17);
#ifdef _DEBUG
    printf("[StorageUI] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveStorageUIStartup() { RemoveHook(&g_hookStartup); }
