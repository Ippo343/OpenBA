// engine/reimpl/workplaceui.cpp
// Reimplementation of WorkPlaceUI vtable methods from Application-x64.exe.
//
// WorkPlaceUI (0x68 bytes), vtable at 0x14015e030.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140016e90 — ScalarDtor(self, flags) [19b prologue]
//   +0x20: 0x140087940 — Serialize(self) [15b prologue]
//   +0x38: 0x140087080 — Startup(self) [17b prologue]
//   +0x48: 0x1400877e0 — OnRemove(self, p2, eventType, eventId) [15b prologue]
//
// Unhookable (too small or thunk):
//   +0x08: 0x140016db0 — GetDescription, 8 bytes
//   +0x10: 0x140016da0 — GetTypeId, 7 bytes
//   +0x18: 0x14003a95a — thunk (6b JMP)
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
    g_lastHook = "WorkPlaceUI::ScalarDtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, WorkPlaceUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize — REIMPLEMENTED (finds worker/job elements, sets IDs)
#include "Dialog/ui_panels.h"
static void Reimpl_Serialize(void* self) {
    g_lastHook = "WorkPlaceUI::Serialize";
    WorkplaceUI_Serialize((WorkPlaceUI*)self);
}

// Startup — REIMPLEMENTED (worker count display update)
#include "Dialog/ui_panels.h"
typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    g_lastHook = "WorkPlaceUI::Startup";
    WorkplaceUI_Startup((WorkPlaceUI*)self);
}

// OnRemove — REIMPLEMENTED (button dispatch)
typedef void (*fn_OnRemove_t)(void* self, void* p2, int eventType, int eventId);
static void Reimpl_OnRemove(void* self, void* p2, int eventType, int eventId) {
    g_lastHook = "WorkPlaceUI::OnRemove";
    WorkplaceUI_OnRemove((WorkPlaceUI*)self, p2, eventType, eventId);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallWorkPlaceUIScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140016e90);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[WorkPlaceUI] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveWorkPlaceUIScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallWorkPlaceUISerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140087940);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 16);
#ifdef _DEBUG
    printf("[WorkPlaceUI] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveWorkPlaceUISerialize() { RemoveHook(&g_hookSerialize); }

void InstallWorkPlaceUIStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140087080);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 17);
#ifdef _DEBUG
    printf("[WorkPlaceUI] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveWorkPlaceUIStartup() { RemoveHook(&g_hookStartup); }

void InstallWorkPlaceUIOnRemove() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400877e0);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 15);
#ifdef _DEBUG
    printf("[WorkPlaceUI] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveWorkPlaceUIOnRemove() { RemoveHook(&g_hookOnRemove); }
