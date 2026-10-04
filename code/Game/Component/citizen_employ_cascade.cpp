// code/Game/Component/citizen_employ_cascade.cpp
//
// Brick M-C — the ProfessionUI worker-count SEAT cascade + the town citizen-list
// registrar. Faithful 1:1 ports of:
//   0x140114000  CitizenComponent_OnAdd          — townMgr+0xa0 citizen-list ADD
//   0x1400896a0  FUN_1400896a0                   — Fisher-Yates shuffle of the house list
//   0x14008cbf0  CitizenComponent_impl_sub_cbf0  — strategy 1 (distance-optimized relocate)
//   0x14008ca20  CitizenComponent_impl_sub_ca20  — strategy 2 (nearest employed laborer)
//   0x14008c5f0  CitizenComponent_impl_c5f0      — strategy 3 (most-laborer house)
//   0x14008c4b0  CitizenComponent_impl_c4b0      — strategy 4 (first idle laborer)
// plus a headless driver CitizenEmploy_SeatWorkers that mirrors the per-worker inner
// loop of CitizenComponent_impl_dd10 (0x14007dd10) — dd10 reads a NumberEditElement UI
// widget we don't have, so we drive the FindAvailableBuilding + cbf0->ca20->c5f0->c4b0
// fallback directly (as if the player set the worker count).
//
// CORRECTED field semantics (reference_employ_cascade_field_semantics): each strategy
// seats a worker by setting citizen+0x1f0 = professionKey and calling AssignHousing
// (0x140111ee0), which sets citizen+0x248 (the WorkPlaceComponent-roster / workplace
// link) — NOT +0x238 (that is the c7cf0 residence/household link, set by AssignWorkplace).
//
// Deps already ported+registered elsewhere:
//   FindAvailableBuilding 0x14008d100  (town_workplace_registry.cpp)
//   CitizenComp_AssignHousing 0x140111ee0  (citizencomponent.cpp) -> +0x248
//   CitizenComp_Entity_FindComponent_Storage 0x140008090  (citizenaicomponent.cpp) -> c7cf0
//   Entity_impl_7fb0 0x140087fb0, CalcDistance 0x140008830, arrayAdd 0x140003110

#include "Component/ResidenceComponent.h"   // B.3.8 CB1a (TypeId)
#include <new>
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Component/coll_bridge.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/CitizenComponent.h"
#include "Component/CitizenDescription.h"   // adultAge (+0x44)
#include "Entity/CitizenBehavior.h"         // the town manager's arrays (+0x20, +0xA0)
#include "Entity/ComponentEntity.h"         // _components / FindComponent<T>
#include "Game/ProfessionList.h"            // field_0x30 (the laborer profession)
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"   // Pf_EntityAnchorCell_7fb0 (0x140087fb0)
#include "Work/exe_bodies.h"     // WorkBase_CalcDistance_8830 (0x140008830)
#include "Game/exe_bodies.h"     // FindAvailableBuilding (0x14008d100)

// Deps ported elsewhere, called by name through their headers:
//   0x140087fb0 Pf_EntityAnchorCell_7fb0 — an entity IBP's anchor cell (used for distance).
//   0x140008830 WorkBase_CalcDistance_8830 — distance between two cells (game = *(gw+0xf8)).
//   0x1401163d0 CitizenComponent_AssignWorkplace(self, workplaceIBP, slotIndex).
//     Passing &nullIBP,-1 detaches the citizen from any workplace worker list and clears
//     citizen+0x238 (workplace) / +0x240 (slot) — the "become an unassigned laborer" clear.
//   0x140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
//   into the exe (ENDGAME B.3.2 step 2) — converted at its call sites below.

namespace {

uint32_t          s_tidResidence = 0;   // ResidenceComponent (c7cf0) — self-resolved
bool              s_init         = false;

// Resolves no door: it only caches the ResidenceComponent type id.
void EnsureFns() {
    if (s_init) return;
    if (s_tidResidence == 0) s_tidResidence = ResidenceComponent::TypeId();
    s_init = true;
}

// The default (laborer) profession: *(gw+0xb0)+0x30, re-read at every use.
inline Profession* LaborerKey()  { return GameUpdateLoop::_singleton->m_professionList->field_0x30; }

// citizen adult (desc+0x44 <= cz+0x3c) && idle (cz+0x208 == 0). No CheckPtr (caller
// already CheckPtr'd; the exe reads these two fields before the mid-check CheckPtr).
inline bool AdultIdle(CitizenComponent* cz) {
    return (cz->description->adultAge <= cz->age)
        && (cz->school == nullptr);
}

// Find the ResidenceComponent (c7cf0) on an entity — mirrors the exe's inline scan
// (stop at first GetTypeId match; return it only if IsDerivedFrom too, else null).
ResidenceComponent* FindResidenceComp(ComponentEntity* entity) {
    int i = 0;
    Object::InternalPtr<Component>* arr = entity->_components;   // +0x30
    int cnt = entity->_componentCount;                            // +0x38
    while (i < cnt) {
        Component* comp = arr[i].Get();
        uint32_t tid = comp->Type();   // slot 2
        if (tid == s_tidResidence) {
            Component* compVal = arr[i].Get();
            return Object::IsDerivedFromNullSafe(compVal, s_tidResidence)
                       ? static_cast<ResidenceComponent*>(compVal) : nullptr;
        }
        ++i;
        cnt = entity->_componentCount;   // faithful re-read
    }
    return nullptr;
}

// Seat epilogue shared by all four strategies: if the citizen isn't already this
// profession at this building, AssignHousing(citizen,&building) (-> +0x248) and set
// +0x1f0 = profKey. `avail` = the workplace building IBP value.
inline void SeatWorker(CitizenComponent* citizen, ComponentEntity* avail, Profession* profKey) {
    ComponentEntity* bldg = avail;
    if ((citizen->profession != profKey) ||
        (citizen->workplace != bldg)) {
        citizen->SetWorkplace(bldg);   // 0x140111ee0
        citizen->profession = profKey;
    }
}

// ============================================================================
// 0x1400896a0 — Fisher-Yates shuffle of the town house list (townMgr+0x20 array,
// townMgr+0x28 count). Uses System::Random::Rand(*(gw+0xd8), n).
// ============================================================================
void Shuffle896a0(CitizenBehavior* townMgr) {
    uint32_t n = townMgr->field_0x28;
    System::Random* rng = GameUpdateLoop::_singleton->m_random;   // +0xd8
    if ((int)n <= 1) return;
    for (int64_t i = 0; (int64_t)i < (int)n; ++i) {
        uint32_t j = rng->Rand(static_cast<uint32_t>(n));
        if ((uint32_t)i != j) {
            ComponentEntity** base = townMgr->field_0x20;
            ComponentEntity* tmp = base[i];
            base[i] = base[(int)j];
            base[(int)j] = tmp;
        }
    }
}

// ============================================================================
// 0x14008c4b0 — strategy 4: first adult+idle+laborer in townMgr+0xa0 -> seat.
// ============================================================================
CitizenComponent** Strat_c4b0(CitizenBehavior* townMgr, CitizenComponent** out,
                              ComponentEntity** availPtr, Profession* profKey) {
    int cnt = townMgr->citizenCount;
    for (int i = 0; i < cnt; ++i) {
        CitizenComponent* cz = townMgr->citizens[i];
        if (AdultIdle(cz)) {
            if (cz->profession == LaborerKey()) {
                SeatWorker(cz, *availPtr, profKey);
                *out = cz;
                return out;
            }
        }
    }
    *out = nullptr;
    return out;
}

// ============================================================================
// 0x14008ca20 — strategy 2: nearest (to avail) adult+idle laborer that already HAS
// a residence (+0x238) -> seat.
// ============================================================================
CitizenComponent** Strat_ca20(CitizenBehavior* townMgr, CitizenComponent** out,
                              ComponentEntity** availPtr, Profession* profKey) {
    int* game = reinterpret_cast<int*>(GameUpdateLoop::_singleton->m_game);   // LEFT: SIG WorkBase_CalcDistance_8830 (int* map; Work/exe_bodies.h, wave X)
    ComponentEntity* availAnchorArg = *availPtr;
    uint32_t availCell = (uint32_t)EntityAnchorCell(&availAnchorArg, nullptr);   // 0x140087fb0
    int bestDist = 0x7ffffff;
    CitizenComponent* best = nullptr;
    int cnt = townMgr->citizenCount;
    for (int i = 0; i < cnt; ++i) {
        CitizenComponent* cz = townMgr->citizens[i];
        if (AdultIdle(cz)) {
            if (cz->profession == LaborerKey()) {
                if (cz->home != nullptr) {
                    ComponentEntity* resAnchor = cz->home;   // +0x238
                    uint32_t czCell = (uint32_t)EntityAnchorCell(&resAnchor, nullptr);
                    int d = WorkBase_CalcDistance_8830(game, czCell, availCell);
                    if (d <= bestDist) { best = cz; bestDist = d; }
                }
            }
        }
    }
    if (best != nullptr) {
        SeatWorker(best, *availPtr, profKey);
        *out = best;
        return out;
    }
    *out = nullptr;
    return out;
}

// ============================================================================
// 0x14008cbf0 — strategy 1: distance-optimized relocate. Scan townMgr+0xa0 for a
// citizen already of THIS profession with a residence (+0x238) whose c7cf0 roster
// holds an idle laborer; pick the nearest such donor; seat one of its laborers.
// (If *availPtr==0 seats immediately during the scan.)
// ============================================================================
CitizenComponent** Strat_cbf0(CitizenBehavior* townMgr, CitizenComponent** out,
                              ComponentEntity** availPtr, Profession* profKey) {
    int* game = reinterpret_cast<int*>(GameUpdateLoop::_singleton->m_game);   // LEFT: SIG WorkBase_CalcDistance_8830 (int* map; Work/exe_bodies.h, wave X)
    int bestRecordIdx = -1;               // local_74
    uint32_t availCell = 0xffffffff;      // local_78
    if (*availPtr != nullptr) {
        ComponentEntity* availAnchorArg = *availPtr;
        availCell = (uint32_t)EntityAnchorCell(&availAnchorArg, nullptr);   // 0x140087fb0
    }
    int bestDist = 0x7ffffff;             // iVar14
    ComponentEntity* bestRes = nullptr;   // local_70

    int cnt = townMgr->citizenCount;
    for (int i = 0; i < cnt; ++i) {       // local_60 = i * 8
        CitizenComponent* cz = townMgr->citizens[i];   // local_80
        if (!AdultIdle(cz)) continue;
        if (cz->profession != profKey) continue;

        ComponentEntity* res = cz->home;  // local_90 = their residence (+0x238)
        if (res == nullptr) continue;
        ComponentEntity* resAnchor = res;                        // local_68
        uint32_t resCell = (uint32_t)EntityAnchorCell(&resAnchor, nullptr);
        int d = bestDist;
        if ((int)availCell >= 0) {
            d = WorkBase_CalcDistance_8830(game, resCell, availCell);
            if (d > bestDist) continue;
        }

        // walk res's c7cf0 roster for an idle laborer. Entity_FindComponent_Storage
        // (0x140008090) is the ResidenceComponent type scan.
        ResidenceComponent* c7cf0 = res->FindComponent<ResidenceComponent>();   // local_98 (IBP)
        int recCount = c7cf0->residentSlotCount;                  // +0x58
        ResidenceComponent::Family* recBase = nullptr;
        for (int r = 0; r < recCount; ++r) {
            recBase = c7cf0->residentSlots;                       // +0x50
            int slotCount = recBase[r].count;
            bool matched = false;
            Object::InternalPtr<CitizenComponent>* memberData = recBase[r].members;
            for (int m = 0; m < slotCount; ++m) {
                recBase = c7cf0->residentSlots;
                memberData = recBase[r].members;
                CitizenComponent* member = memberData[m].Get();
                if (AdultIdle(member)) {
                    if (member->profession == LaborerKey()) {
                        if (*availPtr == nullptr) {
                            SeatWorker(member, *availPtr, profKey);
                            *out = member;
                            return out;
                        }
                        bestRes = res; bestDist = d; bestRecordIdx = r;
                        matched = true;
                        break;
                    }
                }
                slotCount = c7cf0->residentSlots[r].count;
            }
            (void)matched;
            recCount = c7cf0->residentSlotCount;
        }
    }

    // Post-scan: seat from the best donor residence's record bestRecordIdx.
    if (bestRes != nullptr) {
        ResidenceComponent* c7cf0 = FindResidenceComp(bestRes);
        ResidenceComponent::Family* recAddr = &c7cf0->residentSlots[bestRecordIdx];
        int slotCount = recAddr->count;
        for (int m = 0; m < slotCount; ++m) {
            Object::InternalPtr<CitizenComponent>* memberData = c7cf0->residentSlots[bestRecordIdx].members;
            CitizenComponent* member = memberData[m].Get();
            if (member->profession == LaborerKey()) {
                if (AdultIdle(member)) {
                    SeatWorker(member, *availPtr, profKey);
                    *out = member;
                    return out;
                }
            }
            slotCount = c7cf0->residentSlots[bestRecordIdx].count;
        }
    }

    *out = nullptr;
    return out;
}

// ============================================================================
// 0x14008c5f0 — strategy 3: shuffle houses, pick the house whose c7cf0 roster record
// has the most idle laborers, then seat one from that record.
//
// A NULL `*availPtr` here is FAITHFUL — verified against retail 2026-08-15, do not
// "fix" it with a guard. Retail's cascade 0x14008c3d0 guards exactly one of the four
// strategies: `if (avail != 0)` wraps ca20 ALONE. ca20 needs the guard because it
// DEREFS avail on entry (Entity_impl_7fb0(&avail) to get its position for the
// nearest-residence sort); c5f0 and c4b0 only ever STORE the pointer
// (AssignHousing(cz,&avail); cz+0x1f0 = profKey), so retail lets a null through on
// purpose. The asymmetry in one 20-line function is the proof of intent.
//
// Consequence, also faithful: when FindAvailableBuilding returns null the citizen's
// profession still flips with no roster attach, so WPC+0x60 does not move and the
// panel redraws the same "N / M" (trace signature `via=idle roster 0->0`). That is a
// symptom of an upstream registration miss — the workplace absent from the
// townMgr+0x100 profession BST, roster >= capacity, or WPC+0x50 == 0 — NOT of a
// missing null check here.
// ============================================================================
CitizenComponent** Strat_c5f0(CitizenBehavior* townMgr, CitizenComponent** out,
                              ComponentEntity** availPtr, Profession* profKey) {
    ComponentEntity* bestHouse = nullptr;   // local_58
    int bestCount = 0;       // iVar9
    int bestRecordIdx = -1;  // iVar13

    Shuffle896a0(townMgr);

    int houseCnt = townMgr->field_0x28;
    for (int h = 0; h < houseCnt; ++h) {   // local_50[0] = h * 8
        ComponentEntity* house = townMgr->field_0x20[h];   // local_60
        ResidenceComponent* c7cf0 = FindResidenceComp(house);   // local_70
        int recCount = c7cf0->residentSlotCount;
        for (int r = 0; r < recCount; ++r) {
            int laborerN = 0;   // iVar6
            int slotCount = c7cf0->residentSlots[r].count;
            for (int m = 0; m < slotCount; ++m) {
                Object::InternalPtr<CitizenComponent>* memberData = c7cf0->residentSlots[r].members;
                CitizenComponent* member = memberData[m].Get();
                if (AdultIdle(member)) {
                    if (member->profession == LaborerKey()) laborerN++;
                }
                slotCount = c7cf0->residentSlots[r].count;
            }
            if (bestCount < laborerN) { bestHouse = house; bestCount = laborerN; bestRecordIdx = r; }
            recCount = c7cf0->residentSlotCount;
        }
    }

    if (bestHouse != nullptr) {
        ResidenceComponent* c7cf0 = FindResidenceComp(bestHouse);
        int slotCount = c7cf0->residentSlots[bestRecordIdx].count;
        for (int m = 0; m < slotCount; ++m) {
            Object::InternalPtr<CitizenComponent>* memberData = c7cf0->residentSlots[bestRecordIdx].members;
            CitizenComponent* member = memberData[m].Get();
            if (AdultIdle(member)) {
                if (member->profession == LaborerKey()) {
                    SeatWorker(member, *availPtr, profKey);
                    *out = member;
                    return out;
                }
            }
            slotCount = c7cf0->residentSlots[bestRecordIdx].count;
        }
    }

    *out = nullptr;
    return out;
}

} // namespace

// ============================================================================
// 0x140114000 — CitizenComponent_OnAdd: append the citizen to townMgr+0xa0.
// ============================================================================

// ============================================================================
// 0x140129de0 — ResidenceDescription_helper_9de0: append a built house's
// owner-entity IBP (= ResidenceComponent+0x10) to the townMgr+0x20 residence
// list (count at +0x28). Retail wires this as the residence build-state
// callback (helper_a350, state 2 = built) so a house registers the moment its
// construction completes; the town-hall "Homes" stat and the housing shuffle
// read this list. The exe copies the +0x10 IBP value raw (no re-ctor) — same
// here; the caller passes the value it captured at ResidenceComponent::OnCreate.
// ============================================================================
// INSTRUMENTED 2026-07-31. The five guards below are all silent, and the caller in
// residencecomponent.cpp logged "registered into townMgr+0x20" UNCONDITIONALLY after the
// call — so seven "registered" lines coexisted with count(+0x28)==0 and the log was a
// statement of intent, not of outcome. Per the standing rule (log the outcome, inside the
// guarded function, never let a silent no-op read as success), every call now reports
// which guard fired and the count before/after. Behaviour is unchanged: same guards, same
// order, same early return.
extern "C" void TownRegistry_RegisterResidence(int64_t houseEntityIBP) {
    EnsureFns();

    const char* fail = nullptr;
    CitizenBehavior* townMgr = nullptr;
    int before = -1, after = -1;

    if (!houseEntityIBP)              fail = "houseEntityIBP == 0";
    else if (!GameUpdateLoop::_singleton) fail = "GameUpdateLoop::_singleton == 0 (no GameWorld yet)";
    else {
        townMgr = GameUpdateLoop::_singleton->m_citizenList;
        if (townMgr == nullptr)       fail = "townMgr (gameWorld+0xf0) == 0";
    }

    if (!fail) {
        before = townMgr->field_0x28;
        // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
        GameColl::AsArray<GameColl::Ptr8>(&townMgr->field_0x20)
            .SetSizeFill(before + 1, &houseEntityIBP);
        after = townMgr->field_0x28;
        // The append is the only thing that advances the count. If it did not move, the
        // array primitive is not writing the count field we (and retail) read at +0x28 —
        // a different failure from any of the guards above, so name it separately.
        if (after != before + 1) fail = "append ran but count(+0x28) did not advance";
    }

    static int n = 0;
    if (n < 16) {
        ++n;
        std::fprintf(stderr, "[TownRegistry] RegisterResidence ibp=%llx townMgr=%p "
                     "count %d -> %d : %s\n",
                     static_cast<unsigned long long>(houseEntityIBP),
                     reinterpret_cast<void*>(townMgr), before, after,
                     fail ? fail : "APPENDED OK");
        std::fflush(stderr);
    }
}

// ============================================================================
// Headless driver — mirrors CitizenComponent_impl_dd10's per-worker inner loop.
// Seats up to `count` workers of profession `profKey`; returns the number actually
// seated. (dd10 derives profKey from a UI profession index via *(*(gw+0xb0)+0x20 +
// idx*8); here the caller already holds the key — e.g. g_foresterProfKey from the
// lodge's WorkPlaceDescription — so we take it directly.) Faithful fallback order:
// FindAvailableBuilding then cbf0 -> (if avail!=0) ca20 -> c5f0 -> c4b0.
// ============================================================================
extern "C" int CitizenEmploy_SeatWorkers(int64_t townMgr, uint64_t professionKey, int count) {
    EnsureFns();
    if (townMgr == 0 || GameUpdateLoop::_singleton == nullptr) return 0;
    if (professionKey == 0) return 0;
    CitizenBehavior* town = reinterpret_cast<CitizenBehavior*>(townMgr);   // LEFT: SIG CitizenEmploy_SeatWorkers (outside callers: starttown.cpp, standalone harnesses)
    Profession* profKey = reinterpret_cast<Profession*>(professionKey);    // LEFT: SIG CitizenEmploy_SeatWorkers

    int seated = 0;
    for (int i = 0; i < count; ++i) {
        ComponentEntity* availOut[2] = { nullptr, nullptr };
        FindAvailableBuilding(townMgr, availOut, professionKey);
        ComponentEntity* avail = availOut[0];

        CitizenComponent* result[2];
        ComponentEntity* availArg;

        result[0] = nullptr; availArg = avail;
        Strat_cbf0(town, result, &availArg, profKey);
        CitizenComponent* seatedCz = result[0];
        if (seatedCz == nullptr) {
            if (avail != nullptr) {
                result[0] = nullptr; availArg = avail;
                Strat_ca20(town, result, &availArg, profKey);
                seatedCz = result[0];
            }
            if (seatedCz == nullptr) {
                result[0] = nullptr; availArg = avail;
                Strat_c5f0(town, result, &availArg, profKey);
                seatedCz = result[0];
                if (seatedCz == nullptr) {
                    result[0] = nullptr; availArg = avail;
                    Strat_c4b0(town, result, &availArg, profKey);
                    seatedCz = result[0];
                }
            }
        }

        if (seatedCz != nullptr) {
            ++seated;
        } else {
            break;   // no candidate this round -> stop
        }
    }
    return seated;
}

// ============================================================================
// Direct-target variant of CitizenEmploy_SeatWorkers: seat up to `count` workers of
// profession `profKey` onto a SPECIFIC workplace entity (`availEntity`), WITHOUT going
// through FindAvailableBuilding (which requires the workplace to be registered in the
// townMgr+0x100 profession BST). Used for construction sites: registering a build site
// into townMgr+0x100 activates the BuildPathGraph->ProcessPaths->AssignCitizens drain
// path, whose Phase-5B clears every idle laborer's housing via AssignHousing and can
// deadlock the WPC Scene mesh-visual toggle. Seating directly against the site entity
// avoids that entirely while running the exact same cbf0->ca20->c5f0->c4b0 cascade.
// Each seat's AssignHousing attaches the worker to availEntity's WorkPlaceComponent
// roster (+0x58/+0x60); once full the strategies find no more idle laborers and stop.
// ============================================================================
extern "C" int CitizenEmploy_SeatWorkersAt(int64_t townMgr, int64_t availEntity, uint64_t professionKey, int count) {
    EnsureFns();
    if (townMgr == 0 || availEntity == 0 || GameUpdateLoop::_singleton == nullptr) return 0;
    if (professionKey == 0) return 0;
    CitizenBehavior* town     = reinterpret_cast<CitizenBehavior*>(townMgr);       // LEFT: SIG CitizenEmploy_SeatWorkersAt (outside caller: buildcomponent_state8.cpp)
    ComponentEntity* availEnt = reinterpret_cast<ComponentEntity*>(availEntity);   // LEFT: SIG CitizenEmploy_SeatWorkersAt
    Profession*      profKey  = reinterpret_cast<Profession*>(professionKey);      // LEFT: SIG CitizenEmploy_SeatWorkersAt

    int seated = 0;
    for (int i = 0; i < count; ++i) {
        CitizenComponent* result[2];
        ComponentEntity* availArg;

        result[0] = nullptr; availArg = availEnt;
        Strat_cbf0(town, result, &availArg, profKey);
        CitizenComponent* seatedCz = result[0];
        if (seatedCz == nullptr) {
            result[0] = nullptr; availArg = availEnt;
            Strat_ca20(town, result, &availArg, profKey);
            seatedCz = result[0];
            if (seatedCz == nullptr) {
                result[0] = nullptr; availArg = availEnt;
                Strat_c5f0(town, result, &availArg, profKey);
                seatedCz = result[0];
                if (seatedCz == nullptr) {
                    result[0] = nullptr; availArg = availEnt;
                    Strat_c4b0(town, result, &availArg, profKey);
                    seatedCz = result[0];
                }
            }
        }

        if (seatedCz != nullptr) {
            ++seated;
            // A real retail Builder-profession citizen has NO producing workplace: work flows
            // through the task-reservation system (+0x208 / town task-mgr), not AssignWorkplace,
            // so citizen+0x238 == 0 for them (Ghidra-verified). Our idle-laborer seed carries a
            // stale +0x238 (old/residence workplace) which makes the CitizenAI candidate brain
            // offer GatherResources/2 (restock-your-workplace) forever — the "firewood at the
            // build site" distraction. Clear it faithfully via AssignWorkplace(&null,-1) (proper
            // worker-list detach + +0x238=0 / +0x240=-1) so the healthy-branch gather block,
            // gated `if(+0x238!=0)`, is skipped and the builder falls through to Idle -> build.
            ComponentEntity* prevWp = seatedCz->home;   // +0x238
            if (prevWp != nullptr) {
                seatedCz->SetHome(nullptr, -1);         // 0x1401163d0
            }
        } else {
            break;
        }
    }
    return seated;
}

// ============================================================================
// Registerable exports of the four seat strategies. The exe's ONLY dispatcher of
// these addresses is TownHallUI's profession spinner (CitizenComponent_impl_dd10,
// 0x14007dd10 -> fn_CitizenList_FindIdle/FindWorking/FindStudent/FindAny). Now that
// the town-hall panel is promoted in standalone, that path is live, so expose the
// file-static strategies for RegisterExeAddr (standalone/reg_townhallui.cpp).
// ============================================================================
// The C signatures stay for their outside callers (professionui.cpp, townhallui.cpp,
// workplaceui.cpp); each argument is typed once here.
extern "C" void* CitizenEmploy_Strat_cbf0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey) {
    EnsureFns();
    return Strat_cbf0(reinterpret_cast<CitizenBehavior*>(townMgr), reinterpret_cast<CitizenComponent**>(out),   // LEFT: SIG CitizenEmploy_Strat_cbf0 (Component/exe_bodies.h, wave X)
                      reinterpret_cast<ComponentEntity**>(availPtr), reinterpret_cast<Profession*>(profKey));   // LEFT: SIG CitizenEmploy_Strat_cbf0
}
extern "C" void* CitizenEmploy_Strat_ca20(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey) {
    EnsureFns();
    return Strat_ca20(reinterpret_cast<CitizenBehavior*>(townMgr), reinterpret_cast<CitizenComponent**>(out),   // LEFT: SIG CitizenEmploy_Strat_ca20 (Component/exe_bodies.h, wave X)
                      reinterpret_cast<ComponentEntity**>(availPtr), reinterpret_cast<Profession*>(profKey));   // LEFT: SIG CitizenEmploy_Strat_ca20
}
extern "C" void* CitizenEmploy_Strat_c5f0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey) {
    EnsureFns();
    return Strat_c5f0(reinterpret_cast<CitizenBehavior*>(townMgr), reinterpret_cast<CitizenComponent**>(out),   // LEFT: SIG CitizenEmploy_Strat_c5f0 (Component/exe_bodies.h, wave X)
                      reinterpret_cast<ComponentEntity**>(availPtr), reinterpret_cast<Profession*>(profKey));   // LEFT: SIG CitizenEmploy_Strat_c5f0
}
extern "C" void* CitizenEmploy_Strat_c4b0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey) {
    EnsureFns();
    return Strat_c4b0(reinterpret_cast<CitizenBehavior*>(townMgr), reinterpret_cast<CitizenComponent**>(out),   // LEFT: SIG CitizenEmploy_Strat_c4b0 (Component/exe_bodies.h, wave X)
                      reinterpret_cast<ComponentEntity**>(availPtr), reinterpret_cast<Profession*>(profKey));   // LEFT: SIG CitizenEmploy_Strat_c4b0
}

// CitizenList::AssignWorker (0x14008c3d0) — the top-level seat cascade the exe's
// worker-add UI calls (fn_CitizenList_AssignWorker, workplaceui.cpp id 0x1450).
// Tries the four strategies in order on a FRESH copy of *availPtr each time:
// cbf0 (distance-optimized relocate) -> ca20 (nearest employed laborer, skipped if
// *availPtr is 0) -> c5f0 (most-laborer house) -> c4b0 (first idle laborer, writes
// `out` itself). First nonzero winner's 8-byte result is copied into out+0x0 only,
// exactly like the original (out+0x8 is left untouched on those paths).
// Defined below; AddWorker (0x140091130) calls it.
extern "C" void* CitizenList_AssignWorker_c3d0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey);

// ============================================================================
// CitizenList::AddWorker (0x140091130) — the public one-call seat: find a building
// with an open slot for `professionKey`, then run the four-strategy cascade against
// it. Retail is literally two calls (the result is discarded; the seat happens as a
// side effect inside the strategies):
//
//     FUN_14008d100(townMgr, &avail, profKey);        // FindAvailableBuilding
//     local_res20 = avail;                            // fresh copy — c3d0 mutates it
//     CitizenComponent_impl_c3d0(townMgr, out, &local_res20, profKey);
//
// The copy matters: c3d0 takes `availPtr` by pointer and each strategy anchors
// through it, so the caller's value must not be handed over directly.
//
// DEVIATION (deliberate): we hold g_wpcRosterOnly across the call. Retail does not
// need it — its WorkplaceComponent Attach/Detach can run the full Graphics::Scene
// mesh-visual toggle, which faults on our inert standalone objects (READ of 0x10 in
// Detach's e710 getter). Every other seat path in this build holds the same guard
// (ProfessionUI_HandleSpinner, TownHallUI_OnProfessionSpinner, WorkPlaceUI
// W_OnRemove, BuilderPool_Tick, CitizenAssignCitizens); this is a call site, so it
// holds it too. Drop the guard here if WorkPlaceComponent is ever promoted to a live
// controller with real scene objects.
extern "C" void CitizenList_AddWorker_91130(int64_t townMgr, uint64_t professionKey) {
    EnsureFns();
    if (townMgr == 0 || professionKey == 0) return;

    extern bool g_wpcRosterOnly;
    struct RosterGuard {
        bool prev;
        RosterGuard() { prev = g_wpcRosterOnly; g_wpcRosterOnly = true; }
        ~RosterGuard() { g_wpcRosterOnly = prev; }
    } _rosterGuard;

    int64_t avail[2] = { 0, 0 };
    FindAvailableBuilding(townMgr, avail, professionKey);

    int64_t availArg = avail[0];                 // fresh copy — c3d0 anchors through it
    alignas(8) uint8_t out[16] = { 0 };          // local_18[16] in the original
    CitizenList_AssignWorker_c3d0(townMgr, out, &availArg, static_cast<int64_t>(professionKey));
}

extern "C" void* CitizenList_AssignWorker_c3d0(int64_t townMgrV, void* outV, int64_t* availPtrV, int64_t profKeyV) {
    EnsureFns();
    CitizenBehavior*   townMgr  = reinterpret_cast<CitizenBehavior*>(townMgrV);     // LEFT: SIG CitizenList_AssignWorker_c3d0 (Component/exe_bodies.h, wave X)
    CitizenComponent** out      = reinterpret_cast<CitizenComponent**>(outV);       // LEFT: SIG CitizenList_AssignWorker_c3d0
    ComponentEntity**  availPtr = reinterpret_cast<ComponentEntity**>(availPtrV);   // LEFT: SIG CitizenList_AssignWorker_c3d0
    Profession*        profKey  = reinterpret_cast<Profession*>(profKeyV);          // LEFT: SIG CitizenList_AssignWorker_c3d0
    alignas(8) CitizenComponent* local[2] = { nullptr, nullptr };
    ComponentEntity* avail = *availPtr;
    CitizenComponent** r = Strat_cbf0(townMgr, local, &avail, profKey);
    CitizenComponent* won = *r;
    if (won == nullptr) {
        avail = *availPtr;
        if (avail != nullptr) {
            r = Strat_ca20(townMgr, local, &avail, profKey);
            won = *r;
        }
        if (won == nullptr) {
            avail = *availPtr;
            r = Strat_c5f0(townMgr, local, &avail, profKey);
            won = *r;
            if (won == nullptr) {
                avail = *availPtr;
                Strat_c4b0(townMgr, out, &avail, profKey);
                return out;
            }
        }
    }
    *out = won;
    return out;
}

// [6] 0x140114000 — the ADDED-to-world broadcast (ComponentEntity_OnAddedPass,
// flags bit 0): append to the town citizen list townMgr+0xa0, the list the
// census, the town-hall stats and every population system read.
void CitizenComponent::OnAdded() {
    EnsureFns();
    if (GameUpdateLoop::_singleton == nullptr) return;
    CitizenBehavior* townMgr = GameUpdateLoop::_singleton->m_citizenList;
    if (townMgr == nullptr) return;
    CitizenComponent* ref[2] = { nullptr, nullptr };
    ref[0] = this;   // the InternalBasePtr ctor's one store
    // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
    GameColl::AsArray<GameColl::Ptr8>(&townMgr->citizens)
        .SetSizeFill(townMgr->citizenCount + 1, ref);
}
