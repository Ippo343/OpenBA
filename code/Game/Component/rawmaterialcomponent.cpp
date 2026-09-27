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

#include "Component/rawmaterialcomponent_funcs.h"
#include "Component/RawMaterialDescription.h"
#include "Work/Pickup.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// ============================================================
// Resolved helpers
// ============================================================

// FUN_140018020 — serialize field30 (version >= 2)
typedef void (*fn_SerializeField30_t)(void* fieldPtr, void* serializer);
static fn_SerializeField30_t s_fn_SerializeField30 = nullptr;

// FUN_140091e60 — register raw material with world manager
typedef void (*fn_RegisterRawMaterial_t)(void* worldMgr, void* entityChain);
static fn_RegisterRawMaterial_t s_fn_RegisterRawMaterial = nullptr;

// FUN_1400c2ea0 — mark entity for collection
typedef void (*fn_MarkForCollection_t)(void* entity);
static fn_MarkForCollection_t s_fn_MarkForCollection = nullptr;

// FUN_140003110 — add entity to world pending list

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


// fn_InternalBasePtr_Ctor — already in engine_funcs.h
// fn_IsDerivedFrom — already in engine_funcs.h

// CitizenAI_impl_ec60 — register draw item with scene
typedef void (*fn_PostWork_t)(uintptr_t workComp, uint32_t code, char param3, void* workRef);
static fn_PostWork_t s_fn_PostWork = nullptr;


static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;


    uintptr_t slot;

    s_fn_SerializeField30 = reinterpret_cast<fn_SerializeField30_t>(
        ResolveAddr(Module::Exe, 0x140018020));

    s_fn_RegisterRawMaterial = reinterpret_cast<fn_RegisterRawMaterial_t>(
        ResolveAddr(Module::Exe, 0x140091e60));

    s_fn_MarkForCollection = reinterpret_cast<fn_MarkForCollection_t>(
        ResolveAddr(Module::Exe, 0x1400c2ea0));

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

    // CitizenAI_impl_ec60 = the citizen WORK-SCHEDULER POST (the same call the ClearArea
    // controller uses to hand a job to a laborer), NOT a render/draw registration.
    s_fn_PostWork = reinterpret_cast<fn_PostWork_t>(
        ResolveAddr(Module::Exe, 0x14012ec60));  // CALL @14012929e in RawMaterialComponent::Draw

    s_initialized = true;
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

            // Check bit 10 of entity flags at +0x40
            uint32_t entityFlags = *(uint32_t*)((uintptr_t)entity + 0x40);
            if (((entityFlags >> 10) & 1) == 0) {

                if (s_fn_MarkForCollection) {
                    s_fn_MarkForCollection(entity);
                }

                // Add to world pending list
                // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
                {
                    int currentCount = *(int*)(worldMgr + 0xd8);
                    GameColl::AsArray<GameColl::Ptr8>((void*)(worldMgr + 0xd0))
                        .SetSizeFill(currentCount + 1, (void*)((uintptr_t)self + 0x10));
                }
            }
        }
    }
}

// ============================================================
// Reimpl of FUN_140091e60 (register raw material with world manager:
//   grow-array-and-append an 8-byte pointer into the manager's list at +0x110)
// ============================================================
// param_1 = list owner (count +0x118, capacity +0x11c, ptr array +0x110, stride 8).
// Scans the existing array for param_2; if present, returns. Otherwise grows
// (newCount = oldCount+1, tripled capacity if previously non-empty), copies the
// old elements, writes param_2 into the new tail slot, and frees the old buffer.
void RawMaterialComponent_RegisterRawMaterial(void* worldMgr, void* entityChain) {
    uintptr_t base = (uintptr_t)worldMgr;
    int64_t value = (int64_t)entityChain;

    // Scan for existing entry
    int count = *(int*)(base + 0x118);
    int64_t* arr = *(int64_t**)(base + 0x110);
    for (int64_t i = 0; i < count; i++) {
        if (arr[i] == value) {
            return;
        }
    }

    int oldCount = *(int*)(base + 0x118);
    int newCount = oldCount + 1;
    if (newCount == oldCount) return; // unreachable, matches original guard

    int64_t* oldBuf = *(int64_t**)(base + 0x110);
    *(int*)(base + 0x118) = newCount;

    if (*(int*)(base + 0x11c) < newCount) {
        *(int*)(base + 0x11c) = newCount;
        if (oldCount != 0) {
            *(int*)(base + 0x11c) = newCount * 3;
        }
        void* nb = fn_MemAlloc(g_pMemAllocSingleton,
                               (uint64_t)((int64_t)*(int*)(base + 0x11c) << 3), 8);
        *(int64_t**)(base + 0x110) = (int64_t*)nb;
    }

    int start = 0;
    if (oldBuf == *(int64_t**)(base + 0x110)) {
        start = oldCount;
    }
    int64_t* dst = *(int64_t**)(base + 0x110);
    for (int i = start; i < newCount; i++) {
        int64_t* slot = dst + i;
        if (slot) {
            if (i < oldCount) {
                *slot = oldBuf[i];
            } else {
                *slot = value;
            }
        }
    }

    if (oldBuf != *(int64_t**)(base + 0x110)) {
        fn_MemFreeStatic(oldBuf);
    }
}

// ============================================================
// Reimpl of FUN_140129480 (RawMaterialComponent_helper_9480:
//   derive pickup visual category bits from the description's flag word at +0x5c)
// ============================================================
uint64_t RawMaterialComponent_GetPickupCategory(uintptr_t rawMatDesc) {
    uint32_t flags = *(uint32_t*)(rawMatDesc + 0x5c);
    if ((flags >> 5) & 1)   return 2;
    if ((flags >> 6) & 1)   return 4;
    if ((flags >> 10) & 1)  return 0x10;
    if ((flags >> 12) & 1)  return 0x40;
    if ((flags >> 11) & 1)  return 0x20;
    if ((flags >> 13) & 1)  return 0x100;
    if ((flags >> 16) & 1)  return 0x200;
    if ((flags >> 9) & 1)   return 8;
    if ((flags >> 14) & 1)  return 0x80;
    return 1;
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void RawMaterialComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[RawMaterialComponent] Funcs resolved: "
           "RegisterRawMaterial=%p "
           "GetPickupCategory=%p MemAlloc=%p PostWork=%p\n",
           (void*)s_fn_RegisterRawMaterial,
           (void*)s_fn_GetPickupCategory, (void*)fn_MemAlloc,
           (void*)s_fn_PostWork);
#endif
}

// ============================================================================
// ENDGAME B.3.8 VIS — RawMaterialComponent as a REAL class (RawMaterialComponent.h).
//
// Before: a 48-slot RawMaterialComponent_InertVtable filled per create,
// Inert_ScalarDtor in slot 0 (only ~Internal).
// After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/RawMaterialComponent.h"

// 0x140129400: Component() is the CALL 0x1401188b0; the 0x140164170 stamp is
// the compiler's; +0x30 = +0x38 = 0; flags |= 0x89 (bit 0 enrols it in the
// OnAddedPass, bit 7 in StartupComponents). Retail leaves +0x28 unwritten
// (Startup sets it); the tree's zero is kept.
RawMaterialComponent::RawMaterialComponent()
    : field28(nullptr), field30(nullptr), field38(nullptr) {
    OrFlagsInCtor(0x89);
}

// ============================================================
// RawMaterialComponent::Persist (vtable slot [3], 0x140129330)
// ============================================================
// Versioned serialization (version 2):
//   - field28 (resource type ID, int)
//   - field38 (Time accumulator)
//   - field30 (version >= 2 only)

void RawMaterialComponent::Persist(void* stream) {
    EnsureHelpers();

    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    alignas(8) uint8_t serializerBuf[24];
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 2);   // B.3.4 bridge

    // Serialize field28 (resource type ID)
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int*>(&field28), nullptr);   // B.3.4 bridge

    // Serialize field38 (Time accumulator)
    reinterpret_cast<System::Time*>(&field38)->Serialize(*reinterpret_cast<IO::Serializer*>(serializerBuf));   // B.3.4b bridge

    // Version >= 2: serialize field30
    int version = reinterpret_cast<IO::Serializer*>(serializerBuf)->GetVersion();   // B.3.4 bridge
    if (version >= 2 && s_fn_SerializeField30) {
        s_fn_SerializeField30(&field30, serializerBuf);
    }

    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();                                       // B.3.4 bridge
}

// ============================================================
// RawMaterialComponent::Startup (vtable slot [4], 0x1401292c0)
// ============================================================
// Init field28 from building data type at +0x40, then register
// the entity with the world raw material manager.

void RawMaterialComponent::Startup() {
    EnsureHelpers();

    // Get building data via IsDerivedFrom check on _unk18
    void* baseObj = _description;
    void* buildingData = nullptr;
    if (fn_IsDerivedFrom) {
        bool isDerived = fn_IsDerivedFrom(baseObj, RawMaterialDescription::TypeId());
        if (isDerived) {
            buildingData = baseObj;
        }
    }

    // Copy resource type from building data +0x40 to field28
    if (buildingData) {
        *(uint32_t*)&field28 = *(uint32_t*)((uintptr_t)buildingData + 0x40);
    }

    // CheckPtr on owner at +0x10

    // Get entity chain from owner+0x28, register with world manager
    if (s_pGameState && *s_pGameState && s_fn_RegisterRawMaterial) {
        uintptr_t gameState = (uintptr_t)*s_pGameState;
        uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);
        void* entityChain = *(void**)(*(uintptr_t*)(reinterpret_cast<uintptr_t>(this) + 0x10) + 0x28);
        s_fn_RegisterRawMaterial((void*)worldMgr, entityChain);
    }
}

// ============================================================
// RawMaterialComponent::OnAdded (vtable slot [6], 0x1401291d0)
// ============================================================
// ⚠ NOT a render call — the name is Ghidra's. This is THE PILE-PICKUP TRIGGER: it builds a
// Pickup WORK and posts it to the citizen work-scheduler, so a laborer comes and hauls the
// pile away. Slot [6] (+0x30) is the OnAdded hook that ComponentEntity_OnAddedPass dispatches
// when the entity is registered (flags 0x89 have bit 0 set), i.e. the moment a harvested pile
// joins the world via EntityList_RegisterEntity.
//
// Original logic (FUN_1401291d0):
//   1. CheckPtr on entity ref (+0x10)
//   2. FUN_140003050 on the owner entity — its WorkComponent (the post target)
//   3. IsDerivedFrom check on _unk18 for RawMaterialDescription
//   4. helper_9480 — the work kind/category code from the description
//   5. MemAlloc 0x60 bytes for the Pickup work
//   6. Work_Ctor25 (0x1400f72e0) to initialize the Work base
//   7. Set vtable to vtable_Pickup (0x140161810), zero fields at +0x48, +0x50, +0x58
//   8. InternalBasePtr_Ctor to wrap the work
//   9. CheckPtr on the WorkComponent ref
//  10. CitizenAI_impl_ec60 (0x14012ec60) — POST the work to the scheduler

void RawMaterialComponent::OnAdded() {
    EnsureHelpers();

    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    // CheckPtr on entity ref

    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);

    // FindComponent_MatDesc on entity
    uintptr_t matDescBuf[2] = {0, 0};
    if (s_fn_FindComponent_MatDesc) {
        s_fn_FindComponent_MatDesc(entity, (void*)matDescBuf);
    }

    // Get RawMaterialDescription via IsDerivedFrom
    void* baseObj = _description;
    void* rawMatDesc = nullptr;
    if (fn_IsDerivedFrom) {
        if (fn_IsDerivedFrom(baseObj, RawMaterialDescription::TypeId())) {
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
        // Retail: `14012924b CALL 0x1400f72e0` (Work_Ctor25, the entity-seeded base
        // ctor), `LEA RAX,[0x140161810]; MOV [RBX],RAX` (the Pickup table), then
        // +0x48 = +0x50 = +0x58 = 0. B.3.8 (B8-5): the real Pickup ctor builds the same
        // bytes (the compiler stamps Pickup's table and zeroes the three fields), and
        // Work::InitFromEntity is Work_Ctor25's body.
        uintptr_t entityRefBuf[2];
        entityRefBuf[0] = entity;
        Pickup* pickup = new (pickupItem) Pickup();
        pickup->InitFromEntity(entityRefBuf);

        pickupItemResult = pickupItem;
    }

    // Wrap in InternalBasePtr
    uintptr_t itemRef[2] = {0, 0};
    if (fn_InternalBasePtr_Ctor) {
        new (&itemRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(pickupItemResult));   // B.3.3 bridge
    }

    // CheckPtr on the WorkComponent ref

    // POST the Pickup work to the citizen work-scheduler — a laborer then claims it and
    // hauls this pile away. This is the pile-pickup trigger.
    if (s_fn_PostWork) {
        s_fn_PostWork(matDescBuf[0], (uint32_t)category, '\x01', (void*)itemRef);
    }
}

// [9] 0x140129130. PORT GAP (retail 0x140129130) — B38 D9: the body is ported
// above (RawMaterialComponent_Update, the pile expiry timer) but the tree's
// table never installed it; the inert stub is kept here. Installing it is its
// own titled commit.
void RawMaterialComponent::Update(void*) {}

// [0] 0x1400373c0: ~RawMaterialComponent (0x1401293c0: restamp + ~Internal), MemFree on &1.
Object::Base* RawMaterialComponent::DeletingDtor(unsigned flags) {
    this->~RawMaterialComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140037270: MemAlloc(0x40, 8), then the ctor.
void* RawMaterialComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(RawMaterialComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) RawMaterialComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* RawMaterialComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) RawMaterialComponent());
}

DEFINE_EXE_VTABLE(RawMaterialComponent, "_ZTV20RawMaterialComponent")
DEFINE_EXE_TYPE(RawMaterialComponent, "Component")
