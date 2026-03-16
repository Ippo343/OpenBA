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
    g_lastHook = "TradeUI::ScalarDtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, TradeUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    uintptr_t s = (uintptr_t)self;
    fn_MemFreeStatic(*(void**)(s + 0x128));
    *(uint64_t*)(s + 0x128) = 0;
    *(uint64_t*)(s + 0x130) = 0;
    fn_MemFreeStatic(*(void**)(s + 0xB0));
    *(uint64_t*)(s + 0xB0) = 0;
    *(uint64_t*)(s + 0xB8) = 0;
    fn_MemFreeStatic(*(void**)(s + 0xA0));
    *(uint64_t*)(s + 0xA0) = 0;
    *(uint64_t*)(s + 0xA8) = 0;
    fn_MemFreeStatic(*(void**)(s + 0x90));
    *(uint64_t*)(s + 0x90) = 0;
    *(uint64_t*)(s + 0x98) = 0;
    fn_MemFreeStatic(*(void**)(s + 0x80));
    *(uint64_t*)(s + 0x80) = 0;
    *(uint64_t*)(s + 0x88) = 0;
    fn_MemFreeStatic(*(void**)(s + 0x70));
    *(uint64_t*)(s + 0x70) = 0;
    *(uint64_t*)(s + 0x78) = 0;
    fn_MemFreeStatic(*(void**)(s + 0x60));
    *(uint64_t*)(s + 0x60) = 0;
    *(uint64_t*)(s + 0x68) = 0;
    fn_MemFreeStatic(*(void**)(s + 0x50));
    *(uint64_t*)(s + 0x50) = 0;
    *(uint64_t*)(s + 0x58) = 0;
    fn_MemFreeStatic(*(void**)(s + 0x40));
    *(uint64_t*)(s + 0x40) = 0;
    *(uint64_t*)(s + 0x48) = 0;
    fn_MemFreeStatic(*(void**)(s + 0x30));
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Serialize: REIMPLEMENTED
static void Reimpl_Serialize(void* self) {
    g_lastHook = "TradeUI::Serialize";
    TradeUI_Serialize((TradeUI*)self);
}

// Draw: REIMPLEMENTED
static void Reimpl_Draw(void* self) {
    g_lastHook = "TradeUI::Draw";
    TradeUI_Draw((TradeUI*)self);
}

// Slot6: REIMPLEMENTED
static void Reimpl_Slot6(void* self) {
    g_lastHook = "TradeUI::Slot6";
    TradeUI_Slot6((TradeUI*)self);
}

// Startup: REIMPLEMENTED
static void Reimpl_Startup(void* self) {
    g_lastHook = "TradeUI::Startup";
    TradeUI_Startup((TradeUI*)self);
}

// OnCreate: REIMPLEMENTED
static void Reimpl_OnCreate(void* self) {
    g_lastHook = "TradeUI::OnCreate";
    TradeUI_OnCreate((TradeUI*)self);
}

// OnRemove: REIMPLEMENTED
typedef void (*fn_OnRemove_t)(void* self, void* param2, int eventType, int eventId);
static void Reimpl_OnRemove(void* self, void* param2, int eventType, int eventId) {
    g_lastHook = "TradeUI::OnRemove";
    TradeUI_OnRemove((TradeUI*)self, param2, eventType, eventId);
}

// Update: REIMPLEMENTED
typedef void (*fn_Update_t)(void* self, void* stream, void* p3, int64_t p4);
static void Reimpl_Update(void* self, void* stream, void* p3, int64_t p4) {
    g_lastHook = "TradeUI::Update";
    TradeUI_Update((TradeUI*)self, stream, p3, (int64_t)p4);
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
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 18);
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
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 16);
#ifdef _DEBUG
    printf("[TradeUI] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveTradeUISlot6() { RemoveHook(&g_hookSlot6); }

void InstallTradeUIStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140082150);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 17);
#ifdef _DEBUG
    printf("[TradeUI] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveTradeUIStartup() { RemoveHook(&g_hookStartup); }

void InstallTradeUIOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400827b0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 14);
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
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 17);
#ifdef _DEBUG
    printf("[TradeUI] Update hooked at %p\n", (void*)target);
#endif
}
void RemoveTradeUIUpdate() { RemoveHook(&g_hookUpdate); }
