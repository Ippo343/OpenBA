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

#include "Component/citizenaicomponent_funcs.h"
#include "Component/RawMaterialDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/CitizenComponent.h"   // ENDGAME B.3.8 CIT (TypeId)
#include "Component/ResidenceComponent.h" // B.3.8 CB1a (TypeId)
#include "Component/StorageComponent.h"   // B.3.8 CB1a (TypeId)
#include "options.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// Force-work harness target: the first start-town structure (StorageBarn), captured
// in starttown.cpp. Used only when env BANISHED_FORCE_WORK is set. See UpdateAdapter.
extern void* g_pForceWorkBuilding;

// Phase B production harness (production_harness.cpp, env BANISHED_LIVE_PRODUCTION).
// One-shot: drives a faithful log-production accounting event at the ForesterLodge and
// proves the town stock count increments. Self-gated; no-op when the env is unset.

// Phase B-real force-ForesterWork driver (forester_force_harness.cpp, env
// BANISHED_FORCE_FORESTERWORK). Constructs + pushes a ForesterWork action onto ONE
// force-worked citizen's CharacterComponent action stack (replacing the inert building
// ResourceProduceComponent controller) so the executor ticks ForesterWork::Execute ->
// real tree harvest -> production. Returns true if it owns the citizen this tick (the
// caller then skips the competing AI state machine). Self-gated; no-op when env unset.
extern "C" bool StartTown_ForcePushForesterWork(void* citizen);
// Organic-construction test driver (construction_force_harness.cpp, env
// BANISHED_FORCE_CONSTRUCT). Same contract: drives ONE citizen's construction Work
// (gather -> build) at the demo foundation; owns the citizen when active; no-op when unset.
extern "C" bool StartTown_ForcePushConstructionWork(void* citizen);

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

    // ENDGAME B.3.9 (B39_RULES.md D5): 0x1401c7e78 is StorageComponent's registered
    // type-id cell (INVENTORY_C Table 1). This initial value is always overwritten by
    // CitizenAIComponent::Update's per-tick repoint (s_pStorageTypeId = &s_caiTidStorage,
    // below) before any reader sees it, so the direct static call is mechanically
    // equivalent to the old cell read.
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

    int64_t citizenRef = *reinterpret_cast<int64_t*>(comp + 0x38);

    int64_t citizenEntity = *reinterpret_cast<int64_t*>(citizenRef + 0x10);

    int componentCount = *reinterpret_cast<int*>(citizenEntity + 0x38);
    int64_t componentArray = *reinterpret_cast<int64_t*>(citizenEntity + 0x30);

    int idx = 0;
    int64_t byteOff = 0;
    for (; idx < componentCount; idx++, byteOff += 8) {
        void* ibpAddr = reinterpret_cast<void*>(componentArray + byteOff);

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
        if (*reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(comp + 0x38) + 0x238) != 0) {
            int64_t wpEntity = *reinterpret_cast<int64_t*>(
                *reinterpret_cast<int64_t*>(comp + 0x38) + 0x238);

            int64_t findResult[2] = {};
            void* pResult = s_fn_FindComponentStorage(wpEntity, findResult);

            int64_t wpStorage = *reinterpret_cast<int64_t*>(pResult);
            int resourceCount = s_fn_GetResourceCount(wpStorage);

            if (resourceCount > 0) {
                int64_t wpRef = *reinterpret_cast<int64_t*>(
                    *reinterpret_cast<int64_t*>(comp + 0x38) + 0x238);
                s_fn_ReleaseWorkplace(
                    *reinterpret_cast<int64_t*>(comp + 0x38),
                    &wpRef);
            }
        }

        // Re-check health after potentially releasing workplace
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
        citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
        char canFindHouse = s_fn_CanFindHousing(citizenData);
        if (canFindHouse != 0 && *reinterpret_cast<int*>(comp + 0x28) != 2) {
            s_fn_TransitionState(reinterpret_cast<int64_t>(self), 1);
        }
    }

    // --- Phase 8: Check health status for education/age ---
    citizenData = *reinterpret_cast<int64_t*>(comp + 0x38);
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
// Callee reimpls (Component reg wave, batch "people")
// These are the exe helper bodies the call sites above resolve+call.
// Ported faithfully from Ghidra; sub-deps resolved via guarded ResolveAddr
// (same pattern as the rest of code/Game). Registered in reg_component_people.cpp.
// ============================================================

// Shared type-ID globals used by the storage scans / finders.
static uint32_t* s_TypeId_ResidenceComponent = nullptr; // DAT_1401c7cf0
static uint32_t* s_TypeId_RawMaterial = nullptr;         // DAT_1401c7cb8
static uint32_t s_tidResidence_local = 0;   // ENDGAME B.3.9 C1: namespace-scope, no lazy cache (B6-5)
static uint32_t s_tidRawMaterial_local = 0;

// The REAL StorageComponent finder (0x140006270, tid c7e78) — the exe's
// Storage_GetResourceCount (0x14012a9d0) calls THIS, not the misnamed
// 0x140008090 residence finder (disasm 0x14012a9ef: CALL 0x140006270).
static fn_FindComponentStorage_t s_fn_FindStorage6270 = nullptr;

// CheckAge sub-deps (CitizenComponent setup helpers).
typedef void (*fn_SetupYoung_t)(void* citizen);
typedef void (*fn_SetupAdult_t)(void* citizen);
typedef void (*fn_SetupElder_t)(void* citizen);
static fn_SetupYoung_t s_fn_SetupYoung = nullptr;
static fn_SetupAdult_t s_fn_SetupAdult = nullptr;
static fn_SetupElder_t s_fn_SetupElder = nullptr;

// TransitionState sub-deps.
typedef char (*fn_ValidateEnterState_t)(void* self, int state);
typedef void (*fn_BuildCandidateList_t)(void* self, int64_t* outList, char param);
static fn_ValidateEnterState_t s_fn_ValidateEnterState = nullptr;
static fn_BuildCandidateList_t s_fn_BuildCandidateList = nullptr;

namespace Object { uint32_t GetTypeIdByName(const char*); }

static bool s_calleeResolved = false;
static void EnsureCallees() {
    if (s_calleeResolved) return;
    // ENDGAME B.3.9 (B39_RULES.md D5): 0x1401c7cf0 / 0x1401c7cb8 are registered
    // type-id cells (INVENTORY_C Table 1: ResidenceComponent, RawMaterialDescription),
    // read BY CELL as the class's static TypeId() instead of the raw exe address (the
    // Residence value is unconditionally overwritten again below by ::Update's
    // per-tick repoint; caching class statics into namespace-scope storage keeps
    // B6-5's no-lazy-first-use-cache rule).
    s_tidResidence_local = ResidenceComponent::TypeId();
    s_tidRawMaterial_local = RawMaterialDescription::TypeId();
    s_TypeId_ResidenceComponent = &s_tidResidence_local;
    s_TypeId_RawMaterial = &s_tidRawMaterial_local;
    s_fn_FindStorage6270 = reinterpret_cast<fn_FindComponentStorage_t>(
        ResolveAddr(Module::Exe, 0x140006270));
    s_fn_SetupYoung = reinterpret_cast<fn_SetupYoung_t>(ResolveAddr(Module::Exe, 0x140115110));
    s_fn_SetupAdult = reinterpret_cast<fn_SetupAdult_t>(ResolveAddr(Module::Exe, 0x1401151a0));
    s_fn_SetupElder = reinterpret_cast<fn_SetupElder_t>(ResolveAddr(Module::Exe, 0x140115230));
    s_fn_ValidateEnterState = reinterpret_cast<fn_ValidateEnterState_t>(ResolveAddr(Module::Exe, 0x14010f1f0));
    s_fn_BuildCandidateList = reinterpret_cast<fn_BuildCandidateList_t>(ResolveAddr(Module::Exe, 0x14010dca0));
    s_calleeResolved = true;
}

// ============================================================
// CitizenComponent age-mesh setup helpers (Component reg wave, batch B).
// 0x140115110 SetupYoung / 0x1401151a0 SetupAdult / 0x140115230 SetupElder.
// Each writes a uniform RGB scale (a .rdata float constant) into the citizen's
// MeshInstance colour-tint fields (+0x134/+0x138/+0x13c) and the citizen's index
// (self+0x34, as float) into +0x140. The MeshInstance is reached through the
// InternalBasePtr at citizen+0x278 (deref +0x68). CheckPtr both the IBP and the
// resolved local copy, exactly like the original. Closure: only CheckPtr (engine).
// Faithful ports; the only difference vs the exe is the scale constant address.
// ============================================================
// Byte-faithful scale constants (DAT_14017214c/12c/118). HARDCODED literals, not
// read via ResolveAddr(Module::Exe, .rdata) — that returns NULL in standalone, so
// the previous `s_pXScale ? *s_pXScale : 1.0f` fallback collapsed EVERY age-state
// to 1.0, meaning children (SetupElder) never shrank and the per-frame OnUpdate2
// age→scale switch silently overwrote any external scale with 1.0. The C++
// literals compile to the exact bit patterns: 1.0f=0x3f800000, 0.83f=0x3f547ae1,
// 0.66f=0x3f28f5c3.
static constexpr float kScaleYoung = 1.0f;   // grown / adult full-size
static constexpr float kScaleAdult = 0.83f;  // intermediate life-state
static constexpr float kScaleElder = 0.66f;  // child / small render

static void CitizenComp_SetupAgeMesh(void* citizenP, float scale) {
    uint8_t* citizen = reinterpret_cast<uint8_t*>(citizenP);
    int idx = *reinterpret_cast<int*>(citizen + 0x34);
    int64_t meshIBP = *reinterpret_cast<int64_t*>(citizen + 0x278);
    if (meshIBP == 0) return;                                  // no CharacterComponent
    int64_t mesh = *reinterpret_cast<int64_t*>(meshIBP + 0x68);
    if (mesh == 0) return;                                     // ModelComponent not live yet
    *reinterpret_cast<float*>(mesh + 0x134) = scale;
    *reinterpret_cast<float*>(mesh + 0x138) = scale;
    *reinterpret_cast<float*>(mesh + 0x13c) = scale;
    *reinterpret_cast<float*>(mesh + 0x140) = (float)idx;
#if defined(_DEBUG)
    static int s_setupLog = 0;
    if (s_setupLog < 24) {
        ++s_setupLog;
        int64_t desc = *reinterpret_cast<int64_t*>(citizen + 0x288);
        float thresh = desc ? *reinterpret_cast<float*>(desc + 0x44) : -1.0f;
        float age = *reinterpret_cast<float*>(citizen + 0x3c);
        int64_t state208 = *reinterpret_cast<int64_t*>(citizen + 0x208);
        fprintf(stderr, "[SetupAgeMesh] citizen=%p scale=%.2f colour=%d age=%.2f "
                "thresh=%.2f state208=%lld\n",
                (void*)citizen, scale, idx, age, thresh, (long long)state208);
        fflush(stderr);
    }
#endif
}

void CitizenComponent_SetupYoung_Reimpl(void* citizen) {
    CitizenComp_SetupAgeMesh(citizen, kScaleYoung);
}
void CitizenComponent_SetupAdult_Reimpl(void* citizen) {
    CitizenComp_SetupAgeMesh(citizen, kScaleAdult);
}
void CitizenComponent_SetupElder_Reimpl(void* citizen) {
    CitizenComp_SetupAgeMesh(citizen, kScaleElder);
}

// Reimpl of Entity_FindComponent_Storage (0x140008090):
// scan an entity's component list for the ResidenceComponent type (DAT_1401c7cf0)
// and construct an InternalBasePtr (param_2) to it (or null).
void* CitizenComp_Entity_FindComponent_Storage(int64_t entity, void* outIBP) {
    EnsureCallees();
    uint8_t* e = reinterpret_cast<uint8_t*>(entity);
    void* found = nullptr;
    int count = *reinterpret_cast<int*>(e + 0x38);
    int64_t arr = *reinterpret_cast<int64_t*>(e + 0x30);
    for (int i = 0; i < count; i++) {
        void** ibp = reinterpret_cast<void**>(arr + (int64_t)i * 8);
        int64_t comp = *reinterpret_cast<int64_t*>(ibp);
        uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
        if (tid == *s_TypeId_ResidenceComponent) {
            void** ibp2 = reinterpret_cast<void**>(arr + (int64_t)i * 8);
            void* resolved = *ibp2;
            if (fn_IsDerivedFrom(resolved, *s_TypeId_ResidenceComponent)) {
                found = resolved;
            }
            break;
        }
    }
    new (outIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    return outIBP;
}

// Reimpl of StorageComponent_QueryCount (0x14012cf10):
// O(n*m) inventory scan — sums (RawMaterialDescription.unitVolume * slotCount)
// across all inventory rows. Reads type via vtable+0x10, filters by DAT_1401c7cb8.
int CitizenComp_StorageComponent_QueryCount(int64_t self) {
    EnsureCallees();
    if (!self) return 0;   // defensive: callers may pass a null/unfound storage
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    int total = 0;
    int rowCount = *reinterpret_cast<int*>(s + 0x30);
    if (0 < rowCount) {
        int64_t off = 0;
        int row = 0;
        do {
            int64_t arr = *reinterpret_cast<int64_t*>(s + 0x28);
            int64_t inv = *reinterpret_cast<int64_t*>(arr + off);
            void* mat = nullptr;
            int n = *reinterpret_cast<int*>(inv + 0x30);
            for (int j = 0; j < n; j++) {
                int64_t* slot = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(inv + 0x28) + (int64_t)j * 8);
                uint32_t tid = reinterpret_cast<Object::Base*>(*slot)->Type();   // B.3.8 bridge (slot 2)
                if (tid == *s_TypeId_RawMaterial) {
                    void* obj = reinterpret_cast<void*>(*slot);
                    mat = fn_IsDerivedFrom(obj, *s_TypeId_RawMaterial) ? obj : nullptr;
                    break;
                }
            }
            row++;
            off += 0x10;
            int unitVol = mat ? *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(mat) + 0x48) : 0;
            int slotCount = *reinterpret_cast<int*>(arr + (off - 8));
            total += unitVol * slotCount;
        } while (row < *reinterpret_cast<int*>(s + 0x30));
    }
    return total;
}

// Reimpl of Storage_GetResourceCount (0x14012a9d0):
// walks to the owner entity's StorageComponent (finder 0x140006270 — NOT the
// 0x140008090 residence finder; disasm-verified) then sums (count - reserved)
// of inventory rows whose material has the EDIBLE flag (bit 5 at +0x5c).
// [MISPORT FIX 2026-07-19] This used to call the residence finder and read the
// ResidenceComponent's +0x28/+0x30 as inventory rows — which only "worked"
// because the controller state node at +0x28 was absent (inert factory) and the
// start-town food seed hacked rows into it. With the real residence state node
// wired (heat tick bring-up), +0x28 is the node vtable; the exe-faithful path
// reads the sibling StorageComponent.
int CitizenComp_Storage_GetResourceCount(int64_t self) {
    EnsureCallees();
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    int64_t outIBP = 0;
    if (s_fn_FindStorage6270)
        s_fn_FindStorage6270(*reinterpret_cast<int64_t*>(s + 0x10), &outIBP);
    int total = 0;
    if (outIBP == 0) return 0;   // guard: entity without a StorageComponent
    int rowCount = *reinterpret_cast<int*>(outIBP + 0x30);
    if (0 < rowCount) {
        int64_t off = 0;
        int row = 0;
        do {
            int64_t arr = *reinterpret_cast<int64_t*>(outIBP + 0x28);
            int64_t inv = *reinterpret_cast<int64_t*>(arr + off);
            void* mat = nullptr;
            int n = *reinterpret_cast<int*>(inv + 0x30);
            for (int j = 0; j < n; j++) {
                int64_t* slot = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(inv + 0x28) + (int64_t)j * 8);
                uint32_t tid = reinterpret_cast<Object::Base*>(*slot)->Type();   // B.3.8 bridge (slot 2)
                if (tid == *s_TypeId_RawMaterial) {
                    void* obj = reinterpret_cast<void*>(*slot);
                    mat = fn_IsDerivedFrom(obj, *s_TypeId_RawMaterial) ? obj : nullptr;
                    break;
                }
            }
            if (mat && (*reinterpret_cast<unsigned int*>(reinterpret_cast<uint8_t*>(mat) + 0x5c) >> 5 & 1) != 0) {
                total += (*reinterpret_cast<int*>(arr + 8 + off) - *reinterpret_cast<int*>(arr + 0xc + off));
            }
            row++;
            off += 0x10;
        } while (row < *reinterpret_cast<int*>(outIBP + 0x30));
    }
    return total;
}

// Reimpl of CitizenAIComponent_TransitionState (0x14010db90):
// state-machine transition. Tries the queued state (+0x2c); if invalid, builds a
// candidate list and enters the first state whose machine has >0 actions.
void CitizenComp_CitizenAIComponent_TransitionState(int64_t selfV, char param) {
    EnsureCallees();
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);
    int queued = *reinterpret_cast<int*>(self + 0x2c);
    char ok = 0;
    if (queued >= 0 && s_fn_ValidateEnterState) {
        ok = s_fn_ValidateEnterState(self, queued);
    }
    if (queued < 0 || ok == '\0') {
        int idx = 0;
        int64_t list[2] = {0, 0};  // {ptr, count(low32)}
        if (s_fn_BuildCandidateList) s_fn_BuildCandidateList(self, list, param);
        int* arr = reinterpret_cast<int*>(list[0]);
        int n = static_cast<int>(list[1]);
        int* p = arr;
        if (0 < n) {
            do {
                if (s_fn_ValidateEnterState) s_fn_ValidateEnterState(self, *p);
                int64_t cz = *reinterpret_cast<int64_t*>(self + 0x38);
                if (0 < *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(cz + 0x278) + 0x60) + 1) {
                    *reinterpret_cast<int*>(self + 0x28) = arr[(int64_t)idx * 3];
                    break;
                }
                idx++;
                p += 3;
            } while (idx < n);
        }
        fn_MemFreeStatic(arr);
    } else {
        *reinterpret_cast<int*>(self + 0x28) = *reinterpret_cast<int*>(self + 0x2c);
        *reinterpret_cast<int*>(self + 0x2c) = -1;
    }
}

// Reimpl of CitizenAIComponent_CheckEducation (0x14010f7a0):
// raises a citizen's education level toward the residence cap if storage allows.
void CitizenComp_CitizenAIComponent_CheckEducation(int64_t selfV) {
    EnsureCallees();
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);
    void* dataPtr = self + 0x38;
    int64_t res10 = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(dataPtr) + 0x10);
    int64_t res8 = 0;
    CitizenComp_Entity_FindComponent_Storage(res10, &res8);
    // [STANDALONE BRING-UP GUARDS] +0x1f0 = profession-list (assigned at gameplay,
    // not in Startup) and res8 = the residence storage IBP (null if the citizen has
    // no home yet). Both are null for a freshly-spawned standalone citizen; the
    // original derefs them unconditionally. Bail (no education update) until live.
    int64_t profList = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(dataPtr) + 0x1f0);
    if (profList == 0 || res8 == 0) return;
    int cap = *reinterpret_cast<int*>(profList + 0x50);
    if (*reinterpret_cast<int*>(res8 + 0x38) != cap) {
        int64_t d = *reinterpret_cast<int64_t*>(dataPtr);
        int64_t res10b = 0;
        CitizenComp_Entity_FindComponent_Storage(*reinterpret_cast<int64_t*>(d + 0x10), &res10b);
        int cnt = CitizenComp_StorageComponent_QueryCount(res10b);
        if (cnt < cap) {
            *reinterpret_cast<int*>(res8 + 0x38) = cap;
        }
    }
}

// Reimpl of CitizenAIComponent_CheckAge (0x14010f890):
// routes the citizen to young/adult/elder setup based on age vs description
// threshold (+0x44) and whether the adult flag (+0x208) is set.
void CitizenComp_CitizenAIComponent_CheckAge(int64_t selfV) {
    EnsureCallees();
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);
    void* dataPtr = self + 0x38;
    int64_t d = *reinterpret_cast<int64_t*>(dataPtr);
    float ageThresh = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(d + 0x288) + 0x44);
    float age = *reinterpret_cast<float*>(d + 0x3c);
    if (ageThresh <= age && *reinterpret_cast<int64_t*>(d + 0x208) == 0) {
        if (s_fn_SetupYoung) s_fn_SetupYoung(*reinterpret_cast<void**>(dataPtr));
        return;
    }
    if (*reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(dataPtr) + 0x208) != 0) {
        if (s_fn_SetupAdult) s_fn_SetupAdult(*reinterpret_cast<void**>(dataPtr));
        return;
    }
    if (s_fn_SetupElder) s_fn_SetupElder(*reinterpret_cast<void**>(dataPtr));
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
#include "Object/factory_registration.h"
#include <cstdlib>
#include <cstring>

// ------------------------------------------------------------------
// Phase 3 / Keystone D — CitizenAIComponent live-wiring (Brick 1, plumbing).
//
// CitizenAIComponent is the per-citizen decision loop (the SEPARATE, self-contained
// action-state subsystem — NOT the building-controller SM). Its Update sits at
// primary vtable +0x48 (slot 9), driven by the normal EntityList per-tick
// round-robin (same slot as GrowthComponent), and the body-init/Startup is at
// slot +0x20 (= the Ghidra-named "Serialize" 0x14010f920), dispatched at spawn by
// ComponentEntity_StartupComponents.
//
// This brick brings BOTH slots live but CRASH-SAFE + env-gated, exactly like the
// StateMachineComponent plumbing brick:
//   * slot +0x20 -> CitizenAIComponent_BodyInit: ctor field-init (0x14010fb70)
//     + the Startup find (0x14010f920) of the sibling CitizenComponent -> +0x38.
//   * slot +0x48 -> CitizenAIComponent_UpdateAdapter: lazy body-init + GUARD +
//     trace, then the real CitizenAIComponent_Update.
//
// The GUARD is the key safety: CitizenAIComponent_Update derefs the sibling
// CitizenComponent at +0x38, its entity(+0x10), and its CitizenDescription(+0x288)
// unconditionally. In standalone the sibling CitizenComponent is still INERT
// (zeroed body => +0x288==0), so the adapter bails until CitizenComponent goes
// live (Brick 2). What this proves: +0x48 routing + +0x20 body-init +
// citizen-find on the citizen tier, crash-safe under the autostart sim. Default
// (env off) => fully inert => byte-for-byte unchanged run.
// ------------------------------------------------------------------

// The original Startup (0x14010f920) finds the sibling CitizenComponent via the
// .rdata type-id global DAT_1401c7158. In standalone that global is 0xFFFFFFFF
// (RegisterType assigns it too early; the Phase-1 fill set only covers a few
// globals) AND filling it would un-dormant ~30 other CitizenComponent-find
// readers into still-inert components ([[project_phase2_keystone_b]] note). So we
// find SELF-CONTAINED via Object::GetTypeIdByName — the same real-CRC source the
// inert/live component factories use for their vtable+0x10 GetTypeId — which is
// guaranteed to match the spawned component's id without touching any global.
static void* CitizenAI_FindSiblingCitizen(void* ownerEntity) {
    if (!ownerEntity) return nullptr;
    // B.3.8 CIT (B6-5): the class's own record, no lazy first-use cache.
    uint32_t want = CitizenComponent::TypeId();
    if (want == 0 || want == 0xFFFFFFFF) return nullptr;  // type id not yet assigned
    uint8_t* e = reinterpret_cast<uint8_t*>(ownerEntity);
    int count = *reinterpret_cast<int*>(e + 0x38);
    int64_t arr = *reinterpret_cast<int64_t*>(e + 0x30);
    for (int i = 0; i < count; i++) {
        void** ibp = reinterpret_cast<void**>(arr + (int64_t)i * 8);
        void* comp = *ibp;
        if (!comp) continue;
        uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
        if (tid == want) {
            return fn_IsDerivedFrom(comp, want) ? comp : nullptr;
        }
    }
    return nullptr;
}

// ============================================================================
// ENDGAME B.3.8 CIT — CitizenAIComponent as a REAL class (CitizenAIComponent.h).
//
// Before: a hand-expanded COMPONENT_LIVE_FACTORY_S — a 48-slot
// CitizenAIComponent_LiveVtable filled at every create (slots 3 Serialize,
// 4 Startup, 9 Update), Inert_ScalarDtor in slot 0. After: the compiler's
// 16-slot vtable and a real ctor/dtor chain (B38_RULES D1, D2). Slot 3 is wired
// by construction: a class's table cannot lose its serializer.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/CitizenAIComponent.h"

// The private type-id cells CitizenAIComponent::Update repoints its lookups at.
static uint32_t s_caiTidStorage = 0, s_caiTidResidence = 0;
static std::atomic<int> s_caiForceWorkLog{0};   // the [ForceWork] trace cap


// 0x14010fb70. Component() is the CALL 0x1401188b0; the 0x140162470 stamp is
// the compiler's; flags |= 0x89. The sixteen System::Time() at +0x40 are zero.
// +0x28 = 0xc (aiState, 0x14010fba0) and +0x2c = -1 (pending state, 0x14010fba7)
// are the ctor's own stores; Startup (CitizenAIComponent_BodyInit) still
// re-writes them, as it did when the inert factory zero-filled them.
CitizenAIComponent::CitizenAIComponent()
    : aiState(0xc), targetId(-1), data30(0), _pad32{}, data38(0), timers{},
      dataC0(0), dataC8(0) {
    OrFlagsInCtor(0x89);
}

// [0] 0x14002fcf0: restamp + ~Internal (no own work), MemFree on &1.
Object::Base* CitizenAIComponent::DeletingDtor(unsigned flags) {
    this->~CitizenAIComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// Recipe step 2 (body-init), routed to slot +0x20 (StartupComponents dispatch).
// Faithful field init from the ctor 0x14010fb70 (the inert factory zeroes the
// body, so the ctor-set scalars must be re-applied) + the Startup find from
// 0x14010f920 (sibling CitizenComponent -> +0x38). Idempotent.
void CitizenAIComponent::Startup() {
    uint8_t* base = reinterpret_cast<uint8_t*>(this);

    // ctor (0x14010fb70) scalar field init. Flags(0x89) + the 16 Time timers
    // (+0x40..+0xBF) + +0xC0/+0xC8 are already zeroed/flagged by the inert factory.
    *reinterpret_cast<uint32_t*>(base + 0x28) = 0xc;   // aiState = Leisure (initial)
    *reinterpret_cast<int32_t*>(base + 0x2c) = -1;     // pending state = none
    *reinterpret_cast<uint16_t*>(base + 0x30) = 0;

    // Startup (0x14010f920): find sibling CitizenComponent -> +0x38.
    void* owner = *reinterpret_cast<void**>(base + 0x10);
    void* citizen = owner ? CitizenAI_FindSiblingCitizen(owner) : nullptr;
    *reinterpret_cast<void**>(base + 0x38) = citizen;
}

// [6] 0x14010f880 (`JMP 0x14010f890` CheckAge). PORT GAP (retail 0x14010f880) —
// B38 D9: the tree's table held the inert stub here (CheckAge is reached from
// Update), kept.
void CitizenAIComponent::OnAdded() {}

// [9] 0x14010f3e0. RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af).
// Recipe step 1 (Update), routed to slot +0x48. Signature matches the macro's
// UpdateVSlot adapter: (self, int64_t* time).
void CitizenAIComponent::Update(void* timeArg) {
    int64_t* time = static_cast<int64_t*>(timeArg);
    uint8_t* base = reinterpret_cast<uint8_t*>(this);

    EnsureHelpers();
    EnsureCallees();

    // Repoint the type-id globals the Update path derefs to PRIVATE filled copies.
    // The shared .rdata globals StorageComponent(0x1401c7e78) and
    // ResidenceComponent(0x1401c7cf0) are unregistered in standalone => their
    // ResolveAddr returns null => Update's home-storage scan derefs null
    // (crash at line ~149). Filling the shared globals would un-dormant their many
    // readers, so keep them this-contained via GetTypeIdByName (same real-CRC the
    // component factories use). (RawMaterial 0x1401c7cb8 IS already filled.)
    // B.3.8 (spelling 5, B6-5): the function-local lazy by-name caches became the
    // class statics X::TypeId(), copied into namespace-scope cells each call.
    s_caiTidStorage   = StorageComponent::TypeId();
    s_caiTidResidence = ResidenceComponent::TypeId();
    s_pStorageTypeId            = &s_caiTidStorage;
    s_TypeId_ResidenceComponent = &s_caiTidResidence;

    // Lazy body-init: until the sibling CitizenComponent is found at +0x38, retry
    // the find each tick (it may not have been constructed yet at Startup time).
    // Re-running ctor-init while inert is harmless (the guard below bails before
    // Update); once +0x38 != 0 this never runs again.
    if (*reinterpret_cast<void**>(base + 0x38) == nullptr)
        CitizenAIComponent::Startup();   // the body-init, qualified (was the free call)

    void* citizen = *reinterpret_cast<void**>(base + 0x38);
    void* czEntity = nullptr;
    void* czDesc = nullptr;
    void* czChar = nullptr;
    if (citizen) {
        uint8_t* cz = reinterpret_cast<uint8_t*>(citizen);
        czEntity = *reinterpret_cast<void**>(cz + 0x10);
        czDesc   = *reinterpret_cast<void**>(cz + 0x288);
        czChar   = *reinterpret_cast<void**>(cz + 0x278);  // CharacterComponent (alive flag)
    }

    // --- FORCE-WORK HARNESS (env BANISHED_FORCE_WORK, default off) -------------
    // The town-manager building registry is empty in standalone, so AssignHousing/
    // FindWorkplace never find a residence/job -> the citizen stays unhoused +
    // unemployed -> Work (id 5) is never a CitizenAI candidate (see the housed-gate
    // in citizenai_candidatelist.cpp:374). To exercise the Work execution path
    // (State_Work -> db20 -> NavigateTo -> executor -> PerformWork) BEFORE that
    // registry is brought up, directly point ONE citizen at a real workplace
    // building (the first start-town StorageBarn, captured in starttown.cpp):
    //   +0x238 = workplace entity (inline InternalBasePtr)   -> State_Work target
    //   +0x68  = housed flag (non-null)                      -> makes Work a candidate
    // One-shot per citizen (only seeds while +0x238 is still empty).
    if (citizen) {
        const char* e = Opt().forceWork;   // read per call: Options are fixed at boot (B6-5)
        const int s_force = (e && e[0] && e[0] != '0') ? 1 : 0;
        if (s_force == 1 && g_pForceWorkBuilding) {
            uint8_t* cz = reinterpret_cast<uint8_t*>(citizen);
            if (*reinterpret_cast<int64_t*>(cz + 0x238) == 0) {
                if (fn_InternalBasePtr_Ctor)
                    new (cz + 0x238) ::Object::InternalBasePtr(
                        reinterpret_cast<RtObject::Internal*>(g_pForceWorkBuilding));   // B.3.3 bridge
                else
                    *reinterpret_cast<void**>(cz + 0x238) = g_pForceWorkBuilding;
                *reinterpret_cast<void**>(cz + 0x68) = g_pForceWorkBuilding;  // housed flag
                if (s_caiForceWorkLog.fetch_add(1, std::memory_order_relaxed) < 6) {
                    fprintf(stderr, "[ForceWork] citizen=%p workplace+0x238=%p housed+0x68=%p\n",
                            (void*)citizen, g_pForceWorkBuilding, g_pForceWorkBuilding);
                    fflush(stderr);
                }
            }
        }
    }

    // --- PRODUCTION HARNESS (env BANISHED_LIVE_PRODUCTION, default off) --------
    // Phase B of the economy WORK-half: drive ONE faithful log-production accounting
    // event at the force-worked ForesterLodge and prove the town stock count
    // increments. One-shot (this-gated inside). See production_harness.cpp.

    // GUARD (crash-safe): Update derefs citizen / its entity(+0x10) / its
    // CitizenDescription(+0x288) / its CharacterComponent(+0x278, Phase-8 alive
    // check) unconditionally. Bail until all are live (Brick 2 populates them).
    if (!citizen || !czEntity || !czDesc || !czChar) return;

    // --- FORCE-FORESTERWORK DRIVER (env BANISHED_FORCE_FORESTERWORK, default off) ----
    // Drive ONE force-worked citizen as a forester via its action stack, replacing the
    // inert building ResourceProduceComponent controller. When it owns the citizen, skip
    // the competing AI state machine (Leisure/Wander would otherwise claim the idle stack
    // first) so ForesterWork::Execute keeps driving the real harvest/production chain.
    if (StartTown_ForcePushForesterWork(citizen)) return;
    if (StartTown_ForcePushConstructionWork(citizen)) return;

    CitizenAIComponent_Update(this, time);
}

// [15] 0x14010f340 (150 B). PORT GAP (retail 0x14010f340) — B38 D9: not ported;
// the tree's table held the inert stub here, kept.
void CitizenAIComponent::OnMessage(int, void*, uint32_t, uint32_t) {}

// create 0x14002fb20: MemAlloc(0xd0, 8), then the ctor.
void* CitizenAIComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CitizenAIComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CitizenAIComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* CitizenAIComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CitizenAIComponent());
}

DEFINE_EXE_VTABLE(CitizenAIComponent, "_ZTV18CitizenAIComponent")
DEFINE_EXE_TYPE(CitizenAIComponent, "Component")
