// code/Game/Component/pickingcomponent.cpp
// Clean reimplementation of PickingComponent lifecycle methods from Application-x64.exe.
//
// PickingComponent (0x38 bytes) — manages 3D picking (mouse -> entity selection).
// Fields: +0x28=pickingId (int32, -1=unregistered), +0x30=entity ref (IBP)
//
// Vtable at 0x140164040. Two hookable overrides:
//   Shutdown (0x140128bb0) — Serialize IBP + IsDerivedFrom validation
//   OnCreate (0x140128b50) — Remove picking mesh via Scene dispatch
//
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/pickingcomponent_funcs.h"
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

// Object::InternalBasePtr::Serialize
typedef void (*fn_IBP_Serialize_t)(void* ibp, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;

// IsDerivedFrom
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// Picking::Scene::RemoveMesh — dispatched through [0x140152438]
typedef void (*fn_RemoveMesh_t)(void* scene, uint32_t meshId);
static fn_RemoveMesh_t s_fn_RemoveMesh = nullptr;

static uint32_t* s_pComponentEntityTypeId = nullptr;

static bool s_initialized = false;

// ============================================================
// PickingComponent::Shutdown (0x140128bb0)
// ============================================================
void PickingComponent_Shutdown(PickingComponent* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize IBP at +0x30
    if (s_fn_IBP_Serialize)
        s_fn_IBP_Serialize((void*)(selfAddr + 0x30), serializer);

    // Validate: if not ComponentEntity, null it out
    void* ibpVal = *(void**)(selfAddr + 0x30);
    if (ibpVal != nullptr && s_fn_IsDerivedFrom_d && s_pComponentEntityTypeId) {
        if (!s_fn_IsDerivedFrom_d(ibpVal, *s_pComponentEntityTypeId)) {
            *(uintptr_t*)(selfAddr + 0x30) = 0;
        }
    }

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// PickingComponent::OnCreate (0x140128b50)
// ============================================================
// If pickingId >= 0, calls Picking::Scene::RemoveMesh and resets to -1.

void PickingComponent_OnCreate(PickingComponent* self) {
    if ((int)self->pickingId >= 0) {
        if (s_fn_RemoveMesh && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            void* pickingScene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x70);
            s_fn_RemoveMesh(pickingScene, (uint32_t)self->pickingId);
        }
        self->pickingId = -1;
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void PickingComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_pComponentEntityTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c72e8));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    // Picking::Scene::RemoveMesh — dispatch slot at 0x140152438
    slot = ResolveAddr(Module::Exe, 0x140152438);
    if (slot) s_fn_RemoveMesh = *reinterpret_cast<fn_RemoveMesh_t*>(slot);

    s_fn_IBP_Serialize = reinterpret_cast<fn_IBP_Serialize_t>(
        ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z"));

    s_initialized = true;
}
