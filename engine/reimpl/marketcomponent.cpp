// engine/reimpl/marketcomponent.cpp
// Hook wrappers for MarketComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/marketcomponent.cpp.
//
// MarketComponent is a ResourceProduceComponent subclass (0x50 bytes) with no
// additional fields. Only one non-trivial vtable override:
//   Shutdown (0x14004ead0) — calls parent Shutdown + empty Serializer block

#include "MarketComponent.h"
#include "Component/marketcomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(MarketComponent* self, void* stream) {
    g_lastHook = "MarketComponent::Shutdown";
    MarketComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallMarketComponentShutdown() {
    MarketComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14004ead0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x30(4) + MOV RBX,RDX(3) + CALL(5) = 14
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[MarketComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveMarketComponentShutdown() { RemoveHook(&g_hookShutdown); }
