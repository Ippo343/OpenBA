// engine/reimpl/boatcomponent.cpp
// Reimplementation of BoatComponent lifecycle methods.
//
// BoatComponent (0x98 bytes) — Component with ControllerComponent pattern.
// Secondary vtable sub-object at +0x78. Manages fishing boat state:
// entity refs, position vectors, route waypoints, dock ID.
//
// Vtable at 0x140158f40:
//   [3] Shutdown = 0x1400465d0 (passthrough — 95-instruction complex serialization)
//   All other slots are base no-ops (OnCreate = 0x140118830 no-op).
//
// Note: Task mentions GetDescription at 0x1400469c0, but vtable slot [1]
// is 0x1400036d0 (GetTypeInfo — returns DAT_1401c4028). The 0x1400469c0
// function is a separate method (possibly a Startup variant that creates
// a serializer version block and returns 1). Not in the vtable.

#include "BoatComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Component/boatcomponent_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;

// ============================================================
// Shutdown (0x1400465d0) -- delegates to clean code
// ============================================================
static void Reimpl_Shutdown(BoatComponent* self, void* stream) {
    g_lastHook = "BoatComponent::Shutdown";
    BoatComponent_Shutdown(self, stream);
}

void InstallBoatComponentShutdown() {
    BoatComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400465d0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[BoatComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveBoatComponentShutdown() { RemoveHook(&g_hookShutdown); }
