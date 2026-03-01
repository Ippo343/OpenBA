// engine/reimpl/cropfieldcomponent.cpp
// Reimplementation of CropFieldComponent lifecycle methods.
//
// CropFieldComponent (0x68 bytes) extends ControllerComponent with:
//   +0x48: uint16 cropFlags (initialized to 0x100)
//   +0x50: void*  field50   (zeroed — entity ref array pointer)
//   +0x58: void*  field58   (zeroed — entity ref array data)
//   +0x60: void*  field60   (zeroed — entity ref array count)
//
// Vtable at 0x140162ec0 (12 slots):
//   +0x00 Dtor           = 0x1400330a0 (too small to hook)
//   +0x08 GetDescription = 0x140032ec0 (7 bytes, unhookable)
//   +0x10 Startup        = 0x140032eb0 (7 bytes, unhookable)
//   +0x18 Shutdown       = 0x14011ae70 (HOOKED — passthrough, complex entity ref serialization)
//   +0x20 Serialize      = 0x14011ae60 (9-byte MOV+RET, unhookable — zeroes field50)
//   +0x28 Slot5          = 0x140118a60 (no-op)
//   +0x30 Draw           = 0x140118820 (no-op)
//   +0x38 OnCreate       = 0x140118830 (no-op)
//   +0x40 OnRemove       = 0x140118a70 (no-op)
//   +0x48 Update         = 0x140118a80 (no-op)
//   +0x50 Slot10         = 0x140118a90 (no-op)
//   +0x58 OnUpdate2      = 0x140118aa0 (no-op)
//
// Shutdown is complex: Component_Serialize + empty serializer + second serializer block
// that validates entity references (FUN_14011aa20), serializes cropFlags as 2 bools,
// serializes an InternalBasePtr array at +0x50, and a counted entity list at +0x58/+0x60.
// Too many internal helpers to cleanly reimplement — hooked as passthrough.

#include "CropFieldComponent.h"
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
// Shutdown (0x14011ae70) — passthrough
// Complex entity reference serialization with validation, array
// management, and conditional resizing. Uses 6+ helper functions.
//
// Prologue: PUSH RDI(1) + SUB RSP,0x50(4) + MOV [RSP+0x20],-2(9) = 14 → 15 bytes
// ============================================================

typedef void (*fn_Shutdown_Orig_t)(CropFieldComponent* self, void* stream);

static void Reimpl_Shutdown(CropFieldComponent* self, void* stream) {
    g_lastHook = "CropFieldComponent::Shutdown";
    ((fn_Shutdown_Orig_t)g_hookShutdown.trampoline)(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallCropFieldComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14011ae70);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[CropFieldComponent] Shutdown hooked (passthrough) at %p\n", (void*)target);
#endif
}

void RemoveCropFieldComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}
