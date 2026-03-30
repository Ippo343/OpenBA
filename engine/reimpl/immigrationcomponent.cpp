// engine/reimpl/immigrationcomponent.cpp
// Reimplementation of ImmigrationComponent lifecycle methods.
//
// ImmigrationComponent (0x68 bytes) — ControllerComponent subclass.
// Manages immigration/nomad arrival events.
//
// Vtable at 0x140159b30:
//   [3] Shutdown = 0x14004e4b0 (complex serialization with entity array loop)

#include "ImmigrationComponent.h"
#include "Component/immigrationcomponent_funcs.h"
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
// Shutdown (0x14004e4b0)
// ============================================================
static void Reimpl_Shutdown(ImmigrationComponent* self, void* stream) {
    g_lastHook = "ImmigrationComponent::Shutdown";
    ImmigrationComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallImmigrationComponentShutdown() {
    ImmigrationComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004e4b0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ImmigrationComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveImmigrationComponentShutdown() { RemoveHook(&g_hookShutdown); }
