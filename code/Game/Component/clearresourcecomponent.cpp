// code/Game/Component/clearresourcecomponent.cpp
// Clean reimplementation of ClearResourceComponent lifecycle methods.
//
// ClearResourceComponent (0x48 bytes) -- ControllerComponent subclass with no extra fields.
// Handles clearing resources from tiles during construction.
//
// Vtable at 0x140162840. Non-trivial override:
//   Shutdown (0x1401185E0) -- Component_Serialize + two empty serializer blocks
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/clearresourcecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

static bool s_initialized = false;

// ============================================================
// ClearResourceComponent::Shutdown (vtable slot [3], 0x1401185E0)
// ============================================================
// Same pattern as AbsorbResourceComponent: Component_Serialize + two empty blocks.
void ClearResourceComponent_Shutdown(ClearResourceComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // First empty serializer block (version 0)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer1, stream, 0);
        s_fn_Serializer_Dtor(serializer1);
    }

    // Second empty serializer block (version 0)
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer2, stream, 0);
        s_fn_Serializer_Dtor(serializer2);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ClearResourceComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    s_initialized = true;

#ifdef _DEBUG
    printf("[ClearResourceComponent] Funcs resolved: CompSerialize=%p Ctor=%p Dtor=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_Dtor);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(ClearResourceComponent, 0x48, 0x80)
REGISTER_TYPE(ClearResourceComponent, "ControllerComponent",
              &ClearResourceComponent_Factory, &ClearResourceComponent_CreateInPlace);
#endif // STANDALONE_EXE
