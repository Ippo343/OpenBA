// code/Game/Component/fishingcomponent.cpp
// Clean reimplementation of FishingComponent lifecycle methods from Application-x64.exe.
//
// FishingComponent (0x58 bytes) extends ResourceProduceComponent with:
//   +0x50: uint8  fishingFlag  (serialized as bool)
//   +0x54: float  fishingRate  (serialized)
//
// Vtable at 0x140163230. One non-trivial hookable override:
//   Shutdown (0x14011f050) — ResourceProduceComponent::Shutdown + own serializer
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/fishingcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// ResourceProduceComponent::Shutdown (base) at 0x14012b4a0
typedef void (*fn_ResourceProduceShutdown_t)(void* self, void* stream);
static fn_ResourceProduceShutdown_t s_fn_ResourceProduceShutdown = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(this, bool*, name)
typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t s_fn_Serializer_SerializeBool = nullptr;

// IO::Serializer::Serialize(this, float*, name)
typedef void (*fn_Serializer_SerializeFloat_t)(void* self, float* value, const char* name);
static fn_Serializer_SerializeFloat_t s_fn_Serializer_SerializeFloat = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_ResourceProduceShutdown = reinterpret_cast<fn_ResourceProduceShutdown_t>(
        ResolveAddr(Module::Exe, 0x14012b4a0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_Serializer_SerializeBool = *reinterpret_cast<fn_Serializer_SerializeBool_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a60);
    if (slot) s_fn_Serializer_SerializeFloat = *reinterpret_cast<fn_Serializer_SerializeFloat_t*>(slot);

    s_initialized = true;
}

// ============================================================
// FishingComponent::Shutdown (vtable slot [3], 0x14011f050)
// ============================================================
// ResourceProduceComponent::Shutdown(self, stream)
// Serializer s(stream, 0)
// s.Serialize(&self[0x50], null)  // bool: fishingFlag
// s.Serialize(&self[0x54], null)  // float: fishingRate
// ~Serializer()

void FishingComponent_Shutdown(FishingComponent* self, void* stream) {
    EnsureHelpers();

    // Call base ResourceProduceComponent::Shutdown
    if (s_fn_ResourceProduceShutdown) {
        s_fn_ResourceProduceShutdown(self, stream);
    }

    // Serialize FishingComponent's own fields
    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
    }

    if (s_fn_Serializer_SerializeBool) {
        bool* pFishingFlag = reinterpret_cast<bool*>(&self->fishingFlag);
        s_fn_Serializer_SerializeBool(serializer, pFishingFlag, nullptr);
    }

    if (s_fn_Serializer_SerializeFloat) {
        s_fn_Serializer_SerializeFloat(serializer, &self->fishingRate, nullptr);
    }

    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void FishingComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[FishingComponent] Funcs resolved: ResourceProduceShutdown=%p "
           "Serializer_Ctor=%p SerializeBool=%p SerializeFloat=%p\n",
           (void*)s_fn_ResourceProduceShutdown, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_SerializeBool, (void*)s_fn_Serializer_SerializeFloat);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid FishingComponent
// (size 0x58, +0x20 capability flags 0x80, Object::Internal base + shared stub
// vtable, zeroed body). Size from FishingComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(FishingComponent, 0x58, 0x80)
REGISTER_TYPE(FishingComponent, "ResourceProduceComponent",
              &FishingComponent_Factory, &FishingComponent_CreateInPlace);
