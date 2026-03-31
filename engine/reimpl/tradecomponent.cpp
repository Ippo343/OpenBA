// engine/reimpl/tradecomponent.cpp
// Hook wrappers for TradeComponent lifecycle methods.
// Delegates Draw and OnCreate to clean reimplementation in
// code/Game/Component/tradecomponent.cpp.
// Shutdown remains a trampoline passthrough (too complex for safe reimpl).
//
// TradeComponent is the largest ControllerComponent variant (0xa8 bytes).
// Three non-trivial vtable overrides:
//   Shutdown (0x14005bd50) — PASSTHROUGH (complex nested serialization)
//   Draw     (0x14005c1d0) — add entity to trade pending list + call setup
//   OnCreate (0x14005c190) — register entity with world trade list

#include "TradeComponent.h"
#include "Component/tradecomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;

// ============================================================
// Original function typedef for Shutdown passthrough
// ============================================================
typedef void (*fn_TradeComponent_Shutdown_t)(TradeComponent* self, void* stream,
                                              uint64_t param3, uint64_t param4);

// ============================================================
// Reimplementation wrappers
// ============================================================

// Shutdown — PASSTHROUGH (too complex: 174 instructions, nested array serialization,
// string serialize, entity refs, Time, version migration)
static void Reimpl_Shutdown(TradeComponent* self, void* stream,
                             uint64_t param3, uint64_t param4) {
    g_lastHook = "TradeComponent::Shutdown";
    ((fn_TradeComponent_Shutdown_t)g_hookShutdown.trampoline)(self, stream, param3, param4);
}

// Draw — delegates to clean code
static void Reimpl_Draw(TradeComponent* self) {
    g_lastHook = "TradeComponent::Draw";
    TradeComponent_Draw(self);
}

// OnCreate — delegates to clean code
static void Reimpl_OnCreate(TradeComponent* self) {
    g_lastHook = "TradeComponent::OnCreate";
    TradeComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTradeComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005bd50);
    // Prologue: PUSH RBP(1)+PUSH RSI(1)+PUSH RDI(1)+MOV RBP,RSP(3)+SUB RSP,0x50(4)+MOV qword,-0x2(8) = 18
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 19);
#ifdef _DEBUG
    printf("[TradeComponent] Shutdown hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveTradeComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTradeComponentDraw() {
    TradeComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005c1d0);
    // Prologue: PUSH RBX(2)+SUB RSP,0x20(4)+MOV RAX,[RCX+0x10](4)+MOV RBX,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 13);
#ifdef _DEBUG
    printf("[TradeComponent] Draw hooked at %p\n", (void*)target);
#endif
}

void RemoveTradeComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallTradeComponentOnCreate() {
    TradeComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005c190);
    // Prologue: SUB RSP,0x28(4)+MOV RAX,[RCX+0x10](4)+LEA RDX,[RSP+0x30](5) = 13
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[TradeComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}

void RemoveTradeComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
