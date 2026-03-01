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
#include <cstdio>
#include <cstdint>

// ============================================================
// Shutdown (0x1400465d0) — passthrough
// 95-instruction function. Very complex serialization:
//   1. Component_Serialize base
//   2. Version block (serializer version 0)
//   3. InternalBasePtr at +0x28 (entity ref, validated with IsDerivedFrom ComponentEntity)
//   4. Vector3 at +0x40 (position)
//   5. Float at +0x4c
//   6. InternalBasePtr at +0x30 (validated with DAT_1401c7a50 type)
//   7. InternalBasePtr at +0x38 (validated with DAT_1401c7078 type)
//   8. Vector3 at +0x50 (direction/target)
//   9. Int at +0x70 (dock ID, -1 = none)
//  10. Vector3 array at +0x60 (count at +0x68) — route waypoints
//      Loop serializes each Vector3 (12 bytes each)
// Far too complex for reimpl — heavy engine serialization deps.
// ============================================================
typedef void (*BoatShutdown_t)(void* self, void* stream);
static HookInfo g_hookShutdown;

static void Reimpl_Shutdown(void* self, void* stream) {
    g_lastHook = "BoatComponent::Shutdown";
    ((BoatShutdown_t)g_hookShutdown.trampoline)(self, stream);
}

void InstallBoatComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400465d0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[BoatComponent] Shutdown hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveBoatComponentShutdown() { RemoveHook(&g_hookShutdown); }
