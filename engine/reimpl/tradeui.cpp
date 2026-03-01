// engine/reimpl/tradeui.cpp
// Reimplementation of TradeUI vtable methods from Application-x64.exe.
//
// TradeUI (0x148 bytes), vtable at 0x14015de80. Has Ctor at 0x140086b80.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140016020 — ScalarDtor(self, flags) [15b prologue]
//   +0x20: 0x140084dd0 — Serialize(self) [15b prologue]
//   +0x28: 0x140082a70 — Draw(self) [13b prologue]
//   +0x30: 0x140082a00 — Slot6(self) [13b prologue]
//   +0x38: 0x140082150 — Startup(self) [15b prologue]
//   +0x40: 0x1400827b0 — OnCreate(self) [13b prologue]
//   +0x48: 0x140081f20 — OnRemove(self, ?, eventType, eventId) [13b prologue]
//   +0x50: 0x1400868c0 — Update(self, stream, p3, p4) [13b prologue]
//
// Unhookable (too small or thunk):
//   +0x08: 0x140015f70 — GetDescription, 8 bytes
//   +0x10: 0x140015f60 — GetTypeId, 7 bytes
//   +0x18: 0x14003a95a — thunk (6b JMP)

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
static HookInfo g_hookDraw;
static HookInfo g_hookSlot6;
static HookInfo g_hookStartup;
static HookInfo g_hookOnCreate;
static HookInfo g_hookOnRemove;
static HookInfo g_hookUpdate;

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

typedef void (*fn_Draw_t)(void* self);
static void Reimpl_Draw(void* self) {
    ((fn_Draw_t)g_hookDraw.trampoline)(self);
}

typedef void (*fn_Slot6_t)(void* self);
static void Reimpl_Slot6(void* self) {
    ((fn_Slot6_t)g_hookSlot6.trampoline)(self);
}

typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    ((fn_Startup_t)g_hookStartup.trampoline)(self);
}

typedef void (*fn_OnCreate_t)(void* self);
static void Reimpl_OnCreate(void* self) {
    ((fn_OnCreate_t)g_hookOnCreate.trampoline)(self);
}

typedef void (*fn_OnRemove_t)(void* self, void* param2, int eventType, int eventId);
static void Reimpl_OnRemove(void* self, void* param2, int eventType, int eventId) {
    ((fn_OnRemove_t)g_hookOnRemove.trampoline)(self, param2, eventType, eventId);
}

typedef void (*fn_Update_t)(void* self, void* stream, void* p3, int64_t p4);
static void Reimpl_Update(void* self, void* stream, void* p3, int64_t p4) {
    ((fn_Update_t)g_hookUpdate.trampoline)(self, stream, p3, p4);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTradeUIScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140016020);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 15);
#ifdef _DEBUG
    printf("[TradeUI] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTradeUIScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallTradeUISerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140084dd0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TradeUI] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTradeUISerialize() { RemoveHook(&g_hookSerialize); }

void InstallTradeUIDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140082a70);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 13);
#ifdef _DEBUG
    printf("[TradeUI] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveTradeUIDraw() { RemoveHook(&g_hookDraw); }

void InstallTradeUISlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140082a00);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 13);
#ifdef _DEBUG
    printf("[TradeUI] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveTradeUISlot6() { RemoveHook(&g_hookSlot6); }

void InstallTradeUIStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140082150);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 15);
#ifdef _DEBUG
    printf("[TradeUI] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveTradeUIStartup() { RemoveHook(&g_hookStartup); }

void InstallTradeUIOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400827b0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[TradeUI] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveTradeUIOnCreate() { RemoveHook(&g_hookOnCreate); }

void InstallTradeUIOnRemove() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140081f20);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 13);
#ifdef _DEBUG
    printf("[TradeUI] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveTradeUIOnRemove() { RemoveHook(&g_hookOnRemove); }

void InstallTradeUIUpdate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400868c0);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);
#ifdef _DEBUG
    printf("[TradeUI] Update hooked at %p\n", (void*)target);
#endif
}
void RemoveTradeUIUpdate() { RemoveHook(&g_hookUpdate); }
