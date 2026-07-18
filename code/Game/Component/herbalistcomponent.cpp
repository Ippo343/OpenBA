// code/Game/Component/herbalistcomponent.cpp
// Clean reimplementation of HerbalistComponent::Shutdown from Application-x64.exe.
//
// HerbalistComponent (0x50 bytes) is a ResourceProduceComponent subclass that
// manages herbalist buildings. No additional fields beyond ResourceProduceComponent.
//
// Vtable at 0x140159860. Only non-trivial override:
//   Shutdown (0x14004b200) -- calls parent Shutdown + empty serializer block.
//
// Ghidra decompilation:
//   ResourceProduceComponent_Controller_Slot13(param_1, param_2)
//   Serializer(local, param_2, 0); ~Serializer(local)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/herbalistcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// ResourceProduceComponent::Shutdown (parent) -- at 0x14012b4a0
// This already calls Component_Serialize + empty controller block + serialize produceFlag.
typedef void (*fn_ResourceProduceComponent_Shutdown_t)(void* self, void* stream);
static fn_ResourceProduceComponent_Shutdown_t s_fn_ParentShutdown = nullptr;

// IO::Serializer::Serializer(stream, version) -- dispatch slot at 0x140152ad8
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer() -- dispatch slot at 0x140152ad0
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    uintptr_t slot;

    s_fn_ParentShutdown = reinterpret_cast<fn_ResourceProduceComponent_Shutdown_t>(
        ResolveAddr(Module::Exe, 0x14012b4a0));

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_initialized = true;
}

// ============================================================
// HerbalistComponent::Shutdown (vtable slot [3], 0x14004b200)
// ============================================================
// Calls parent ResourceProduceComponent::Shutdown, then creates
// an empty serializer block (no additional data to serialize).

void HerbalistComponent_Shutdown(HerbalistComponent* self, void* stream) {
    EnsureHelpers();

    // Call parent ResourceProduceComponent::Shutdown
    if (s_fn_ParentShutdown) {
        s_fn_ParentShutdown(self, stream);
    }

    // Empty serializer block (subclass compat)
    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void HerbalistComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[HerbalistComponent] Funcs resolved: ParentShutdown=%p\n",
           (void*)s_fn_ParentShutdown);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid HerbalistComponent
// (size 0x50, +0x20 capability flags 0x80, Object::Internal base + shared stub
// vtable, zeroed body). Size from HerbalistComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(HerbalistComponent, 0x50, 0x80)
REGISTER_TYPE(HerbalistComponent, "ResourceProduceComponent",
              &HerbalistComponent_Factory, &HerbalistComponent_CreateInPlace);
