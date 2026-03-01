// engine/reimpl/citizenaicomponent.cpp
// Reimplementation of CitizenAIComponent::Update from Application-x64.exe.
//
// Core citizen AI decision loop. Runs every game tick per citizen. Logic:
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
// Original: 0x14010f3e0

#include "CitizenAIComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;

// ============================================================
// Helper function pointers (resolved at install time)
// ============================================================

// Inline component search on entity (same pattern as FUN_140002e10 etc.)
// This one searches by type ID DAT_1401c7e78 (storage-related)
// We just inline the component lookup that the decompiler expanded

// StorageComponent_QueryCount(storageComponent) -> int
// At 0x14012cf10 (44 xrefs — major performance hotspot)
typedef int (*fn_StorageQueryCount_t)(int64_t storageComp);
static fn_StorageQueryCount_t fn_StorageQueryCount = nullptr;

// Entity_FindComponent_Storage(entity, outPtr) -> InternalBasePtr*
// At 0x140008090
typedef void* (*fn_FindComponentStorage_t)(int64_t entity, void* outPtr);
static fn_FindComponentStorage_t fn_FindComponentStorage = nullptr;

// Storage_GetResourceCount(storageComp) -> int
// At 0x14012a9d0
typedef int (*fn_GetResourceCount_t)(int64_t storageComp);
static fn_GetResourceCount_t fn_GetResourceCount = nullptr;

// CitizenComponent_ReleaseWorkplace(citizenComp, entityPtr)
// At 0x1401152e0
typedef void (*fn_ReleaseWorkplace_t)(int64_t citizenComp, void* entityPtr);
static fn_ReleaseWorkplace_t fn_ReleaseWorkplace = nullptr;

// CitizenComponent_CanFindHousing(citizenComp) -> bool
// At 0x140115be0
typedef char (*fn_CanFindHousing_t)(int64_t citizenComp);
static fn_CanFindHousing_t fn_CanFindHousing = nullptr;

// CitizenAIComponent_TransitionState(aiComp, param)
// At 0x14010db90
typedef void (*fn_TransitionState_t)(int64_t aiComp, char param);
static fn_TransitionState_t fn_TransitionState = nullptr;

// CitizenAIComponent_CheckEducation(aiComp)
// At 0x14010f7a0
typedef void (*fn_CheckEducation_t)(int64_t aiComp);
static fn_CheckEducation_t fn_CheckEducation = nullptr;

// CitizenAIComponent_CheckAge(aiComp)
// At 0x14010f890
typedef void (*fn_CheckAge_t)(int64_t aiComp);
static fn_CheckAge_t fn_CheckAge = nullptr;

// Globals
static uint32_t* g_pStorageTypeId = nullptr;  // DAT_1401c7e78

// ============================================================
// Update (0x14010f3e0)
// Main citizen AI tick.
//
// Parameters: RCX = this, RDX = pointer to Time (int64*)
// ============================================================
static void Reimpl_Update(CitizenAIComponent* self, int64_t* pDeltaTime) {
    g_lastHook = "CitizenAIComponent::Update";
    int64_t dt = *pDeltaTime;

#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[CitizenAIComponent::Update] self=%p dt=%lld aiState=%d\n",
               (void*)self, dt, self->aiState);
        callCount++;
    }
#endif

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
    // Navigate: self->data38 -> entity owner -> component list -> find by type
    // CheckPtr calls are no-ops, skipped
    int64_t citizenRef = *reinterpret_cast<int64_t*>(comp + 0x38);
    int64_t citizenEntity = *reinterpret_cast<int64_t*>(citizenRef + 0x10);

    // Inline component search for storage type
    int componentCount = *reinterpret_cast<int*>(citizenEntity + 0x38);
    int64_t componentArray = *reinterpret_cast<int64_t*>(citizenEntity + 0x30);

    void* storageComp = nullptr;
    for (int i = 0; i < componentCount; i++) {
        // CheckPtr — no-op
        int64_t compRef = *reinterpret_cast<int64_t*>(componentArray + i * 8);
        // Get type ID via vtable call at +0x10 (GetDescription/GetType)
        int64_t compVtable = *reinterpret_cast<int64_t*>(compRef);
        typedef int (*fn_GetType_t)();
        fn_GetType_t getType = *reinterpret_cast<fn_GetType_t*>(compVtable + 0x10);
        int typeId = getType();

        if (typeId == static_cast<int>(*g_pStorageTypeId)) {
            // CheckPtr — no-op
            // IsDerivedFrom check
            bool derived = fn_IsDerivedFrom(
                reinterpret_cast<void*>(compRef), *g_pStorageTypeId);
            if (derived) {
                storageComp = reinterpret_cast<void*>(compRef);
            }
            break;
        }
    }

    // InternalBasePtr constructor — just stores the pointer
    int64_t storageRef = reinterpret_cast<int64_t>(storageComp);

    // CheckPtr — no-op
    int storageCount = fn_StorageQueryCount(storageRef);
    if (storageCount == 0) {
        *reinterpret_cast<int64_t*>(comp + 0xC8) = 0;
    }

    // --- Phase 4: Check hunger/thirst flags ---
    // data30 byte 0 = hunger flag, data30 byte 1 = thirst flag
    uint8_t hungerFlag = *reinterpret_cast<uint8_t*>(comp + 0x30);
    uint8_t thirstFlag = *reinterpret_cast<uint8_t*>(comp + 0x31);
    int aiState = *reinterpret_cast<int*>(comp + 0x28);

    // bVar4 = (hungerFlag && aiState == 0xE) || (thirstFlag && aiState == 0xF)
    bool isInNeedState = false;
    if ((hungerFlag != 0 && aiState == 0x0E) || (thirstFlag != 0 && aiState == 0x0F)) {
        isInNeedState = true;
    }

    // --- Phase 5: Hunger check ---
    // CheckPtr — no-op
    int64_t citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
    float health = *reinterpret_cast<float*>(citizenData + 0x44);
    int64_t configPtr = *reinterpret_cast<int64_t*>(citizenData + 0x288);
    float hungerThreshold = *reinterpret_cast<float*>(configPtr + 0xCC);

    if (health <= hungerThreshold || isInNeedState ||
        (aiState - 0x0EU) < 2 || *reinterpret_cast<int64_t*>(comp + 0xB0) > 0) {
        // Not hungry enough to transition, or in special state
        // But if hunger flag is set and health is now OK, clear it
        if (hungerFlag != 0) {
            // Re-read health (CheckPtr — no-op)
            citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
            health = *reinterpret_cast<float*>(citizenData + 0x44);
            configPtr = *reinterpret_cast<int64_t*>(citizenData + 0x288);
            hungerThreshold = *reinterpret_cast<float*>(configPtr + 0xCC);

            if (health <= hungerThreshold) {
                *reinterpret_cast<uint8_t*>(comp + 0x30) = 0;
            }
        }
    } else {
        // Hungry — check if has workplace with resources, release if so
        // CheckPtr — no-op
        citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
        int64_t workplaceRef = *reinterpret_cast<int64_t*>(citizenData + 0x238);
        if (workplaceRef != 0) {
            // CheckPtr — no-op
            citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
            int64_t workplaceEntity = *reinterpret_cast<int64_t*>(citizenData + 0x238);
            // CheckPtr — no-op (on workplaceEntity)
            int64_t local_res8 = workplaceEntity;
            // Find storage component on workplace
            int64_t findResult[2] = {};
            void* pResult = fn_FindComponentStorage(local_res8, findResult);
            // CheckPtr — no-op
            int64_t wpStorage = *reinterpret_cast<int64_t*>(pResult);
            int resourceCount = fn_GetResourceCount(wpStorage);

            if (resourceCount > 0) {
                // Re-read citizen data and release workplace
                citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
                int64_t wpRef = *reinterpret_cast<int64_t*>(citizenData + 0x238);
                // CheckPtr — no-op
                citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
                fn_ReleaseWorkplace(citizenData, &wpRef);
            }
        }

        // Check health again after potentially releasing workplace
        // CheckPtr — no-op
        citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
        health = *reinterpret_cast<float*>(citizenData + 0x44);
        configPtr = *reinterpret_cast<int64_t*>(citizenData + 0x288);
        hungerThreshold = *reinterpret_cast<float*>(configPtr + 0xCC);

        if (health > hungerThreshold && health != hungerThreshold) {
            fn_TransitionState(reinterpret_cast<int64_t>(self), 1);
            *reinterpret_cast<uint8_t*>(comp + 0x30) =
                (*reinterpret_cast<int*>(comp + 0x28) == 0x0E) ? 1 : 0;
        }
    }

    // --- Phase 6: Thirst check ---
    // Re-read thirst data
    // CheckPtr — no-op
    citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
    float hydration = *reinterpret_cast<float*>(citizenData + 0x48);
    configPtr = *reinterpret_cast<int64_t*>(citizenData + 0x288);
    float thirstThreshold = *reinterpret_cast<float*>(configPtr + 0xEC);

    // Re-read flags (may have been modified above)
    thirstFlag = *reinterpret_cast<uint8_t*>(comp + 0x31);
    aiState = *reinterpret_cast<int*>(comp + 0x28);
    isInNeedState = ((*reinterpret_cast<uint8_t*>(comp + 0x30) != 0 && aiState == 0x0E) ||
                     (thirstFlag != 0 && aiState == 0x0F));

    if (hydration <= thirstThreshold || isInNeedState ||
        (aiState - 0x0EU) < 2 || *reinterpret_cast<int64_t*>(comp + 0xB8) > 0) {
        // Not thirsty enough or in special state
        if (thirstFlag != 0) {
            // CheckPtr — no-op
            citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
            hydration = *reinterpret_cast<float*>(citizenData + 0x48);
            configPtr = *reinterpret_cast<int64_t*>(citizenData + 0x288);
            thirstThreshold = *reinterpret_cast<float*>(configPtr + 0xEC);

            if (hydration <= thirstThreshold) {
                *reinterpret_cast<uint8_t*>(comp + 0x31) = 0;
            }
        }
    } else {
        // Thirsty — transition to thirst state
        fn_TransitionState(reinterpret_cast<int64_t>(self), 1);
        *reinterpret_cast<uint8_t*>(comp + 0x31) =
            (*reinterpret_cast<int*>(comp + 0x28) == 0x0F) ? 1 : 0;
    }

    // --- Phase 7: Housing check ---
    hungerFlag = *reinterpret_cast<uint8_t*>(comp + 0x30);
    thirstFlag = *reinterpret_cast<uint8_t*>(comp + 0x31);
    if (hungerFlag == 0 && thirstFlag == 0) {
        // CheckPtr — no-op
        citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
        char canFindHouse = fn_CanFindHousing(citizenData);
        if (canFindHouse != 0 && *reinterpret_cast<int*>(comp + 0x28) != 2) {
            fn_TransitionState(reinterpret_cast<int64_t>(self), 1);
        }
    }

    // --- Phase 8: Check health status for education/age ---
    // CheckPtr — no-op
    citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
    int64_t healthComp = *reinterpret_cast<int64_t*>(citizenData + 0x278);
    // CheckPtr — no-op
    int healthVal = *reinterpret_cast<int*>(healthComp + 0x60);
    if (healthVal + 1 > 0) {
        // Citizen is alive (health > -1), skip education/age checks
        return;
    }

    // Citizen health critical — check education and age transitions
    fn_CheckEducation(reinterpret_cast<int64_t>(self));
    fn_CheckAge(reinterpret_cast<int64_t>(self));
    fn_TransitionState(reinterpret_cast<int64_t>(self), 0);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCitizenAIComponentUpdate() {
    // Resolve exe helper functions
    fn_StorageQueryCount = reinterpret_cast<fn_StorageQueryCount_t>(
        ResolveAddr(Module::Exe, 0x14012cf10));
    fn_FindComponentStorage = reinterpret_cast<fn_FindComponentStorage_t>(
        ResolveAddr(Module::Exe, 0x140008090));
    fn_GetResourceCount = reinterpret_cast<fn_GetResourceCount_t>(
        ResolveAddr(Module::Exe, 0x14012a9d0));
    fn_ReleaseWorkplace = reinterpret_cast<fn_ReleaseWorkplace_t>(
        ResolveAddr(Module::Exe, 0x1401152e0));
    fn_CanFindHousing = reinterpret_cast<fn_CanFindHousing_t>(
        ResolveAddr(Module::Exe, 0x140115be0));
    fn_TransitionState = reinterpret_cast<fn_TransitionState_t>(
        ResolveAddr(Module::Exe, 0x14010db90));
    fn_CheckEducation = reinterpret_cast<fn_CheckEducation_t>(
        ResolveAddr(Module::Exe, 0x14010f7a0));
    fn_CheckAge = reinterpret_cast<fn_CheckAge_t>(
        ResolveAddr(Module::Exe, 0x14010f890));

    // Globals
    g_pStorageTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7e78));

#ifdef _DEBUG
    printf("[CitizenAIComponent] StorageQueryCount  = %p\n", (void*)fn_StorageQueryCount);
    printf("[CitizenAIComponent] FindCompStorage    = %p\n", (void*)fn_FindComponentStorage);
    printf("[CitizenAIComponent] GetResourceCount   = %p\n", (void*)fn_GetResourceCount);
    printf("[CitizenAIComponent] ReleaseWorkplace   = %p\n", (void*)fn_ReleaseWorkplace);
    printf("[CitizenAIComponent] CanFindHousing     = %p\n", (void*)fn_CanFindHousing);
    printf("[CitizenAIComponent] TransitionState    = %p\n", (void*)fn_TransitionState);
    printf("[CitizenAIComponent] CheckEducation     = %p\n", (void*)fn_CheckEducation);
    printf("[CitizenAIComponent] CheckAge           = %p\n", (void*)fn_CheckAge);
    printf("[CitizenAIComponent] StorageTypeId      = %p -> 0x%x\n",
           (void*)g_pStorageTypeId, g_pStorageTypeId ? *g_pStorageTypeId : 0);
#endif

    if (!fn_StorageQueryCount || !fn_FindComponentStorage || !fn_GetResourceCount ||
        !fn_ReleaseWorkplace || !fn_CanFindHousing || !fn_TransitionState ||
        !fn_CheckEducation || !fn_CheckAge) {
#ifdef _DEBUG
        printf("[CitizenAIComponent] ERROR: failed to resolve helpers, skipping hook\n");
#endif
        return;
    }

    if (!fn_IsDerivedFrom) {
#ifdef _DEBUG
        printf("[CitizenAIComponent] ERROR: fn_IsDerivedFrom not resolved, skipping hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, CitizenAIComponentAddr::Update);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x60],RBX(5) +
    //           MOV [RSP+0x68],RBP(5) = 16 bytes.
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 16);
#ifdef _DEBUG
    printf("[CitizenAIComponent] Update hooked at %p (prologue=16)\n", (void*)target);
#endif
}

void RemoveCitizenAIComponentUpdate() { RemoveHook(&g_hookUpdate); }
