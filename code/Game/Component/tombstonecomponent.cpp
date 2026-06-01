// code/Game/Component/tombstonecomponent.cpp
// Clean reimplementation of TombstoneComponent lifecycle methods from Application-x64.exe.
//
// TombstoneComponent (0x30 bytes) — manages tombstone lifetime.
// Fields: +0x28 = Time (int64, countdown timer).
//
// Vtable at 0x14015a980. Three non-trivial overrides:
//   Shutdown  (0x1400578E0) — Serialize with version halving
//   Serialize (0x140057860) — Actually OnCreate: init random lifetime
//   Update    (0x1400577D0) — Timer countdown + entity removal
//
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/tombstonecomponent_funcs.h"
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

typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

// System::Time::Serialize(this, serializer)
typedef void (*fn_Time_Serialize_t)(void* time, void* serializer);
static fn_Time_Serialize_t s_fn_Time_Serialize = nullptr;

// Base::IsDerivedFrom — dispatched through [0x140152a98]
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* self, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// System::Random::RandFloat(random)
typedef float (*fn_RandFloat_t)(void* random);
static fn_RandFloat_t s_fn_RandFloat = nullptr;

// InternalBasePtr::CheckPtr
typedef void (*fn_CheckPtr_t)(void* ptr);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// FUN_1400c2ea0 — mark entity for removal
typedef void (*fn_MarkForRemoval_t)(void* entity);
static fn_MarkForRemoval_t s_fn_MarkForRemoval = nullptr;

// FUN_140003110 — dynamic array push
typedef void (*fn_ArrayPush_t)(void* arrayBase, int newCount, void* elementPtr);
static fn_ArrayPush_t s_fn_ArrayPush = nullptr;

// Global: seconds per tick multiplier (DAT_140172234)
static float* s_pSecondsPerTickMul = nullptr;

// Game state singleton + tombstone type ID
static void** s_pGameState = nullptr;
static uint32_t* s_pTombstoneTypeId = nullptr;

static bool s_initialized = false;

// ============================================================
// TombstoneComponent::Shutdown (0x1400578E0)
// ============================================================
void TombstoneComponent_Shutdown(TombstoneComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor)
        s_fn_Serializer_Ctor(serializer, stream, 2);

    int64_t* pTime = (int64_t*)&self->field28;
    if (s_fn_Time_Serialize)
        s_fn_Time_Serialize(pTime, serializer);

    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializer);
        if (version < 2) {
            *pTime = *pTime / 2;
        }
    }

    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// TombstoneComponent::Serialize (0x140057860) — actually OnCreate
// ============================================================
void TombstoneComponent_Serialize(TombstoneComponent* self) {
    void* baseObj = self->_unk18;

    // IsDerivedFrom check for tombstone data type
    void* buildingData = nullptr;
    if (s_fn_IsDerivedFrom_d && s_pTombstoneTypeId) {
        bool isDerived = s_fn_IsDerivedFrom_d(baseObj, *s_pTombstoneTypeId);
        if (isDerived) buildingData = baseObj;
    }

    if (!buildingData || !s_pGameState || !*s_pGameState) return;

    // Random float in [0, 1)
    void* rng = *(void**)((uintptr_t)*s_pGameState + 0xD8);
    float randVal = 0.0f;
    if (s_fn_RandFloat)
        randVal = s_fn_RandFloat(rng);

    // Compute lifetime: (randVal * max + min) * tickMultiplier
    float minTime = *(float*)((uintptr_t)buildingData + 0x20);
    float maxTime = *(float*)((uintptr_t)buildingData + 0x24);
    float seconds = (randVal * maxTime + minTime) * *s_pSecondsPerTickMul;

    // Construct Time from seconds, store at +0x28
    int64_t timeVal;
    if (fn_Time_Ctor_Float)
        fn_Time_Ctor_Float(&timeVal, seconds);
    *(int64_t*)&self->field28 = timeVal;
}

// ============================================================
// TombstoneComponent::Update (0x1400577D0)
// ============================================================
void TombstoneComponent_Update(TombstoneComponent* self, int64_t* pDeltaTime) {
    int64_t* pTimer = (int64_t*)&self->field28;
    *pTimer -= *pDeltaTime;

    if (*pTimer < 1) {
        void* entityPtr = self->owner;
        if (!s_pGameState || !*s_pGameState) return;
        void* gameWorld = *(void**)((uintptr_t)*s_pGameState + 0xE8);

        if (entityPtr != nullptr) {
            if (s_fn_CheckPtr)
                s_fn_CheckPtr((void*)((uintptr_t)self + 0x10));

            // Check bit 10 of entity flags at +0x40
            uint32_t entityFlags = *(uint32_t*)((uintptr_t)entityPtr + 0x40);
            if (((entityFlags >> 10) & 1) == 0) {
                if (s_fn_CheckPtr)
                    s_fn_CheckPtr((void*)((uintptr_t)self + 0x10));
                if (s_fn_MarkForRemoval)
                    s_fn_MarkForRemoval(entityPtr);

                // Add to removal queue
                if (s_fn_ArrayPush && gameWorld) {
                    int count = *(int*)((uintptr_t)gameWorld + 0xD8);
                    s_fn_ArrayPush((void*)((uintptr_t)gameWorld + 0xD0), count + 1,
                                   (void*)((uintptr_t)self + 0x10));
                }
            }
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void TombstoneComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_MarkForRemoval = reinterpret_cast<fn_MarkForRemoval_t>(
        ResolveAddr(Module::Exe, 0x1400c2ea0));
    s_fn_ArrayPush = reinterpret_cast<fn_ArrayPush_t>(
        ResolveAddr(Module::Exe, 0x140003110));
    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_pTombstoneTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4a68));
    s_pSecondsPerTickMul = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172234));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152aa8);
    if (slot) s_fn_Time_Serialize = *reinterpret_cast<fn_Time_Serialize_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x1401512f8);
    if (slot) s_fn_RandFloat = *reinterpret_cast<fn_RandFloat_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ab0);
    if (slot) s_fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(slot);

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(TombstoneComponent, 0x30, 0x88)
REGISTER_TYPE(TombstoneComponent, "Component",
              &TombstoneComponent_Factory, &TombstoneComponent_CreateInPlace);
#endif // STANDALONE_EXE
