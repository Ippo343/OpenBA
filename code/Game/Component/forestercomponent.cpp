// code/Game/Component/forestercomponent.cpp
// Clean reimplementation of ForesterComponent lifecycle methods from Application-x64.exe.
//
// ForesterComponent (0x58 bytes) extends ResourceProduceComponent with:
//   +0x50: uint16 foresterFlags (two packed bytes, each serialized as bool)
//
// Vtable at 0x140163460. One non-trivial hookable override:
//   Shutdown (0x14011f960) — ResourceProduceComponent::Shutdown + own serializer
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/forestercomponent_funcs.h"
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

    s_initialized = true;
}

// ============================================================
// ForesterComponent::Shutdown (vtable slot [3], 0x14011f960)
// ============================================================
// ResourceProduceComponent::Shutdown(self, stream)
// Serializer s(stream, 0)
// s.Serialize(&self[0x50], null)  // bool at +0x50 (foresterFlags low byte)
// s.Serialize(&self[0x51], null)  // bool at +0x51 (foresterFlags high byte)
// ~Serializer()

void ForesterComponent_Shutdown(ForesterComponent* self, void* stream) {
    EnsureHelpers();

    // Call base ResourceProduceComponent::Shutdown
    if (s_fn_ResourceProduceShutdown) {
        s_fn_ResourceProduceShutdown(self, stream);
    }

    // Serialize ForesterComponent's own fields
    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // foresterFlags at +0x50 is uint16, serialized as two individual bools
    if (s_fn_Serializer_SerializeBool) {
        bool* flagLow  = reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(self) + 0x50);
        bool* flagHigh = reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(self) + 0x51);
        s_fn_Serializer_SerializeBool(serializer, flagLow, nullptr);
        s_fn_Serializer_SerializeBool(serializer, flagHigh, nullptr);
    }

    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ForesterComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[ForesterComponent] Funcs resolved: ResourceProduceShutdown=%p "
           "Serializer_Ctor=%p SerializeBool=%p\n",
           (void*)s_fn_ResourceProduceShutdown, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_SerializeBool);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(ForesterComponent, 0x58, 0x80)
REGISTER_TYPE(ForesterComponent, "ResourceProduceComponent",
              &ForesterComponent_Factory, &ForesterComponent_CreateInPlace);
#endif // STANDALONE_EXE
