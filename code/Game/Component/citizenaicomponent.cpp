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
#include "Component/CitizenDescription.h" // B.4 W1 COMP-b: description fields by name
#include "Component/CharacterComponent.h"
#include "Component/AttendanceComponent.h"
#include "Game/Profession.h"
#include "Component/ResidenceComponent.h" // B.3.8 CB1a (TypeId)
#include "Component/StorageComponent.h"   // B.3.8 CB1a (TypeId)
#include "options.h"
#include <new>
#include "object/TypeInfo.h"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors
#include "Entity/ComponentEntity.h"   // ComponentEntity::FindComponent<T>

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
// Exe helpers, called by name (Component/Work exe_bodies.h):
//   StorageComponent_QueryCount 0x14012cf10, Entity_FindComponent_Storage 0x140008090,
//   Storage_GetResourceCount 0x14012a9d0, CitizenComponent_ReleaseWorkplace 0x1401152e0,
//   CitizenComponent_CanFindHousing 0x140115be0, CitizenAIComponent_TransitionState
//   0x14010db90, _CheckEducation 0x14010f7a0, _CheckAge 0x14010f890.
// ============================================================

// Shared type-id cells (defined below with the callee reimpls).
static void EnsureCallees();

// ============================================================
// CitizenAIComponent::Update (vtable slot [9], 0x14010f3e0) -- the body
// ============================================================
// Main citizen AI tick. RDX = pointer to the elapsed System::Time (int64 ticks).
// timers[14] / timers[15] (+0xB0 / +0xB8) are the cooldowns of states 0xe / 0xf.
void CitizenAIComponent::Decide(const int64_t* time) {
    int64_t dt = *time;

    // --- Phase 1: count the 16 per-state timers down, clamped at 0 ---
    for (int i = 0; i < 16; i++) {
        timers[i] -= dt;
        if (timers[i] < 0) {
            timers[i] = 0;
        }
    }

    // --- Phase 2: the two accumulators ---
    leisureTimer += dt;
    dataC8 += dt;

    // --- Phase 3: the citizen's own StorageComponent (what it carries) ---
    StorageComponent* storageComp = citizen->_entity->FindComponent<StorageComponent>();

    // Use inline fn_StorageComponent_QueryCount (from engine_funcs.h) instead of
    // the exe's StorageComponent_QueryCount (0x14012cf10). The exe version crashes
    // when a material type lacks RawMaterialDescription (NULL+0x48 deref).
    // Our inline version has a null guard on the component lookup.
    int storageCount = 0;
    if (storageComp) {
        storageCount = fn_StorageComponent_QueryCount(storageComp);
    }
    if (storageCount == 0) {
        dataC8 = 0;
    }

    // --- Phase 4: the seeking flags ---
    uint8_t hungerFlag = seekingFood;
    uint8_t thirstFlag = seekingWarmth;
    int aiState = state;

    bool isInNeedState = false;
    if ((hungerFlag != 0 && aiState == 0x0E) || (thirstFlag != 0 && aiState == 0x0F)) {
        isInNeedState = true;
    }

    // --- Phase 5: hunger ---
    CitizenComponent* citizenData = citizen;
    if (!citizenData) return;  // citizen entity gone, bail out
    float health = citizenData->hunger;
    float* pHungerThresh = &citizenData->description->_fcc;

    if (health <= *pHungerThresh || health == *pHungerThresh || isInNeedState ||
        ((uint32_t)(aiState - 0x0E) < 2) ||
        timers[14] > 0) {
        // Not hungry enough to transition, or in special state
        if (hungerFlag != 0) {
            citizenData = citizen;
            health = citizenData->hunger;
            pHungerThresh = &citizenData->description->_fcc;

            if (health < *pHungerThresh || health == *pHungerThresh) {
                seekingFood = 0;
            }
        }
    } else {
        // Hungry: eat at home first when the home has food
        if (citizen->home != nullptr) {
            ResidenceComponent* homeStorage = citizen->home->FindComponent<ResidenceComponent>();
            int resourceCount = CitizenComp_Storage_GetResourceCount(reinterpret_cast<int64_t>(homeStorage));

            if (resourceCount > 0) {
                citizen->EatFrom(citizen->home);
            }
        }

        // Re-check hunger after eating
        citizenData = citizen;
        health = citizenData->hunger;
        pHungerThresh = &citizenData->description->_fcc;

        if (*pHungerThresh <= health && health != *pHungerThresh) {
            TransitionState(1);
            seekingFood = (state == 0x0E) ? 1 : 0;
        }
    }

    // --- Phase 6: coldness ---
    citizenData = citizen;
    float hydration = citizenData->coldness;
    float* pThirstThresh = &citizenData->description->_fec;

    // Re-read flags (may have been modified above)
    thirstFlag = seekingWarmth;
    aiState = state;
    isInNeedState = ((seekingFood != 0 && aiState == 0x0E) ||
                     (thirstFlag != 0 && aiState == 0x0F));

    if (hydration <= *pThirstThresh || hydration == *pThirstThresh || isInNeedState ||
        ((uint32_t)(aiState - 0x0E) < 2) ||
        timers[15] > 0) {
        // Not cold enough or in special state
        if (thirstFlag != 0) {
            citizenData = citizen;
            hydration = citizenData->coldness;
            pThirstThresh = &citizenData->description->_fec;

            if (hydration < *pThirstThresh || hydration == *pThirstThresh) {
                seekingWarmth = 0;
            }
        }
    } else {
        // Cold: transition to the warming state
        TransitionState(1);
        seekingWarmth = (state == 0x0F) ? 1 : 0;
    }

    // --- Phase 7: housing check ---
    if (seekingFood == 0 && seekingWarmth == 0) {
        char canFindHouse = citizen->CanFindHousing();
        if (canFindHouse != 0 && state != 2) {
            TransitionState(1);
        }
    }

    // --- Phase 8: an action is running -> nothing more to decide ---
    if (citizen->character->currentAction + 1 > 0) {
        return;
    }

    // The action stack is empty: schooling, the age mesh, and a new state
    CheckEducation();
    CheckAge();
    TransitionState(0);
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

// CheckAge sub-deps (CitizenComponent setup helpers).

// TransitionState sub-deps.


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

static void CitizenComp_SetupAgeMesh(CitizenComponent* citizen, float scale) {
    int idx = citizen->field_0x34;
    CharacterComponent* meshIBP = citizen->character;
    if (meshIBP == nullptr) return;                            // no CharacterComponent
    // The ModelComponent's MeshInstance is raw storage (its owner's H5 member): the
    // colour-tint fields inside it are written raw.
    uint8_t* mesh = reinterpret_cast<uint8_t*>(meshIBP->modelComponent);
    if (mesh == nullptr) return;                               // ModelComponent not live yet
    *reinterpret_cast<float*>(mesh + 0x134) = scale;
    *reinterpret_cast<float*>(mesh + 0x138) = scale;
    *reinterpret_cast<float*>(mesh + 0x13c) = scale;
    *reinterpret_cast<float*>(mesh + 0x140) = (float)idx;
#if defined(_DEBUG)
    static int s_setupLog = 0;
    if (s_setupLog < 24) {
        ++s_setupLog;
        CitizenDescription* desc = citizen->description;
        float thresh = desc ? desc->adultAge : -1.0f;
        float age = citizen->age;
        AttendanceComponent* state208 = citizen->school;
        fprintf(stderr, "[SetupAgeMesh] citizen=%p scale=%.2f colour=%d age=%.2f "
                "thresh=%.2f state208=%p\n",
                (void*)citizen, scale, idx, age, thresh, (void*)state208);
        fflush(stderr);
    }
#endif
}

void CitizenComponent::SetupYoung() {
    CitizenComp_SetupAgeMesh(this, kScaleYoung);
}
void CitizenComponent::SetupAdult() {
    CitizenComp_SetupAgeMesh(this, kScaleAdult);
}
void CitizenComponent::SetupElder() {
    CitizenComp_SetupAgeMesh(this, kScaleElder);
}

// Reimpl of Entity_FindComponent_Storage (0x140008090):
// scan an entity's component list for the ResidenceComponent type (DAT_1401c7cf0)
// and construct an InternalBasePtr (param_2) to it (or null).
void* CitizenComp_Entity_FindComponent_Storage(int64_t entity, void* outIBP) {
    return new (outIBP) ::Object::InternalPtr<ResidenceComponent>(
        reinterpret_cast<ComponentEntity*>(entity)->FindComponent<ResidenceComponent>());   // B4 FORWARD: receiver typed by the callers' blocks
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
                    mat = Object::IsDerivedFromNullSafe(obj, *s_TypeId_RawMaterial) ? obj : nullptr;
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
    WorkBase_FindComponent_6270(*reinterpret_cast<int64_t*>(s + 0x10), &outIBP);
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
                    mat = Object::IsDerivedFromNullSafe(obj, *s_TypeId_RawMaterial) ? obj : nullptr;
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

// CitizenAIComponent::TransitionState (0x14010db90):
// state-machine transition. Tries the queued state; if invalid, builds a
// candidate list and enters the first state whose handler started an action.
void CitizenAIComponent::TransitionState(char param) {
    EnsureCallees();
    int queued = queuedState;
    char ok = 0;
    if (queued >= 0) {
        ok = ValidateAndEnterState(queued);
    }
    if (queued < 0 || ok == '\0') {
        int idx = 0;
        int64_t list[2] = {0, 0};  // {ptr, count(low32)}
        BuildCandidateStateList(list, param);
        int* arr = reinterpret_cast<int*>(list[0]);
        int n = static_cast<int>(list[1]);
        int* p = arr;
        if (0 < n) {
            do {
                ValidateAndEnterState(*p);
                if (0 < citizen->character->currentAction + 1) {
                    state = arr[(int64_t)idx * 3];
                    break;
                }
                idx++;
                p += 3;
            } while (idx < n);
        }
        System::MemoryAllocator::MemFree(arr);
    } else {
        state = queuedState;
        queuedState = -1;
    }
}

// CitizenAIComponent::CheckEducation (0x14010f7a0):
// raises a capacity toward the citizen's profession value if the storage allows.
// (Retail 0x14010f7e1 / 0x14010f830 call the StorageComponent finder 0x140006270;
// the tree calls the residence finder 0x140008090 -- kept, listed in the hand-in.)
void CitizenAIComponent::CheckEducation() {
    EnsureCallees();
    ResidenceComponent* res8 = citizen->_entity->FindComponent<ResidenceComponent>();
    // [STANDALONE BRING-UP GUARDS] the profession (assigned at gameplay,
    // not in Startup) and res8 = the residence storage (null if the citizen has
    // no home yet). Both are null for a freshly-spawned standalone citizen; the
    // original derefs them unconditionally. Bail (no education update) until live.
    Profession* profList = citizen->profession;
    if (profList == nullptr || res8 == nullptr) return;
    int cap = profList->priority;
    // +0x38 lies inside ResidenceComponent's embedded controller: raw.
    int* res8Cap = reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(res8) + 0x38);
    if (*res8Cap != cap) {
        ResidenceComponent* res10b = citizen->_entity->FindComponent<ResidenceComponent>();
        int cnt = CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>(res10b));
        if (cnt < cap) {
            *res8Cap = cap;
        }
    }
}

// CitizenAIComponent::CheckAge (0x14010f890):
// routes the citizen to the young/adult/elder mesh setup from its age against the
// description's adultAge and whether it attends a school.
void CitizenAIComponent::CheckAge() {
    EnsureCallees();
    CitizenComponent* d = citizen;
    float ageThresh = d->description->adultAge;
    float age = d->age;
    if (ageThresh <= age && d->school == nullptr) {
        citizen->SetupYoung();
        return;
    }
    if (citizen->school != nullptr) {
        citizen->SetupAdult();
        return;
    }
    citizen->SetupElder();
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
static CitizenComponent* CitizenAI_FindSiblingCitizen(ComponentEntity* ownerEntity) {
    if (!ownerEntity) return nullptr;
    // B.3.8 CIT (B6-5): the class's own record, no lazy first-use cache.
    uint32_t want = CitizenComponent::TypeId();
    if (want == 0 || want == 0xFFFFFFFF) return nullptr;  // type id not yet assigned
    for (int i = 0; i < ownerEntity->_componentCount; i++) {
        Component* comp = ownerEntity->_components[i].Get();
        if (!comp) continue;
        if (comp->Type() == want) {
            return comp->IsDerivedFrom(want) ? static_cast<CitizenComponent*>(comp) : nullptr;
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
static uint32_t s_caiTidResidence = 0;
static std::atomic<int> s_caiForceWorkLog{0};   // the [ForceWork] trace cap


// 0x14010fb70. Component() is the CALL 0x1401188b0; the 0x140162470 stamp is
// the compiler's; flags |= 0x89. The sixteen System::Time() at +0x40 are zero.
// +0x28 = 0xc (state, 0x14010fba0) and +0x2c = -1 (queued state, 0x14010fba7)
// are the ctor's own stores; Startup (CitizenAIComponent_BodyInit) still
// re-writes them, as it did when the inert factory zero-filled them.
CitizenAIComponent::CitizenAIComponent()
    : state(0xc), queuedState(-1), seekingFood(false), seekingWarmth(false), _pad32{},
      citizen(nullptr), timers{}, leisureTimer(0), dataC8(0) {
    OrFlagsInCtor(0x89);
}

// Recipe step 2 (body-init), routed to slot +0x20 (StartupComponents dispatch).
// Faithful field init from the ctor 0x14010fb70 (the inert factory zeroes the
// body, so the ctor-set scalars must be re-applied) + the Startup find from
// 0x14010f920 (sibling CitizenComponent -> +0x38). Idempotent.
void CitizenAIComponent::Startup() {
    // ctor (0x14010fb70) scalar field init. Flags(0x89) + the 16 Time timers
    // (+0x40..+0xBF) + +0xC0/+0xC8 are already zeroed/flagged by the inert factory.
    state = 0xc;          // Leisure (initial)
    queuedState = -1;     // none
    seekingFood = false;  // the ctor's one word store at +0x30, as two (H7)
    seekingWarmth = false;

    // Startup (0x14010f920): find sibling CitizenComponent -> +0x38.
    ComponentEntity* owner = _entity;
    citizen = owner ? CitizenAI_FindSiblingCitizen(owner) : nullptr;
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
    const int64_t* time = static_cast<const int64_t*>(timeArg);

    EnsureCallees();

    // B.3.8 (spelling 5, B6-5): the function-local lazy by-name caches became the
    // class statics X::TypeId(), copied into namespace-scope cells each call.
    s_caiTidResidence = ResidenceComponent::TypeId();
    s_TypeId_ResidenceComponent = &s_caiTidResidence;

    // Lazy body-init: until the sibling CitizenComponent is found at +0x38, retry
    // the find each tick (it may not have been constructed yet at Startup time).
    // Re-running ctor-init while inert is harmless (the guard below bails before
    // Update); once +0x38 != 0 this never runs again.
    if (citizen == nullptr)
        CitizenAIComponent::Startup();   // the body-init, qualified (was the free call)

    ComponentEntity* czEntity = nullptr;
    CitizenDescription* czDesc = nullptr;
    CharacterComponent* czChar = nullptr;
    if (citizen) {
        czEntity = citizen->_entity;
        czDesc   = citizen->description;
        czChar   = citizen->character;  // CharacterComponent (alive flag)
    }

    // --- FORCE-WORK HARNESS (env BANISHED_FORCE_WORK, default off) -------------
    // The town-manager building registry is empty in standalone, so AssignHousing/
    // FindWorkplace never find a residence/job -> the citizen stays unhoused +
    // unemployed -> Work (id 5) is never a CitizenAI candidate (see the housed-gate
    // in citizenai_candidatelist.cpp:374). To exercise the Work execution path
    // (State_Work -> db20 -> NavigateTo -> executor -> PerformWork) BEFORE that
    // registry is brought up, directly point ONE citizen at a real workplace
    // building (the first start-town StorageBarn, captured in starttown.cpp):
    //   +0x238 = home (inline InternalBasePtr)              -> State_Work target
    //   +0x68  = the Disease* slot (non-null)               -> makes Work a candidate
    // One-shot per citizen (only seeds while +0x238 is still empty).
    if (citizen) {
        const char* e = Opt().forceWork;   // read per call: Options are fixed at boot (B6-5)
        const int s_force = (e && e[0] && e[0] != '0') ? 1 : 0;
        if (s_force == 1 && g_pForceWorkBuilding) {
            if (citizen->home == nullptr) {
                new (&citizen->home) ::Object::InternalBasePtr(
                        reinterpret_cast<RtObject::Internal*>(g_pForceWorkBuilding));   // B.3.3 bridge
                // the harness plants a non-disease pointer in the Disease* slot
                *reinterpret_cast<void**>(&citizen->disease) = g_pForceWorkBuilding;
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

    Decide(time);
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

DEFINE_EXE_TYPE(CitizenAIComponent, "Component")
