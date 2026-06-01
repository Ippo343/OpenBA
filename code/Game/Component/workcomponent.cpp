// code/Game/Component/workcomponent.cpp
// Clean reimplementation of WorkComponent lifecycle methods from Application-x64.exe.
//
// WorkComponent (0x38 bytes) is a direct Component subclass managing work ID,
// flags, and state fields. Three non-trivial vtable overrides:
//   Shutdown  (0x14012e710) — versioned serialization of workId, workState, workFlags
//   Draw      (0x14012e6f0) — compute work capacity and store at +0x30
//   OnCreate  (0x14012e6c0) — check if work slot available, register if so
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/workcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(this, int*, name)
typedef void (*fn_Serializer_Serialize_t)(void* self, int* value, const char* name);
static fn_Serializer_Serialize_t s_fn_Serializer_Serialize = nullptr;

// IO::Serializer::GetVersion(this) -> int
typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

// FUN_14012e5c0 — compute work capacity for this component
typedef int (*fn_ComputeWorkCapacity_t)(void* self);
static fn_ComputeWorkCapacity_t s_fn_ComputeWorkCapacity = nullptr;

// FUN_14012ead0 — check if work slot is available (BST lookup)
typedef bool (*fn_CheckWorkSlot_t)(void* self);
static fn_CheckWorkSlot_t s_fn_CheckWorkSlot = nullptr;

// FUN_14012eb50 — register work slot
typedef void (*fn_RegisterWorkSlot_t)(void* self);
static fn_RegisterWorkSlot_t s_fn_RegisterWorkSlot = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serializer_Serialize = *reinterpret_cast<fn_Serializer_Serialize_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    s_fn_ComputeWorkCapacity = reinterpret_cast<fn_ComputeWorkCapacity_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));

    s_fn_CheckWorkSlot = reinterpret_cast<fn_CheckWorkSlot_t>(
        ResolveAddr(Module::Exe, 0x14012ead0));

    s_fn_RegisterWorkSlot = reinterpret_cast<fn_RegisterWorkSlot_t>(
        ResolveAddr(Module::Exe, 0x14012eb50));

    s_initialized = true;
}

// ============================================================
// WorkComponent::Shutdown (vtable slot [3], 0x14012e710)
// ============================================================
// Versioned serialization:
//   - Always: serialize workId (+0x28)
//   - Version < 3: serialize a legacy bool (discarded)
//   - Version >= 1: serialize workState (+0x30)
//   - Version >= 2: serialize workFlags (+0x2c)

void WorkComponent_Shutdown(WorkComponent* self, void* stream) {
    EnsureHelpers();

    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    uint8_t serializerBuf[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializerBuf, stream, 3);
    }

    // Always serialize workId
    if (s_fn_Serializer_Serialize) {
        s_fn_Serializer_Serialize(serializerBuf, &self->workId, nullptr);
    }

    // Legacy: version < 3 had a bool field (no longer used)
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializerBuf);
        if (version < 3) {
            bool legacyBool = false;
            s_fn_Serializer_Serialize(serializerBuf, (int*)&legacyBool, nullptr);
        }
    }

    // Version >= 1: serialize workState
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializerBuf);
        if (version >= 1) {
            s_fn_Serializer_Serialize(serializerBuf, &self->workState, nullptr);
        }
    }

    // Version >= 2: serialize workFlags
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializerBuf);
        if (version >= 2) {
            s_fn_Serializer_Serialize(serializerBuf, (int*)&self->workFlags, nullptr);
        }
    }

    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializerBuf);
    }
}

// ============================================================
// WorkComponent::Draw (vtable slot [6], 0x14012e6f0)
// ============================================================
// Compute the effective work capacity and store it at +0x30.

void WorkComponent_Draw(WorkComponent* self) {
    EnsureHelpers();

    if (s_fn_ComputeWorkCapacity) {
        int capacity = s_fn_ComputeWorkCapacity(self);
        self->workState = capacity;
    }
}

// ============================================================
// WorkComponent::OnCreate (vtable slot [7], 0x14012e6c0)
// ============================================================
// Check if work slot is available (via BST lookup using workId).
// If available, register it with the work slot manager.

void WorkComponent_OnCreate(WorkComponent* self) {
    EnsureHelpers();

    if (!self) return;

    if (s_fn_CheckWorkSlot && s_fn_RegisterWorkSlot) {
        bool available = s_fn_CheckWorkSlot(self);
        if (available) {
            s_fn_RegisterWorkSlot(self);
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void WorkComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[WorkComponent] Funcs resolved: Component_Serialize=%p Serializer_Ctor=%p "
           "ComputeWorkCapacity=%p CheckWorkSlot=%p RegisterWorkSlot=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_ComputeWorkCapacity, (void*)s_fn_CheckWorkSlot,
           (void*)s_fn_RegisterWorkSlot);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0x38 (WorkComponent_Factory MemAlloc), flags 0x83 (Ctor 0x14012e820:
// Component base sets +0x20=0x80, leaf OR 0x3 => 0x83).
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(WorkComponent, 0x38, 0x83)
REGISTER_TYPE(WorkComponent, "Component",
              &WorkComponent_Factory, &WorkComponent_CreateInPlace);
#endif // STANDALONE_EXE
