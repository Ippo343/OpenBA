// code/Game/Component/absorbresourcecomponent.cpp
// Clean reimplementation of AbsorbResourceComponent lifecycle methods.
//
// AbsorbResourceComponent (0x48 bytes) is a ControllerComponent subclass with no
// additional fields. Only one non-trivial vtable override:
//   Shutdown (0x140105c00) -- Component_Serialize + two empty Serializer blocks
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/absorbresourcecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class Shutdown) at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer::Serializer(this, stream, version) -- dispatch slot at 0x140152ad8
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer(this) -- dispatch slot at 0x140152ad0
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

static bool s_initialized = false;

// ============================================================
// AbsorbResourceComponent::Shutdown (vtable slot [3], 0x140105c00)
// ============================================================
// Pattern: Component_Serialize + two empty Serializer ctor/dtor pairs.
// Same pattern as MarketLocationComponent and StorageLocationComponent.
void AbsorbResourceComponent_Shutdown(AbsorbResourceComponent* self, void* stream) {
    // Call base Component_Serialize
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // First empty serializer block (version 0)
    uint8_t serializerBuf1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializerBuf1, stream, 0);
        s_fn_Serializer_Dtor(serializerBuf1);
    }

    // Second empty serializer block (version 0)
    uint8_t serializerBuf2[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializerBuf2, stream, 0);
        s_fn_Serializer_Dtor(serializerBuf2);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void AbsorbResourceComponent_InitFuncs() {
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
    printf("[AbsorbResourceComponent] Funcs resolved: Serialize=%p Ctor=%p Dtor=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_Dtor);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid AbsorbResourceComponent
// (size 0x48, +0x20 capability flags 0x80, Object::Internal base + shared stub
// vtable, zeroed body). Size from AbsorbResourceComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(AbsorbResourceComponent, 0x48, 0x80)
REGISTER_TYPE(AbsorbResourceComponent, "ControllerComponent",
              &AbsorbResourceComponent_Factory, &AbsorbResourceComponent_CreateInPlace);
#endif // STANDALONE_EXE
