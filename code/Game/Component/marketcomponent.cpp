// code/Game/Component/marketcomponent.cpp
// Clean reimplementation of MarketComponent lifecycle methods from Application-x64.exe.
//
// MarketComponent is a ResourceProduceComponent subclass (0x50 bytes) with no
// additional fields beyond the parent. Only one non-trivial vtable override:
//   Shutdown (0x14004ead0) — calls ResourceProduceComponent_Shutdown + empty Serializer block
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/marketcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// ResourceProduceComponent_Shutdown (parent) at 0x14012b4a0
typedef void (*fn_ParentShutdown_t)(void* self, void* stream);
static fn_ParentShutdown_t s_fn_ParentShutdown = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_ParentShutdown = reinterpret_cast<fn_ParentShutdown_t>(
        ResolveAddr(Module::Exe, 0x14012b4a0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    s_initialized = true;
}

// ============================================================
// MarketComponent::Shutdown (vtable slot [3], 0x14004ead0)
// ============================================================
// Calls parent ResourceProduceComponent_Shutdown, then creates/destroys
// an empty Serializer block (version 0). This empty block exists for
// binary format compatibility (versioned sub-section with no fields).
//
// Original disassembly:
//   CALL ResourceProduceComponent_Shutdown (0x14012b4a0)
//   CALL Serializer_Ctor(local, stream, 0)
//   CALL Serializer_Dtor(local)

void MarketComponent_Shutdown(MarketComponent* self, void* stream) {
    EnsureHelpers();

    // Call parent ResourceProduceComponent::Shutdown
    if (s_fn_ParentShutdown) {
        s_fn_ParentShutdown(self, stream);
    }

    // Empty serializer block (version 0)
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        uint8_t serializerBuf[24];
        s_fn_Serializer_Ctor(serializerBuf, stream, 0);
        s_fn_Serializer_Dtor(serializerBuf);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void MarketComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[MarketComponent] Funcs resolved: ParentShutdown=%p Ctor=%p Dtor=%p\n",
           (void*)s_fn_ParentShutdown, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_Dtor);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Inert-but-valid MarketComponent: size 0x50 + Object::Internal base + shared stub
// vtable + capability flags +0x20=0x80 (from MarketComponent_Ctor) + zeroed body.
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(MarketComponent, 0x50, 0x80)
REGISTER_TYPE(MarketComponent, "ResourceProduceComponent",
              &MarketComponent_Factory, &MarketComponent_CreateInPlace);
#endif // STANDALONE_EXE
