// engine/reimpl/storagecapacityui.cpp
// Reimplementation of StorageCapacityUI vtable methods from Application-x64.exe.
//
// StorageCapacityUI (0x38 bytes), vtable at 0x14015d950.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140014450 — ScalarDtor(self, flags) [19b prologue]
//   +0x20: 0x14007b600 — Serialize(self) [15b prologue]
//   +0x38: 0x14007b4d0 — Startup(self) [15b prologue]
//
// Unhookable (too small or thunk):
//   +0x08: 0x1400143e0 — GetDescription, 8 bytes
//   +0x10: 0x1400143d0 — GetTypeId, 7 bytes
//   +0x18: 0x14007b6b0 — Shutdown, 3 bytes (RET 0)
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
// Reimplemented wrappers
// ============================================================

// ScalarDtor — REIMPLEMENTED
// Original: 0x140014450. Sets vtable, calls base dtor, conditional free.
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "StorageCapacityUI::ScalarDtor";

    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, StorageCapacityUIAddr::Vtable);

    ((DialogController*)self)->vtable = s_vtable;
    fn_DialogController_Dtor(self);
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// Serialize — REIMPLEMENTED (finds ProgressElement by type, stores at +0x30)
#include "Dialog/ui_panels.h"
static void Reimpl_Serialize(void* self) {
    g_lastHook = "StorageCapacityUI::Serialize";
    StorageCapacityUI_Serialize((StorageCapacityUI*)self);
}

// Startup — REIMPLEMENTED (queries storage, updates progress bar)
static void Reimpl_Startup(void* self) {
    g_lastHook = "StorageCapacityUI::Startup";
    StorageCapacityUI_Startup((StorageCapacityUI*)self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallStorageCapacityUIScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140014450);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[StorageCapacityUI] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveStorageCapacityUIScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallStorageCapacityUISerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007b600);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[StorageCapacityUI] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveStorageCapacityUISerialize() { RemoveHook(&g_hookSerialize); }

void InstallStorageCapacityUIStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007b4d0);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 15);
#ifdef _DEBUG
    printf("[StorageCapacityUI] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveStorageCapacityUIStartup() { RemoveHook(&g_hookStartup); }
