// code/Game/Component/wellcomponent.cpp
// Clean reimplementation of WellComponent lifecycle methods from Application-x64.exe.
//
// WellComponent (0x48 bytes) extends ControllerComponent. No additional fields.
// Vtable at 0x14015ae90. Two non-trivial overrides:
//   Shutdown  (0x14005cfa0) — Two empty serializer blocks
//   OnCreate  (0x14005cf60) — Registers entity with population manager
//
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/wellcomponent_funcs.h"
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

// FUN_140091ba0 — adds entity to dynamic array (water source registration)
typedef void (*fn_ArrayPushEntity_t)(void* arrayBase, void* internalBasePtr);
static fn_ArrayPushEntity_t s_fn_ArrayPushEntity = nullptr;

// Game world/population manager singleton
static void** s_pWorldManager = nullptr;

static bool s_initialized = false;

// ============================================================
// WellComponent::Shutdown (vtable slot [3], 0x14005cfa0)
// ============================================================
// Component_Serialize + two empty serializer blocks (controller compat).

void WellComponent_Shutdown(WellComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // First empty serializer block (component section)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor)
        s_fn_Serializer_Ctor(serializer1, stream, 0);
    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer1);

    // Second empty serializer block (controller section)
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor)
        s_fn_Serializer_Ctor(serializer2, stream, 0);
    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer2);
}

// ============================================================
// WellComponent::OnCreate (vtable slot [7], 0x14005cf60)
// ============================================================
// Registers the owning entity with the world manager's water source list.

void WellComponent_OnCreate(WellComponent* self) {
    // Create InternalBasePtr on stack from owner entity
    uint8_t basePtr[24];
    if (fn_InternalBasePtr_Ctor)
        fn_InternalBasePtr_Ctor(basePtr, self->owner);

    // Push to world manager's water source array at +0xF0
    if (s_pWorldManager && *s_pWorldManager && s_fn_ArrayPushEntity) {
        void* manager = *s_pWorldManager;
        void* waterArray = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(manager) + 0xF0);
        s_fn_ArrayPushEntity(waterArray, basePtr);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void WellComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    s_fn_ArrayPushEntity = reinterpret_cast<fn_ArrayPushEntity_t>(
        ResolveAddr(Module::Exe, 0x140091ba0));
    s_pWorldManager = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(WellComponent, 0x48, 0x82)
REGISTER_TYPE(WellComponent, "ControllerComponent",
              &WellComponent_Factory, &WellComponent_CreateInPlace);
#endif // STANDALONE_EXE
