// engine/reimpl/toolbardialog.cpp
// Reimplementation of ToolbarDialog vtable methods from Application-x64.exe.
//
// ToolbarDialog (0x108 bytes), vtable at 0x14015fa00.
// Extends DialogInstance (0xB8 base).
// Has Ctor at 0x1400b9890.
//
// Vtable layout (10 slots):
//   +0x00: 0x140021760 — ScalarDtor (hookable, 15b prologue)
//   +0x08: 0x140021700 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x1400216f0 — GetTypeId (7b, unhookable)
//   +0x18: 0x14003a948 — Runtime thunk (6b, unhookable)
//   +0x20: 0x1400b97c0 — Startup: RET 0 (3b, unhookable)
//   +0x28: 0x1400b97b0 — Shutdown: JMP rel (5b, unhookable)
//   +0x30: 0x1400b8d40 — Update: sync toolbar buttons with game state (hookable, 13b)
//   +0x38: 0x14003a990 — Runtime thunk (6b, unhookable)
//   +0x40: 0x1400b92a0 — OnEvent: handle toolbar button clicks (hookable, 14b)
//   +0x48: 0x14003a954 — Runtime thunk (6b, unhookable)

#include "ui/Dialogs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookScalarDtor;
static HookInfo g_hookUpdate;
static HookInfo g_hookOnEvent;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot 0: ScalarDtor(this, flags)
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

// Slot 6: Update(this, param2, param3, param4) — sync toolbar buttons with game state
typedef void (*fn_Update_t)(void* self, uint64_t param2, uint64_t param3, uint64_t param4);
static void Reimpl_Update(void* self, uint64_t param2, uint64_t param3, uint64_t param4) {
    ((fn_Update_t)g_hookUpdate.trampoline)(self, param2, param3, param4);
}

// Slot 8: OnEvent(this, element, eventType, eventData, p5, p6, p7) — handle toolbar clicks
typedef void (*fn_OnEvent_t)(void* self, void* element, int eventType, int eventData,
                             uint64_t p5, uint64_t p6, int p7);
static void Reimpl_OnEvent(void* self, void* element, int eventType, int eventData,
                           uint64_t p5, uint64_t p6, int p7) {
    ((fn_OnEvent_t)g_hookOnEvent.trampoline)(self, element, eventType, eventData, p5, p6, p7);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallToolbarDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140021760);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 15);
#ifdef _DEBUG
    printf("[ToolbarDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveToolbarDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallToolbarDialogUpdate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400b8d40);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);
#ifdef _DEBUG
    printf("[ToolbarDialog] Update hooked at %p\n", (void*)target);
#endif
}
void RemoveToolbarDialogUpdate() { RemoveHook(&g_hookUpdate); }

void InstallToolbarDialogOnEvent() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400b92a0);
    InstallHook(target, (void*)Reimpl_OnEvent, &g_hookOnEvent, 16);
#ifdef _DEBUG
    printf("[ToolbarDialog] OnEvent hooked at %p\n", (void*)target);
#endif
}
void RemoveToolbarDialogOnEvent() { RemoveHook(&g_hookOnEvent); }
