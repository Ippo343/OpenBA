// engine/reimpl/marketlocationcomponent.cpp
// Hook wrappers for MarketLocationComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/marketlocationcomponent.cpp.
//
// MarketLocationComponent is a ControllerComponent subclass (0x48 bytes) with no
// additional fields. Two non-trivial vtable overrides:
//   Shutdown (0x14004efa0) — Component_Serialize + two empty Serializer blocks
//   OnCreate (0x14004ef60) — register entity with game world entity list

#include "MarketLocationComponent.h"
#include "Component/marketlocationcomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookOnCreate;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(MarketLocationComponent* self, void* stream) {
    g_lastHook = "MarketLocationComponent::Shutdown";
    MarketLocationComponent_Shutdown(self, stream);
}

static void Reimpl_OnCreate(MarketLocationComponent* self) {
    g_lastHook = "MarketLocationComponent::OnCreate";
    MarketLocationComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallMarketLocationComponentShutdown() {
    MarketLocationComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14004efa0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x40(4) + MOV RBX,RDX(3) + CALL(5) = 14
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[MarketLocationComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveMarketLocationComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallMarketLocationComponentOnCreate() {
    MarketLocationComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14004ef60);
    // Prologue: SUB RSP,0x28(4) + MOV RAX,[RCX+0x10](4) + LEA RDX,[RSP+0x30](5) = 13
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[MarketLocationComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}

void RemoveMarketLocationComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
