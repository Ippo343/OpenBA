// code/Game/Component/highlightcomponent.cpp
// Clean reimplementation of HighlightComponent::Shutdown from Application-x64.exe.
//
// HighlightComponent (0x128 bytes) manages selection/highlight outlines around entities.
// Embeds Graphics::EdgeMeshInstance at +0x28 (0xD0 bytes) and Math::Transform at +0x100.
//
// Vtable at 0x140159990. Only non-trivial override:
//   Shutdown (0x14004c880) -- Component_Serialize + empty serializer (version 0)
//
// Ghidra decompilation:
//   Component_Serialize(param_1, param_2)
//   Serializer(local, param_2, 0); ~Serializer(local)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/highlightcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class) -- at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

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

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_initialized = true;
}

// ============================================================
// HighlightComponent::Shutdown (vtable slot [3], 0x14004c880)
// ============================================================
// Minimal: Component_Serialize + empty Serializer (version 0).

void HighlightComponent_Shutdown(HighlightComponent* self, void* stream) {
    EnsureHelpers();

    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void HighlightComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[HighlightComponent] Funcs resolved: Serialize=%p\n",
           (void*)s_fn_Component_Serialize);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(HighlightComponent, 0x128, 0x80)
REGISTER_TYPE(HighlightComponent, "Component",
              &HighlightComponent_Factory, &HighlightComponent_CreateInPlace);
#endif // STANDALONE_EXE
