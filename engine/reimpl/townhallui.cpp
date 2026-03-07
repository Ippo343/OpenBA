// engine/reimpl/townhallui.cpp
// Reimplementation of TownHallUI vtable methods from Application-x64.exe.
//
// TownHallUI (0x1E8 bytes), vtable at 0x14015dce0. Second-largest panel.
// Extends DialogController (0x30 base). Has Ctor at 0x140081280.
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x1400146b0 — ScalarDtor(self, flags) [15b prologue]
//   +0x20: 0x140081000 — Serialize(self) [15b prologue]
//   +0x38: 0x14007d550 — Startup(self) [13b prologue]
//   +0x40: 0x14007e550 — OnCreate(self) [13b prologue]
//   +0x48: 0x14007c180 — OnRemove(self, ?, eventType, ...) [13b prologue]
//   +0x50: 0x140080db0 — Update(self, stream) [15b prologue]
//
// Unhookable (too small or thunk):
//   +0x08: 0x140014650 — GetDescription, 8 bytes
//   +0x10: 0x140014640 — GetTypeId, 7 bytes
//   +0x18: 0x14003a95a — thunk (6b JMP)
//   +0x28: 0x14003a960 — thunk (6b JMP)
//   +0x30: 0x14003a966 — thunk (6b JMP)

#include "ui/Panels.h"
#include "Dialog/ui_panels.h"
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
static HookInfo g_hookOnCreate;
static HookInfo g_hookOnRemove;
static HookInfo g_hookUpdate;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "TownHallUI::ScalarDtor";
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

typedef void (*fn_Serialize_t)(void* self);
static void Reimpl_Serialize(void* self) {
    g_lastHook = "TownHallUI::Serialize";
    TownHallUI_Serialize((TownHallUI*)self);
}

typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    g_lastHook = "TownHallUI::Startup";
    ((fn_Startup_t)g_hookStartup.trampoline)(self);
}

typedef void (*fn_OnCreate_t)(void* self);
static void Reimpl_OnCreate(void* self) {
    g_lastHook = "TownHallUI::OnCreate";
    ((fn_OnCreate_t)g_hookOnCreate.trampoline)(self);
}

typedef void (*fn_OnRemove_t)(void* self, void* param2, int eventType, int eventId);
static void Reimpl_OnRemove(void* self, void* param2, int eventType, int eventId) {
    g_lastHook = "TownHallUI::OnRemove";
    ((fn_OnRemove_t)g_hookOnRemove.trampoline)(self, param2, eventType, eventId);
}

typedef void (*fn_Update_t)(void* self, void* stream);
static void Reimpl_Update(void* self, void* stream) {
    g_lastHook = "TownHallUI::Update";
    TownHallUI_Update((TownHallUI*)self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTownHallUIScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400146b0);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 15);
#ifdef _DEBUG
    printf("[TownHallUI] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTownHallUIScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallTownHallUISerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140081000);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TownHallUI] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTownHallUISerialize() { RemoveHook(&g_hookSerialize); }

void InstallTownHallUIStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007d550);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 14);
#ifdef _DEBUG
    printf("[TownHallUI] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveTownHallUIStartup() { RemoveHook(&g_hookStartup); }

void InstallTownHallUIOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007e550);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[TownHallUI] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveTownHallUIOnCreate() { RemoveHook(&g_hookOnCreate); }

void InstallTownHallUIOnRemove() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007c180);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 13);
#ifdef _DEBUG
    printf("[TownHallUI] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveTownHallUIOnRemove() { RemoveHook(&g_hookOnRemove); }

void InstallTownHallUIUpdate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140080db0);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 15);
#ifdef _DEBUG
    printf("[TownHallUI] Update hooked at %p\n", (void*)target);
#endif
}
void RemoveTownHallUIUpdate() { RemoveHook(&g_hookUpdate); }
