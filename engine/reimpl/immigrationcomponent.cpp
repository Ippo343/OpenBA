// engine/reimpl/immigrationcomponent.cpp
// Reimplementation of ImmigrationComponent lifecycle methods.
//
// ImmigrationComponent (0x68 bytes) — ControllerComponent subclass.
// Manages immigration/nomad arrival events.
//
// Vtable at 0x140159b30:
//   [3] Shutdown = 0x14004e4b0 (passthrough — complex serialization with entity array loop)
//   All other slots are base no-ops (OnCreate = 0x140118830 no-op).

#include "ImmigrationComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Shutdown (0x14004e4b0) — passthrough
// Serializes Component base + version block + field50 (int) + field60 (array count)
// + entity array at +0x58 with IsDerivedFrom validation per element +
// Time::Serialize at +0x48. Complex 68-instruction function.
// ============================================================
typedef void (*ImmigrationShutdown_t)(void* self, void* stream);
static HookInfo g_hookShutdown;

static void Reimpl_Shutdown(void* self, void* stream) {
    ((ImmigrationShutdown_t)g_hookShutdown.trampoline)(self, stream);
}

void InstallImmigrationComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14004e4b0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ImmigrationComponent] Shutdown hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveImmigrationComponentShutdown() { RemoveHook(&g_hookShutdown); }
