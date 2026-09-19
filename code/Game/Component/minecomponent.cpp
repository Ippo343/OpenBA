// code/Game/Component/minecomponent.cpp
// Clean reimplementation of MineComponent lifecycle methods from Application-x64.exe.
//
// MineComponent (0x28 bytes) — simplest possible component, no additional fields.
// Vtable at 0x140159ed0. Only Shutdown has real logic.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/minecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

static bool s_initialized = false;

// ============================================================
// MineComponent::Shutdown (vtable slot [3], 0x14004f260)
// ============================================================
// Base component serialization + empty serializer block.

void MineComponent_Shutdown(MineComponent* self, void* stream) {
    // Slot-3 serializers are dispatched straight off the vtable by the entity
    // stream, so nothing else ever reaches this file. Resolving here is what
    // keeps the pointers below non-null -- when they were null the whole body
    // read ZERO bytes and silently desynced every object after it.
    MineComponent_InitFuncs();
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void MineComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
// Slot 3 must be wired: retail's body is Component::Serialize plus one empty
// Serializer scope, which is 16 real bytes in the save encoding.
COMPONENT_INERT_FACTORY_SER(MineComponent, 0x28, 0x80, MineComponent_Shutdown)
REGISTER_TYPE(MineComponent, "Component",
              &MineComponent_Factory, &MineComponent_CreateInPlace);
