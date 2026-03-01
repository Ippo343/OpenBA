// engine/reimpl/quarrycomponent.cpp
// Reimplementation of QuarryComponent lifecycle methods.
//
// QuarryComponent is one of the simplest components (0x28 bytes, same as base Component).
// No additional fields beyond the Component base. Identical Shutdown pattern to MineComponent.
//
// Vtable at 0x14015a490 (12 slots):
//   +0x00 Dtor           = 0x14000b5d0 (too small to hook)
//   +0x08 GetDescription = 0x14000b3c0 (7 bytes, unhookable)
//   +0x10 Startup        = 0x14000b3b0 (7 bytes, unhookable)
//   +0x18 Shutdown       = 0x140055230 (HOOKED — serialize state)
//   +0x20 Serialize      = 0x140118a50 (no-op)
//   +0x28 Slot5          = 0x140118a60 (no-op)
//   +0x30 Draw           = 0x140055220 (3-byte RET, unhookable)
//   +0x38 OnCreate       = 0x140055210 (3-byte RET, unhookable)
//   +0x40 OnRemove       = 0x140118a70 (no-op)
//   +0x48 Update         = 0x140118a80 (no-op)
//   +0x50 Slot10         = 0x140118a90 (no-op)
//   +0x58 OnUpdate2      = 0x140118aa0 (no-op)
//
// Only Shutdown has real logic and is large enough to hook.

#include "QuarryComponent.h"
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
// Local function pointers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// ============================================================
// Shutdown (0x140055230)
// Same pattern as MineComponent: Component_Serialize + empty Serializer block.
//
// Prologue: PUSH RBX; SUB RSP,0x30; MOV RBX,RDX; CALL ...
// Safe prologue size: 13 bytes
// ============================================================

static void Reimpl_Shutdown(QuarryComponent* self, void* stream) {
    g_lastHook = "QuarryComponent::Shutdown";
    fn_Component_Serialize(self, stream);

    uint8_t serializer[16];
    fn_Serializer_Ctor(serializer, stream, 0);
    fn_Serializer_Dtor(serializer);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallQuarryComponentShutdown() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad8)));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad0)));

    if (!fn_Component_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor) {
#ifdef _DEBUG
        printf("[QuarryComponent] ERROR: Failed to resolve serializer functions\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140055230);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[QuarryComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveQuarryComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}
