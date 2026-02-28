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
#include <cstdio>
#include <cstdint>

// ============================================================
// Shutdown (0x1400449d0) — passthrough
// 40-instruction function. Serializes Component base + isEnabled bool at +0x28 +
// int array (count at +0x38, data at +0x30):
//   1. Calls Component_Serialize
//   2. Creates Serializer, serializes isEnabled
//   3. Reads count from +0x38, serializes it
//   4. On load: resizes array at +0x30 via FUN_1400021d0
//   5. Bulk-serializes int array elements
// Uses IO::Serializer chain — passthrough.
// ============================================================
typedef void (*AutoPickupShutdown_t)(void* self, void* stream);
static HookInfo g_hookShutdown;

static void Reimpl_Shutdown(void* self, void* stream) {
    ((AutoPickupShutdown_t)g_hookShutdown.trampoline)(self, stream);
}

void InstallAutoPickupComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400449d0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[AutoPickupComponent] Shutdown hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveAutoPickupComponentShutdown() { RemoveHook(&g_hookShutdown); }

// ============================================================
// Serialize (0x140044990) — passthrough
// 17-instruction function. Checks IsDerivedFrom on +0x18 with type DAT_1401c3fe8,
// reads byte at +0x20 from the derived object and stores it in isEnabled (+0x28).
// Initializes the "enabled" flag based on the entity's type configuration.
// ============================================================
typedef void (*AutoPickupSerialize_t)(void* self);
static HookInfo g_hookSerialize;

static void Reimpl_Serialize(void* self) {
    ((AutoPickupSerialize_t)g_hookSerialize.trampoline)(self);
}

void InstallAutoPickupComponentSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140044990);
    // Prologue: MOV [RSP+8],RBX(5) + PUSH RDI(1) + SUB RSP,0x20(4) + MOV RBX,[RCX+0x18](4) = 14
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[AutoPickupComponent] Serialize hooked at %p (passthrough, prologue=14)\n", (void*)target);
#endif
}
void RemoveAutoPickupComponentSerialize() { RemoveHook(&g_hookSerialize); }
