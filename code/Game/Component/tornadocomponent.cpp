// code/Game/Component/tornadocomponent.cpp
// Clean reimplementation of TornadoComponent lifecycle methods from Application-x64.exe.
//
// TornadoComponent (0x68 bytes) manages tornado disasters.
// Fields: +0x28=Time, +0x30=Vector3 position, +0x38=count, +0x40=path array,
//         +0x48=pathCount, +0x50=tornadoId, +0x58=Vector3 direction, +0x60=field
//
// Vtable at 0x14015aa80. Three non-trivial overrides:
//   Shutdown  (0x140058b80) — Serialize position, tornadoId, path, time, direction
//   Serialize (0x1400585a0) — Full tornado init (236 instructions)
//   OnRemove  (0x140057cd0) — Tornado movement/damage (210 instructions)
//
// Serialize and OnRemove are the most complex component methods in the entire game.
// They interact with pathfinding, audio, random number generation, entity destruction,
// and rendering. We call the original exe functions for these two methods.
//
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/tornadocomponent_funcs.h"
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

typedef void (*fn_SerializeInt_t)(void* self, int* value, const char* name);
static fn_SerializeInt_t s_fn_SerializeInt = nullptr;

typedef void (*fn_Vector3_Serialize_t)(void* vec3, void* serializer);
static fn_Vector3_Serialize_t s_fn_Vector3_Serialize = nullptr;

typedef void (*fn_Time_Serialize_t)(void* time, void* serializer);
static fn_Time_Serialize_t s_fn_Time_Serialize = nullptr;

// FUN_14000d030 — resize Vector3 array
typedef void (*fn_ResizeVec3Array_t)(void* arrayPtr, uint32_t newCount);
static fn_ResizeVec3Array_t s_fn_ResizeVec3Array = nullptr;

// Original exe functions for the two complex methods
typedef void (*fn_Serialize_Orig_t)(void* self);
static fn_Serialize_Orig_t s_fn_Serialize_Orig = nullptr;

typedef void (*fn_OnRemove_Orig_t)(void* self, void* time);
static fn_OnRemove_Orig_t s_fn_OnRemove_Orig = nullptr;

static bool s_initialized = false;

// ============================================================
// TornadoComponent::Shutdown (0x140058b80)
// ============================================================
// Serialize: Vector3 at +0x30, int at +0x50, variable-length Vector3 array
// at +0x40/+0x48, Time at +0x28, Vector3 at +0x58.

void TornadoComponent_Shutdown(TornadoComponent* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize position Vector3 at +0x30
    if (s_fn_Vector3_Serialize)
        s_fn_Vector3_Serialize((void*)(selfAddr + 0x30), serializer);

    // Serialize tornadoId at +0x50
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer, (int*)(selfAddr + 0x50), nullptr);

    // Serialize path waypoint count from +0x48
    int localCount = *(int*)(selfAddr + 0x48);
    int pathCount = localCount;
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer, &pathCount, nullptr);

    // If loading, resize path array
    if (*(int*)((uintptr_t)stream + 8) == 0 && s_fn_ResizeVec3Array) {
        s_fn_ResizeVec3Array((void*)(selfAddr + 0x40), (uint32_t)pathCount);
    }

    // Serialize each path waypoint Vector3
    void* pathArray = *(void**)(selfAddr + 0x40);
    if (pathCount > 0 && s_fn_Vector3_Serialize) {
        uint8_t* pVec = (uint8_t*)pathArray;
        for (int i = 0; i < pathCount; i++) {
            s_fn_Vector3_Serialize(pVec, serializer);
            pVec += 0xC;  // sizeof(Vector3) = 12
        }
    }

    // Serialize Time at +0x28
    if (s_fn_Time_Serialize)
        s_fn_Time_Serialize((void*)(selfAddr + 0x28), serializer);

    // Serialize direction Vector3 at +0x58
    if (s_fn_Vector3_Serialize)
        s_fn_Vector3_Serialize((void*)(selfAddr + 0x58), serializer);

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// TornadoComponent::Serialize (0x1400585a0)
// ============================================================
// Full tornado initialization: random endpoints, A* path generation,
// notification message creation. 236 instructions, deeply coupled to
// Random, pathfinding, String system, notification UI.
// Calls original exe function.

void TornadoComponent_Serialize(TornadoComponent* self) {
    if (s_fn_Serialize_Orig)
        s_fn_Serialize_Orig(self);
}

// ============================================================
// TornadoComponent::OnRemove (0x140057cd0)
// ============================================================
// Tornado movement, pathfinding, audio emitter updates, entity damage,
// building interaction. 210 instructions, deeply coupled to Physics,
// Audio::Scene, entity system.
// Calls original exe function.

void TornadoComponent_OnRemove(TornadoComponent* self, void* time) {
    if (s_fn_OnRemove_Orig)
        s_fn_OnRemove_Orig(self, time);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void TornadoComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_ResizeVec3Array = reinterpret_cast<fn_ResizeVec3Array_t>(
        ResolveAddr(Module::Exe, 0x14000d030));

    // Original complex functions
    s_fn_Serialize_Orig = reinterpret_cast<fn_Serialize_Orig_t>(
        ResolveAddr(Module::Exe, 0x1400585a0));
    s_fn_OnRemove_Orig = reinterpret_cast<fn_OnRemove_Orig_t>(
        ResolveAddr(Module::Exe, 0x140057cd0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_SerializeInt = *reinterpret_cast<fn_SerializeInt_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152aa8);
    if (slot) s_fn_Time_Serialize = *reinterpret_cast<fn_Time_Serialize_t*>(slot);

    s_fn_Vector3_Serialize = reinterpret_cast<fn_Vector3_Serialize_t>(
        ResolveExport("?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z"));

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(TornadoComponent, 0x68, 0x84)
REGISTER_TYPE(TornadoComponent, "Component",
              &TornadoComponent_Factory, &TornadoComponent_CreateInPlace);
