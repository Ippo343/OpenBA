// code/Game/Component/absorbresourcecomponent.cpp
// Clean reimplementation of AbsorbResourceComponent lifecycle methods.
//
// AbsorbResourceComponent (0x48 bytes) is a ControllerComponent subclass with no
// additional fields. Only one non-trivial vtable override:
//   Shutdown (0x140105c00) -- Component_Serialize + two empty Serializer blocks
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/absorbresourcecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Game/serialize_externals.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class Shutdown) at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

static bool s_initialized = false;

// ============================================================
// AbsorbResourceComponent::Shutdown (vtable slot [3], 0x140105c00)
// ============================================================
// Pattern: Component_Serialize + two empty Serializer ctor/dtor pairs.
// Same pattern as MarketLocationComponent and StorageLocationComponent.
void AbsorbResourceComponent_Shutdown(AbsorbResourceComponent* self, void* stream) {
    // Slot-3 serializers are dispatched straight off the vtable by the entity
    // stream, so nothing else ever reaches this file. Resolving here is what
    // keeps the pointers below non-null -- when they were null the whole body
    // read ZERO bytes and silently desynced every object after it.
    AbsorbResourceComponent_InitFuncs();
    // Call base Component_Serialize
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // First empty serializer block (version 0)
    alignas(8) uint8_t serializerBuf1[24];
    ::new (static_cast<void*>(serializerBuf1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializerBuf1)->~Serializer();                                       // B.3.4 bridge

    // Second empty serializer block (version 0)
    alignas(8) uint8_t serializerBuf2[24];
    ::new (static_cast<void*>(serializerBuf2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializerBuf2)->~Serializer();                                       // B.3.4 bridge
}

// Component::Persist (0x1401189b0) and ExternalPtr<Description>::Serialize
// (0x140031d10), whose ported bodies lived here, are in component.cpp
// (ENDGAME B.3.8 K1); their registered doors keep the old names.

// ============================================================
// Init: resolve function pointers
// ============================================================
void AbsorbResourceComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    s_initialized = true;

#ifdef _DEBUG
    printf("[AbsorbResourceComponent] Funcs resolved: Serialize=%p\n",
           (void*)s_fn_Component_Serialize);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid AbsorbResourceComponent
// (size 0x48, +0x20 capability flags 0x80, Object::Internal base + shared stub
// vtable, zeroed body). Size from AbsorbResourceComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// Slot 3 must be wired: retail 0x140105c00 is Component::Serialize plus TWO
// empty Serializer scopes, and an empty scope still occupies 16 bytes in the
// save encoding — the base-only fill read 32 bytes too few and desynced the
// stream for every object after it.
COMPONENT_INERT_FACTORY_SER(AbsorbResourceComponent, 0x48, 0x80,
                            AbsorbResourceComponent_Shutdown)
REGISTER_TYPE(AbsorbResourceComponent, "ControllerComponent",
              &AbsorbResourceComponent_Factory, &AbsorbResourceComponent_CreateInPlace);
