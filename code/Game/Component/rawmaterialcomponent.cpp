// code/Game/Component/rawmaterialcomponent.cpp
// Clean reimplementation of RawMaterialComponent lifecycle methods from Application-x64.exe.
//
// RawMaterialComponent (0x40 bytes) manages raw material lifecycle: spawning,
// decay timing, and collection marking.
//
// Vtable at 0x140164170. Four overrides:
//   Shutdown  (0x140129330) — versioned serialization of field28, field38, field30
//   Serialize (0x1401292c0) — init field28 from building data, register with world
//   Draw      (0x1401291d0) — create pickup render object and register with scene
//   Update    (0x140129130) — accumulate time, check threshold, mark for collection
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/rawmaterialcomponent_funcs.h"
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

// IO::Serializer ctor/dtor
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;
typedef void (*fn_Serialize_Int_t)(void* self, int* value, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;
typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

// System::Time::Serialize(this, serializer)
typedef void (*fn_Time_Serialize_t)(void* timePtr, void* serializer);
static fn_Time_Serialize_t s_fn_Time_Serialize = nullptr;

// FUN_140018020 — serialize field30 (version >= 2)
typedef void (*fn_SerializeField30_t)(void* fieldPtr, void* serializer);
static fn_SerializeField30_t s_fn_SerializeField30 = nullptr;

// Object::InternalBasePtr::CheckPtr via dispatch slot 0x140152ab0
typedef void (*fn_CheckPtr_t)(void* ptr);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// FUN_140091e60 — register raw material with world manager
typedef void (*fn_RegisterRawMaterial_t)(void* worldMgr, void* entityChain);
static fn_RegisterRawMaterial_t s_fn_RegisterRawMaterial = nullptr;

// FUN_1400c2ea0 — mark entity for collection
typedef void (*fn_MarkForCollection_t)(void* entity);
static fn_MarkForCollection_t s_fn_MarkForCollection = nullptr;

// FUN_140003110 — add entity to world pending list
typedef void (*fn_AddToPending_t)(void* listPtr, int newCount, void* entityPtr);
static fn_AddToPending_t s_fn_AddToPending = nullptr;

// System::Time::Time(out, float) — construct Time from float
typedef void* (*fn_Time_FromFloat_t)(void* outTime, float seconds);
static fn_Time_FromFloat_t s_fn_Time_FromFloat = nullptr;

// DAT_14017225c — threshold time constant (float)
static float* s_pThresholdTime = nullptr;

// DAT_1401c3dd8 — game state singleton
static void** s_pGameState = nullptr;

// FUN_140003050 — FindComponent (MaterialDescription type DWORD_1401c8070)
typedef void* (*fn_FindComponent_MatDesc_t)(uintptr_t entity, void* retval);
static fn_FindComponent_MatDesc_t s_fn_FindComponent_MatDesc = nullptr;

// RawMaterialComponent_helper_9480 — determine pickup category from material flags
typedef uint64_t (*fn_GetPickupCategory_t)(uintptr_t rawMatDesc);
static fn_GetPickupCategory_t s_fn_GetPickupCategory = nullptr;

// Use fn_MemAlloc + g_pMemAllocSingleton from engine_funcs.h

// Work_Ctor25 (0x1400d3cc0) — construct pickup work item base
typedef void (*fn_WorkCtor25_t)(void* self, void* entityRef);
static fn_WorkCtor25_t s_fn_WorkCtor25 = nullptr;

// vtable_Pickup — pickup draw item vtable at 0x14015e430
static void** s_vtable_Pickup = nullptr;

// fn_InternalBasePtr_Ctor — already in engine_funcs.h
// fn_IsDerivedFrom — already in engine_funcs.h

// CitizenAI_impl_ec60 — register draw item with scene
typedef void (*fn_RegisterDrawItem_t)(uintptr_t entityRef, uint32_t category, char param3, void* itemRef);
static fn_RegisterDrawItem_t s_fn_RegisterDrawItem = nullptr;

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
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    // System::Time::Serialize — via export or dispatch slot
    s_fn_Time_Serialize = reinterpret_cast<fn_Time_Serialize_t>(
        ResolveExport("?Serialize@Time@System@@QEAAXAEAVSerializer@IO@@@Z"));
    if (!s_fn_Time_Serialize) {
        slot = ResolveAddr(Module::Exe, 0x140152b08);
        if (slot) s_fn_Time_Serialize = *reinterpret_cast<fn_Time_Serialize_t*>(slot);
    }

    s_fn_SerializeField30 = reinterpret_cast<fn_SerializeField30_t>(
        ResolveAddr(Module::Exe, 0x140018020));

    slot = ResolveAddr(Module::Exe, 0x140152ab0);
    if (slot) s_fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(slot);

    s_fn_RegisterRawMaterial = reinterpret_cast<fn_RegisterRawMaterial_t>(
        ResolveAddr(Module::Exe, 0x140091e60));

    s_fn_MarkForCollection = reinterpret_cast<fn_MarkForCollection_t>(
        ResolveAddr(Module::Exe, 0x1400c2ea0));

    s_fn_AddToPending = reinterpret_cast<fn_AddToPending_t>(
        ResolveAddr(Module::Exe, 0x140003110));

    slot = ResolveAddr(Module::Exe, 0x140152b00);
    if (slot) s_fn_Time_FromFloat = *reinterpret_cast<fn_Time_FromFloat_t*>(slot);

    s_pThresholdTime = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x14017225c));

    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    // FindComponent_MatDesc — FUN_140003050 (finds by type DWORD_1401c8070)
    s_fn_FindComponent_MatDesc = reinterpret_cast<fn_FindComponent_MatDesc_t>(
        ResolveAddr(Module::Exe, 0x140003050));

    s_fn_GetPickupCategory = reinterpret_cast<fn_GetPickupCategory_t>(
        ResolveAddr(Module::Exe, 0x140129480));

    // fn_MemAlloc + g_pMemAllocSingleton resolved globally in engine_funcs

    s_fn_WorkCtor25 = reinterpret_cast<fn_WorkCtor25_t>(
        ResolveAddr(Module::Exe, 0x1400d3cc0));

    s_vtable_Pickup = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x14015e430));

    s_fn_RegisterDrawItem = reinterpret_cast<fn_RegisterDrawItem_t>(
        ResolveAddr(Module::Exe, 0x14009ec60));

    s_initialized = true;
}

// ============================================================
// RawMaterialComponent::Shutdown (vtable slot [3], 0x140129330)
// ============================================================
// Versioned serialization (version 2):
//   - field28 (resource type ID, int)
//   - field38 (Time accumulator)
//   - field30 (version >= 2 only)

void RawMaterialComponent_Shutdown(RawMaterialComponent* self, void* stream) {
    EnsureHelpers();

    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    uint8_t serializerBuf[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializerBuf, stream, 2);
    }

    // Serialize field28 (resource type ID)
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serializerBuf, (int*)&self->field28, nullptr);
    }

    // Serialize field38 (Time accumulator)
    if (s_fn_Time_Serialize) {
        s_fn_Time_Serialize(&self->field38, serializerBuf);
    }

    // Version >= 2: serialize field30
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializerBuf);
        if (version >= 2 && s_fn_SerializeField30) {
            s_fn_SerializeField30(&self->field30, serializerBuf);
        }
    }

    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializerBuf);
    }
}

// ============================================================
// RawMaterialComponent::Serialize (vtable slot [4], 0x1401292c0)
// ============================================================
// Init field28 from building data type at +0x40, then register
// the entity with the world raw material manager.

void RawMaterialComponent_Serialize(RawMaterialComponent* self) {
    EnsureHelpers();

    // Get building data via IsDerivedFrom check on _unk18
    void* baseObj = self->_unk18;
    void* buildingData = nullptr;
    if (fn_IsDerivedFrom && g_TypeId_RawMaterialDescription) {
        bool isDerived = fn_IsDerivedFrom(baseObj, *g_TypeId_RawMaterialDescription);
        if (isDerived) {
            buildingData = baseObj;
        }
    }

    // Copy resource type from building data +0x40 to field28
    if (buildingData) {
        *(uint32_t*)&self->field28 = *(uint32_t*)((uintptr_t)buildingData + 0x40);
    }

    // CheckPtr on owner at +0x10
    if (s_fn_CheckPtr) {
        s_fn_CheckPtr((void*)((uintptr_t)self + 0x10));
    }

    // Get entity chain from owner+0x28, register with world manager
    if (s_pGameState && *s_pGameState && s_fn_RegisterRawMaterial) {
        uintptr_t gameState = (uintptr_t)*s_pGameState;
        uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);
        void* entityChain = *(void**)(*(uintptr_t*)((uintptr_t)self + 0x10) + 0x28);
        s_fn_RegisterRawMaterial((void*)worldMgr, entityChain);
    }
}

// ============================================================
// RawMaterialComponent::Draw (vtable slot [6], 0x1401291d0)
// ============================================================
// Creates a pickup render object and registers it with the scene.
//
// Original logic:
//   1. CheckPtr on entity ref (+0x10)
//   2. FindComponent_MatDesc on entity — find MaterialDescription component
//   3. IsDerivedFrom check on _unk18 for RawMaterialDescription
//   4. GetPickupCategory — determine visual category from material flags
//   5. MemAlloc 0x60 bytes for pickup draw item
//   6. WorkCtor25 to initialize base
//   7. Set vtable to vtable_Pickup, zero fields at +0x48, +0x50, +0x58
//   8. InternalBasePtr_Ctor to wrap the item
//   9. CheckPtr on MaterialDescription ref
//  10. RegisterDrawItem to add to scene

void RawMaterialComponent_Draw(RawMaterialComponent* self) {
    EnsureHelpers();

    uintptr_t selfAddr = (uintptr_t)self;

    // CheckPtr on entity ref
    if (s_fn_CheckPtr) {
        s_fn_CheckPtr((void*)(selfAddr + 0x10));
    }

    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);

    // FindComponent_MatDesc on entity
    uintptr_t matDescBuf[2] = {0, 0};
    if (s_fn_FindComponent_MatDesc) {
        s_fn_FindComponent_MatDesc(entity, (void*)matDescBuf);
    }

    // Get RawMaterialDescription via IsDerivedFrom
    void* baseObj = self->_unk18;
    void* rawMatDesc = nullptr;
    if (fn_IsDerivedFrom && g_TypeId_RawMaterialDescription) {
        if (fn_IsDerivedFrom(baseObj, *g_TypeId_RawMaterialDescription)) {
            rawMatDesc = baseObj;
        }
    }

    if (!rawMatDesc) return;

    // Get pickup category from material flags
    uint64_t category = 1;
    if (s_fn_GetPickupCategory) {
        category = s_fn_GetPickupCategory((uintptr_t)rawMatDesc);
    }

    // Allocate pickup draw item (0x60 bytes)
    void* pickupItem = nullptr;
    if (fn_MemAlloc && g_pMemAllocSingleton) {
        pickupItem = fn_MemAlloc(g_pMemAllocSingleton, 0x60, 8);
    }

    void* pickupItemResult = nullptr;
    if (pickupItem) {
        // Construct base with entity reference
        uintptr_t entityRef = entity;
        uintptr_t entityRefBuf[2];
        entityRefBuf[0] = entityRef;
        if (s_fn_WorkCtor25) {
            s_fn_WorkCtor25(pickupItem, (void*)entityRefBuf);
        }

        // Set pickup vtable and zero extra fields
        if (s_vtable_Pickup) {
            *(void**)pickupItem = s_vtable_Pickup;
        }
        *(uint32_t*)((uintptr_t)pickupItem + 0x48) = 0;
        *(uint64_t*)((uintptr_t)pickupItem + 0x50) = 0;
        *(uint64_t*)((uintptr_t)pickupItem + 0x58) = 0;

        pickupItemResult = pickupItem;
    }

    // Wrap in InternalBasePtr
    uintptr_t itemRef[2] = {0, 0};
    if (fn_InternalBasePtr_Ctor) {
        fn_InternalBasePtr_Ctor(&itemRef, pickupItemResult);
    }

    // CheckPtr on MatDesc ref
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)matDescBuf);
    }

    // Register draw item with scene
    if (s_fn_RegisterDrawItem) {
        s_fn_RegisterDrawItem(matDescBuf[0], (uint32_t)category, '\x01', (void*)itemRef);
    }
}

// ============================================================
// RawMaterialComponent::Update (vtable slot [9], 0x140129130)
// ============================================================
// Accumulates delta time into field38. When threshold exceeded,
// marks entity for collection and adds to world pending list.

void RawMaterialComponent_Update(RawMaterialComponent* self, int64_t* pDeltaTime) {
    EnsureHelpers();

    // Accumulate delta time into field38 (Time)
    int64_t* pAccumulator = (int64_t*)&self->field38;
    *pAccumulator += *pDeltaTime;

    // Construct threshold time from float constant
    int64_t thresholdTime[3]; // Time object on stack
    if (!s_fn_Time_FromFloat || !s_pThresholdTime) return;
    s_fn_Time_FromFloat(thresholdTime, *s_pThresholdTime);

    // Check if accumulated time exceeds threshold
    if (thresholdTime[0] < *pAccumulator) {
        void* entity = *(void**)((uintptr_t)self + 0x10);

        if (!s_pGameState || !*s_pGameState) return;
        uintptr_t gameState = (uintptr_t)*s_pGameState;
        uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xe8);

        if (entity != nullptr) {
            if (s_fn_CheckPtr) {
                s_fn_CheckPtr((void*)((uintptr_t)self + 0x10));
            }

            // Check bit 10 of entity flags at +0x40
            uint32_t entityFlags = *(uint32_t*)((uintptr_t)entity + 0x40);
            if (((entityFlags >> 10) & 1) == 0) {
                if (s_fn_CheckPtr) {
                    s_fn_CheckPtr((void*)((uintptr_t)self + 0x10));
                }

                if (s_fn_MarkForCollection) {
                    s_fn_MarkForCollection(entity);
                }

                // Add to world pending list
                if (s_fn_AddToPending) {
                    int currentCount = *(int*)(worldMgr + 0xd8);
                    s_fn_AddToPending((void*)(worldMgr + 0xd0), currentCount + 1,
                                     (void*)((uintptr_t)self + 0x10));
                }
            }
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void RawMaterialComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[RawMaterialComponent] Funcs resolved: Component_Serialize=%p "
           "Time_Serialize=%p CheckPtr=%p RegisterRawMaterial=%p "
           "GetPickupCategory=%p MemAlloc=%p RegisterDrawItem=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Time_Serialize,
           (void*)s_fn_CheckPtr, (void*)s_fn_RegisterRawMaterial,
           (void*)s_fn_GetPickupCategory, (void*)fn_MemAlloc,
           (void*)s_fn_RegisterDrawItem);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(RawMaterialComponent, 0x40, 0x89)
REGISTER_TYPE(RawMaterialComponent, "Component",
              &RawMaterialComponent_Factory, &RawMaterialComponent_CreateInPlace);
#endif // STANDALONE_EXE
