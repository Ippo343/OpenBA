// code/Game/Component/citizenaicomponent.cpp
// Clean reimplementation of CitizenAIComponent::Update from Application-x64.exe.
//
// CitizenAIComponent (0xD0 bytes) is the core citizen AI decision loop.
// Runs every game tick per citizen. Logic:
//   1. Decrement 16 cooldown timers at +0x40..+0xBF, clamp to 0
//   2. Increment general accumulators at +0xC0 and +0xC8
//   3. Find home storage, query resource count — if empty, reset +0xC8
//   4. Check hunger: citizen health +0x44 vs config threshold +0xCC
//      - If hungry and not in transition states: try to release workplace,
//        then transition to hunger state
//   5. Check thirst: citizen health +0x48 vs config threshold +0xEC
//      - Similar logic to hunger
//   6. If neither hungry nor thirsty: check if citizen can find housing
//   7. Check education eligibility and age transitions
//
// Vtable at 0x140162470.
// Original: 0x14010f3e0
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/citizenaicomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// StorageComponent_QueryCount(storageComponent) -> int
// At 0x14012cf10 (44 xrefs — major performance hotspot)
typedef int (*fn_StorageQueryCount_t)(int64_t storageComp);
static fn_StorageQueryCount_t s_fn_StorageQueryCount = nullptr;

// Entity_FindComponent_Storage(entity, outPtr) -> InternalBasePtr*
// At 0x140008090
typedef void* (*fn_FindComponentStorage_t)(int64_t entity, void* outPtr);
static fn_FindComponentStorage_t s_fn_FindComponentStorage = nullptr;

// Storage_GetResourceCount(storageComp) -> int
// At 0x14012a9d0
typedef int (*fn_GetResourceCount_t)(int64_t storageComp);
static fn_GetResourceCount_t s_fn_GetResourceCount = nullptr;

// CitizenComponent_ReleaseWorkplace(citizenComp, entityPtr)
// At 0x1401152e0
typedef void (*fn_ReleaseWorkplace_t)(int64_t citizenComp, void* entityPtr);
static fn_ReleaseWorkplace_t s_fn_ReleaseWorkplace = nullptr;

// CitizenComponent_CanFindHousing(citizenComp) -> bool
// At 0x140115be0
typedef char (*fn_CanFindHousing_t)(int64_t citizenComp);
static fn_CanFindHousing_t s_fn_CanFindHousing = nullptr;

// CitizenAIComponent_TransitionState(aiComp, param)
// At 0x14010db90
typedef void (*fn_TransitionState_t)(int64_t aiComp, char param);
static fn_TransitionState_t s_fn_TransitionState = nullptr;

// CitizenAIComponent_CheckEducation(aiComp)
// At 0x14010f7a0
typedef void (*fn_CheckEducation_t)(int64_t aiComp);
static fn_CheckEducation_t s_fn_CheckEducation = nullptr;

// CitizenAIComponent_CheckAge(aiComp)
// At 0x14010f890
typedef void (*fn_CheckAge_t)(int64_t aiComp);
static fn_CheckAge_t s_fn_CheckAge = nullptr;

// Globals
static uint32_t* s_pStorageTypeId = nullptr;  // DAT_1401c7e78

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    s_fn_StorageQueryCount = reinterpret_cast<fn_StorageQueryCount_t>(
        ResolveAddr(Module::Exe, 0x14012cf10));
    s_fn_FindComponentStorage = reinterpret_cast<fn_FindComponentStorage_t>(
        ResolveAddr(Module::Exe, 0x140008090));
    s_fn_GetResourceCount = reinterpret_cast<fn_GetResourceCount_t>(
        ResolveAddr(Module::Exe, 0x14012a9d0));
    s_fn_ReleaseWorkplace = reinterpret_cast<fn_ReleaseWorkplace_t>(
        ResolveAddr(Module::Exe, 0x1401152e0));
    s_fn_CanFindHousing = reinterpret_cast<fn_CanFindHousing_t>(
        ResolveAddr(Module::Exe, 0x140115be0));
    s_fn_TransitionState = reinterpret_cast<fn_TransitionState_t>(
        ResolveAddr(Module::Exe, 0x14010db90));
    s_fn_CheckEducation = reinterpret_cast<fn_CheckEducation_t>(
        ResolveAddr(Module::Exe, 0x14010f7a0));
    s_fn_CheckAge = reinterpret_cast<fn_CheckAge_t>(
        ResolveAddr(Module::Exe, 0x14010f890));

    s_pStorageTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7e78));

    s_helpersResolved = true;
}

// ============================================================
// CitizenAIComponent::Update (vtable slot [9], 0x14010f3e0)
// ============================================================
// Main citizen AI tick.
// Parameters: RCX = this, RDX = pointer to Time (int64*)

void CitizenAIComponent_Update(CitizenAIComponent* self, int64_t* pDeltaTime) {
    EnsureHelpers();
    int64_t dt = *pDeltaTime;

    uint8_t* comp = reinterpret_cast<uint8_t*>(self);

    // --- Phase 1: Decrement 16 cooldown timers at +0x40..+0xBF ---
    int64_t* timerPtr = reinterpret_cast<int64_t*>(comp + 0x40);
    for (int i = 0; i < 16; i++) {
        timerPtr[i] -= dt;
        if (timerPtr[i] < 0) {
            timerPtr[i] = 0;
        }
    }

    // --- Phase 2: Increment accumulators ---
    *reinterpret_cast<int64_t*>(comp + 0xC0) += dt;
    *reinterpret_cast<int64_t*>(comp + 0xC8) += dt;

    // --- Phase 3: Find storage component, check resource count ---
    // Navigate: self->data38 (IBP to citizen) -> citizen+0x10 (IBP to entity) -> components
    void* storageComp = nullptr;

    fn_InternalBasePtr_CheckPtr(comp + 0x38);
    int64_t citizenRef = *reinterpret_cast<int64_t*>(comp + 0x38);

    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(citizenRef + 0x10));
    int64_t citizenEntity = *reinterpret_cast<int64_t*>(citizenRef + 0x10);

    int componentCount = *reinterpret_cast<int*>(citizenEntity + 0x38);
    int64_t componentArray = *reinterpret_cast<int64_t*>(citizenEntity + 0x30);

    int idx = 0;
    int64_t byteOff = 0;
    for (; idx < componentCount; idx++, byteOff += 8) {
        void* ibpAddr = reinterpret_cast<void*>(componentArray + byteOff);
        fn_InternalBasePtr_CheckPtr(ibpAddr);

        int64_t compRef = *reinterpret_cast<int64_t*>(ibpAddr);
        uint32_t dVar3 = *s_pStorageTypeId;

        // vtable[2] = GetTypeId at +0x10
        int64_t compVtable = *reinterpret_cast<int64_t*>(compRef);
        typedef uint32_t (*fn_GetType_t)();
        fn_GetType_t getType = *reinterpret_cast<fn_GetType_t*>(compVtable + 0x10);
        uint32_t typeId = getType();

        if (typeId == dVar3) {
            // Re-read by index with CheckPtr (matches original)
            void* ibpAddr2 = reinterpret_cast<void*>(componentArray + (int64_t)idx * 8);
            fn_InternalBasePtr_CheckPtr(ibpAddr2);
            void* resolved = reinterpret_cast<void*>(*reinterpret_cast<int64_t*>(ibpAddr2));

            bool derived = fn_IsDerivedFrom(resolved, *s_pStorageTypeId);
            if (derived) {
                storageComp = resolved;
            }
            break;
        }
    }

    // Use inline fn_StorageComponent_QueryCount (from engine_funcs.h) instead of
    // the exe's StorageComponent_QueryCount (0x14012cf10). The exe version crashes
    // when a material type lacks RawMaterialDescription (NULL+0x48 deref).
    // Our inline version has a null guard on the component lookup.
    int storageCount = 0;
    if (storageComp) {
        storageCount = fn_StorageComponent_QueryCount(storageComp);
    }
    if (storageCount == 0) {
        *reinterpret_cast<int64_t*>(comp + 0xC8) = 0;
    }

    // --- Phase 4: Check hunger/thirst flags ---
    uint8_t hungerFlag = comp[0x30];
    uint8_t thirstFlag = comp[0x31];
    int aiState = *reinterpret_cast<int*>(comp + 0x28);

    bool isInNeedState = false;
    if ((hungerFlag != 0 && aiState == 0x0E) || (thirstFlag != 0 && aiState == 0x0F)) {
        isInNeedState = true;
    }

    // --- Phase 5: Hunger check ---
    fn_InternalBasePtr_CheckPtr(comp + 0x38);
    int64_t citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
    if (!citizenData) return;  // citizen entity gone, bail out
    float health = *reinterpret_cast<float*>(citizenData + 0x44);
    float* pHungerThresh = reinterpret_cast<float*>(
        *reinterpret_cast<int64_t*>(citizenData + 0x288) + 0xCC);

    if (health <= *pHungerThresh || health == *pHungerThresh || isInNeedState ||
        ((uint32_t)(aiState - 0x0E) < 2) ||
        *reinterpret_cast<int64_t*>(comp + 0xB0) > 0) {
        // Not hungry enough to transition, or in special state
        if (hungerFlag != 0) {
            fn_InternalBasePtr_CheckPtr(comp + 0x38);
            citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
            health = *reinterpret_cast<float*>(citizenData + 0x44);
            pHungerThresh = reinterpret_cast<float*>(
                *reinterpret_cast<int64_t*>(citizenData + 0x288) + 0xCC);

            if (health < *pHungerThresh || health == *pHungerThresh) {
                comp[0x30] = 0;
            }
        }
    } else {
        // Hungry — check if has workplace with resources, release if so
        fn_InternalBasePtr_CheckPtr(comp + 0x38);
        if (*reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(comp + 0x38) + 0x238) != 0) {
            fn_InternalBasePtr_CheckPtr(comp + 0x38);
            int64_t wpEntity = *reinterpret_cast<int64_t*>(
                *reinterpret_cast<int64_t*>(comp + 0x38) + 0x238);

            fn_InternalBasePtr_CheckPtr(&wpEntity);
            int64_t findResult[2] = {};
            void* pResult = s_fn_FindComponentStorage(wpEntity, findResult);

            fn_InternalBasePtr_CheckPtr(pResult);
            int64_t wpStorage = *reinterpret_cast<int64_t*>(pResult);
            int resourceCount = s_fn_GetResourceCount(wpStorage);

            if (resourceCount > 0) {
                fn_InternalBasePtr_CheckPtr(comp + 0x38);
                int64_t wpRef = *reinterpret_cast<int64_t*>(
                    *reinterpret_cast<int64_t*>(comp + 0x38) + 0x238);
                fn_InternalBasePtr_CheckPtr(comp + 0x38);
                s_fn_ReleaseWorkplace(
                    *reinterpret_cast<int64_t*>(comp + 0x38),
                    &wpRef);
            }
        }

        // Re-check health after potentially releasing workplace
        fn_InternalBasePtr_CheckPtr(comp + 0x38);
        citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
        health = *reinterpret_cast<float*>(citizenData + 0x44);
        pHungerThresh = reinterpret_cast<float*>(
            *reinterpret_cast<int64_t*>(citizenData + 0x288) + 0xCC);

        if (*pHungerThresh <= health && health != *pHungerThresh) {
            s_fn_TransitionState(reinterpret_cast<int64_t>(self), 1);
            comp[0x30] = (*reinterpret_cast<int*>(comp + 0x28) == 0x0E) ? 1 : 0;
        }
    }

    // --- Phase 6: Thirst check ---
    fn_InternalBasePtr_CheckPtr(comp + 0x38);
    citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
    float hydration = *reinterpret_cast<float*>(citizenData + 0x48);
    float* pThirstThresh = reinterpret_cast<float*>(
        *reinterpret_cast<int64_t*>(citizenData + 0x288) + 0xEC);

    // Re-read flags (may have been modified above)
    thirstFlag = comp[0x31];
    aiState = *reinterpret_cast<int*>(comp + 0x28);
    isInNeedState = ((comp[0x30] != 0 && aiState == 0x0E) ||
                     (thirstFlag != 0 && aiState == 0x0F));

    if (hydration <= *pThirstThresh || hydration == *pThirstThresh || isInNeedState ||
        ((uint32_t)(aiState - 0x0E) < 2) ||
        *reinterpret_cast<int64_t*>(comp + 0xB8) > 0) {
        // Not thirsty enough or in special state
        if (thirstFlag != 0) {
            fn_InternalBasePtr_CheckPtr(comp + 0x38);
            citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
            hydration = *reinterpret_cast<float*>(citizenData + 0x48);
            pThirstThresh = reinterpret_cast<float*>(
                *reinterpret_cast<int64_t*>(citizenData + 0x288) + 0xEC);

            if (hydration < *pThirstThresh || hydration == *pThirstThresh) {
                comp[0x31] = 0;
            }
        }
    } else {
        // Thirsty — transition to thirst state
        s_fn_TransitionState(reinterpret_cast<int64_t>(self), 1);
        comp[0x31] = (*reinterpret_cast<int*>(comp + 0x28) == 0x0F) ? 1 : 0;
    }

    // --- Phase 7: Housing check ---
    if (comp[0x30] == 0 && comp[0x31] == 0) {
        fn_InternalBasePtr_CheckPtr(comp + 0x38);
        citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
        char canFindHouse = s_fn_CanFindHousing(citizenData);
        if (canFindHouse != 0 && *reinterpret_cast<int*>(comp + 0x28) != 2) {
            s_fn_TransitionState(reinterpret_cast<int64_t>(self), 1);
        }
    }

    // --- Phase 8: Check health status for education/age ---
    fn_InternalBasePtr_CheckPtr(comp + 0x38);
    citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(citizenData + 0x278));
    int64_t healthComp = *reinterpret_cast<int64_t*>(citizenData + 0x278);
    if (*reinterpret_cast<int*>(healthComp + 0x60) + 1 > 0) {
        // Citizen is alive (health > -1), skip education/age checks
        return;
    }

    // Citizen health critical — check education and age transitions
    s_fn_CheckEducation(reinterpret_cast<int64_t>(self));
    s_fn_CheckAge(reinterpret_cast<int64_t>(self));
    s_fn_TransitionState(reinterpret_cast<int64_t>(self), 0);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void CitizenAIComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[CitizenAIComponent] Funcs resolved: StorageQueryCount=%p FindCompStorage=%p "
           "GetResourceCount=%p ReleaseWorkplace=%p CanFindHousing=%p "
           "TransitionState=%p CheckEducation=%p CheckAge=%p StorageTypeId=%p->0x%x\n",
           (void*)s_fn_StorageQueryCount, (void*)s_fn_FindComponentStorage,
           (void*)s_fn_GetResourceCount, (void*)s_fn_ReleaseWorkplace,
           (void*)s_fn_CanFindHousing, (void*)s_fn_TransitionState,
           (void*)s_fn_CheckEducation, (void*)s_fn_CheckAge,
           (void*)s_pStorageTypeId, s_pStorageTypeId ? *s_pStorageTypeId : 0);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0xd0 (CitizenAIComponent_Factory MemAlloc), flags 0x89 (Ctor 0x14010fb70:
// Component base sets +0x20=0x80, leaf OR 0x89 => 0x89).
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(CitizenAIComponent, 0xd0, 0x89)
REGISTER_TYPE(CitizenAIComponent, "Component",
              &CitizenAIComponent_Factory, &CitizenAIComponent_CreateInPlace);
#endif // STANDALONE_EXE
