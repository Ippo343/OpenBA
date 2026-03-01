// engine/reimpl/minecomponent.cpp
// Reimplementation of MineComponent lifecycle methods.
//
// MineComponent is one of the simplest components (0x28 bytes, same as base Component).
// No additional fields beyond the Component base.
//
// Vtable at 0x140159ed0 (12 slots):
//   +0x00 Dtor           = 0x140008fa0 (too small to hook)
//   +0x08 GetDescription = 0x140008dc0 (7 bytes, unhookable)
//   +0x10 Startup        = 0x140008db0 (7 bytes, unhookable)
//   +0x18 Shutdown       = 0x14004f260 (HOOKED — serialize state)
//   +0x20 Serialize      = 0x140118a50 (no-op)
//   +0x28 Slot5          = 0x140118a60 (no-op)
//   +0x30 Draw           = 0x14004f250 (3-byte RET, unhookable)
//   +0x38 OnCreate       = 0x14004f240 (3-byte RET, unhookable)
//   +0x40 OnRemove       = 0x140118a70 (no-op)
//   +0x48 Update         = 0x140118a80 (no-op)
//   +0x50 Slot10         = 0x140118a90 (no-op)
//   +0x58 OnUpdate2      = 0x140118aa0 (no-op)
//
// Only Shutdown has real logic and is large enough to hook.
// Shutdown pattern:
//   1. Call Component_Serialize(this, stream)
//   2. Create IO::Serializer (empty version block, no fields serialized)
//   3. Destroy IO::Serializer

#include "MineComponent.h"
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
// Local function pointers (resolved at install time)
// ============================================================

// Component_Serialize(this, stream) — base component serialization
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t fn_Component_Serialize = nullptr;

// IO::Serializer::Serializer(this, stream, version)
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer(this)
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// ============================================================
// Shutdown (0x14004f260)
// Serialize component state. MineComponent has no own fields,
// so just calls Component_Serialize and creates an empty Serializer block.
//
// Prologue: PUSH RBX; SUB RSP,0x30; MOV RBX,RDX; CALL Component_Serialize
// Safe prologue size: 13 bytes (boundary at CALL instruction start)
// ============================================================

static void Reimpl_Shutdown(MineComponent* self, void* stream) {
    g_lastHook = "MineComponent::Shutdown";
    // Base component serialization (owner ref, flags, etc.)
    fn_Component_Serialize(self, stream);

    // Empty serializer version block — no component-specific fields
    uint8_t serializer[16];
    fn_Serializer_Ctor(serializer, stream, 0);
    fn_Serializer_Dtor(serializer);
}

// ============================================================
// Install / Remove
// ============================================================

static bool ResolveSerializerFunctions() {
    // Component_Serialize is a direct function in the exe
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    // IO::Serializer ctor/dtor are called through .rdata dispatch pointers.
    // The pointers at 0x140152ad8 / 0x140152ad0 contain runtime addresses after
    // the exe's init fills the dispatch table. We read through them at install time.
    uintptr_t pCtorSlot = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t pDtorSlot = ResolveAddr(Module::Exe, 0x140152ad0);
    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(pCtorSlot));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(pDtorSlot));

    return fn_Component_Serialize && fn_Serializer_Ctor && fn_Serializer_Dtor;
}

void InstallMineComponentShutdown() {
    if (!ResolveSerializerFunctions()) {
#ifdef _DEBUG
        printf("[MineComponent] ERROR: Failed to resolve serializer functions\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004f260);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[MineComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveMineComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}
