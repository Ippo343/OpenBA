// code/Game/Component/clearareacomponent.cpp
// Clean reimplementation of ClearAreaComponent lifecycle methods.
//
// ClearAreaComponent (0x50 bytes) -- ControllerComponent subclass.
// Has one extra field: clearState (uint32) at +0x48.
// Handles clearing terrain areas during construction/placement.
//
// Vtable at 0x1401626d0. Non-trivial override:
//   Shutdown (0x140117EC0) -- serialize with clearState
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/clearareacomponent_funcs.h"
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

typedef void (*fn_Serialize_Int_t)(void* serializer, int* value, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;

static bool s_initialized = false;

// ============================================================
// ClearAreaComponent::Shutdown (vtable slot [3], 0x140117EC0)
// ============================================================
// Calls Component_Serialize, then:
//   1. Empty serializer block (version 0) -- format compat stub
//   2. Serializer block with clearState at +0x48
void ClearAreaComponent_Shutdown(ClearAreaComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // Empty serializer block (format compatibility stub)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer1, stream, 0);
        s_fn_Serializer_Dtor(serializer1);
    }

    // Serialize clearState
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor && s_fn_Serialize_Int && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer2, stream, 0);
        s_fn_Serialize_Int(serializer2, (int*)&self->clearState, nullptr);
        s_fn_Serializer_Dtor(serializer2);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ClearAreaComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    s_initialized = true;

#ifdef _DEBUG
    printf("[ClearAreaComponent] Funcs resolved: CompSerialize=%p Ctor=%p Dtor=%p SerInt=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_Dtor, (void*)s_fn_Serialize_Int);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid ClearAreaComponent
// (size 0x50, +0x20 capability flags 0x80, Object::Internal base + shared stub
// vtable, zeroed body). Size from ClearAreaComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(ClearAreaComponent, 0x50, 0x80)
REGISTER_TYPE(ClearAreaComponent, "ControllerComponent",
              &ClearAreaComponent_Factory, &ClearAreaComponent_CreateInPlace);
#endif // STANDALONE_EXE
