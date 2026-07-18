// code/Game/Component/minecomponent.cpp
// Clean reimplementation of MineComponent lifecycle methods from Application-x64.exe.
//
// MineComponent (0x28 bytes) — simplest possible component, no additional fields.
// Vtable at 0x140159ed0. Only Shutdown has real logic.
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/minecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

static bool s_initialized = false;

// ============================================================
// MineComponent::Shutdown (vtable slot [3], 0x14004f260)
// ============================================================
// Base component serialization + empty serializer block.

void MineComponent_Shutdown(MineComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor)
        s_fn_Serializer_Ctor(serializer, stream, 0);
    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void MineComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(MineComponent, 0x28, 0x80)
REGISTER_TYPE(MineComponent, "Component",
              &MineComponent_Factory, &MineComponent_CreateInPlace);
