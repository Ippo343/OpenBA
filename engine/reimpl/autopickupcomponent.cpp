// engine/reimpl/autopickupcomponent.cpp
// Reimplementation of AutoPickupComponent lifecycle methods.
//
// AutoPickupComponent (0x40 bytes) — direct Component subclass.
// Auto-collects dropped resources near buildings.
// Has an "enabled" byte at +0x28, and an int array at +0x30 (ptr) / +0x38 (count).
//
// Vtable at 0x140158df0:
//   [3] Shutdown  = 0x1400449d0 (passthrough — serialization with bool + int array)
//   [4] Serialize = 0x140044990 (passthrough — IsDerivedFrom check to init isEnabled)
//   All other slots are base no-ops.

#include "AutoPickupComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Component/autopickupcomponent_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookSerialize;

// ============================================================
// Reimpl wrappers -- delegate to clean code
// ============================================================
static void Reimpl_Shutdown(AutoPickupComponent* self, void* stream) {
    g_lastHook = "AutoPickupComponent::Shutdown";
    AutoPickupComponent_Shutdown(self, stream);
}

static void Reimpl_Serialize(AutoPickupComponent* self) {
    g_lastHook = "AutoPickupComponent::Serialize";
    AutoPickupComponent_Serialize(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallAutoPickupComponentShutdown() {
    AutoPickupComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400449d0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[AutoPickupComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveAutoPickupComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallAutoPickupComponentSerialize() {
    AutoPickupComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140044990);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV RBX,[RCX+0x18](4) = 14
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[AutoPickupComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveAutoPickupComponentSerialize() { RemoveHook(&g_hookSerialize); }
