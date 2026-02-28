// engine/reimpl/orchardcomponent.cpp
// Reimplementation of OrchardComponent lifecycle methods.
//
// OrchardComponent (0x68 bytes) extends ControllerComponent with:
//   +0x48: uint8  orchardFlag  (zeroed)
//   +0x4C: int32  orchardId    (initialized to -1)
//   +0x50: void*  field50      (zeroed — entity ref array)
//   +0x58: void*  field58      (zeroed — entity ref data)
//   +0x60: void*  field60      (zeroed — entity ref array for resources)
//
// Vtable at 0x14015a180 (12 slots):
//   +0x00 Dtor           = 0x140009a70 (too small to hook)
//   +0x08 GetDescription = 0x140009860 (7 bytes, unhookable)
//   +0x10 Startup        = 0x140009850 (7 bytes, unhookable)
//   +0x18 Shutdown       = 0x140051de0 (HOOKED — passthrough, complex entity ref serialization)
//   +0x20 Serialize      = 0x140051dd0 (9-byte MOV+RET, unhookable — zeroes field60)
//   +0x28 Slot5          = 0x140118a60 (no-op)
//   +0x30 Draw           = 0x140118820 (no-op)
//   +0x38 OnCreate       = 0x140118830 (no-op)
//   +0x40 OnRemove       = 0x140118a70 (no-op)
//   +0x48 Update         = 0x140118a80 (no-op)
//   +0x50 Slot10         = 0x140118a90 (no-op)
//   +0x58 OnUpdate2      = 0x140118aa0 (no-op)
//
// Shutdown is complex: Component_Serialize + empty serializer + second serializer
// that validates entity references (FUN_140051830), serializes orchardFlag/orchardId,
// and manages entity ref arrays at +0x50/+0x58/+0x60.
// Hooked as passthrough due to many internal helpers.

#include "OrchardComponent.h"
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
// Shutdown (0x140051de0) — passthrough
// Complex entity reference serialization with validation, array
// management, and conditional resizing. Same pattern as CropFieldComponent.
//
// Prologue: PUSH RDI(1) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 14 → 15 bytes
// ============================================================

typedef void (*fn_Shutdown_Orig_t)(OrchardComponent* self, void* stream);

static void Reimpl_Shutdown(OrchardComponent* self, void* stream) {
    ((fn_Shutdown_Orig_t)g_hookShutdown.trampoline)(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallOrchardComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140051de0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[OrchardComponent] Shutdown hooked (passthrough) at %p\n", (void*)target);
#endif
}

void RemoveOrchardComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}
