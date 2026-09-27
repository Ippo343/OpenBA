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

#include "resolve.h"
#include "Component/ResidenceComponent.h"   // B.3.8 CB1a (TypeId)
#include "engine_funcs.h"
#include "Component/coll_bridge.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/CitizenComponent.h"

// ---- deps ported elsewhere (link to them) ----
class CitizenComponent;   // opaque — only for matching the AssignHousing mangled name
extern "C" void* FindAvailableBuilding(int64_t townMgr, void* out, uint64_t professionKey); // 0x14008d100
extern void  CitizenComp_AssignHousing(CitizenComponent* self, int64_t* buildingIBPptr);    // 0x140111ee0 -> +0x248
extern void* CitizenComp_Entity_FindComponent_Storage(int64_t entity, void* outIBP);        // 0x140008090 -> c7cf0

namespace {

// 0x140087fb0 — Entity_impl_7fb0: resolve an entity IBP's anchor cell (used for distance).
typedef int   (*fn_Anchor7fb0_t)(void* entityIBPptr, float* out);
// 0x140008830 — WorkBase distance metric between two cells (game = *(gw+0xf8)).
typedef int   (*fn_CalcDist8830_t)(int* game, uint32_t cellA, uint32_t cellB);
// 0x140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
// into the exe (ENDGAME B.3.2 step 2) — converted at its call sites below.
// 0x1401163d0 — CitizenComponent::AssignWorkplace(self, workplaceIBP, slotIndex).
//   Passing &nullIBP,-1 detaches the citizen from any workplace worker list and clears
//   citizen+0x238 (workplace) / +0x240 (slot) — the "become an unassigned laborer" clear.
typedef void  (*fn_AssignWorkplace_t)(void* self, void* workplaceIBP, int slotIndex);

fn_Anchor7fb0_t      s_anchor7fb0      = nullptr;
fn_CalcDist8830_t    s_calcDist8830    = nullptr;
fn_AssignWorkplace_t s_assignWorkplace = nullptr;
uint32_t          s_tidResidence = 0;   // ResidenceComponent (c7cf0) — self-resolved
bool              s_init         = false;

void EnsureFns() {
    if (s_init) return;
    s_anchor7fb0   = reinterpret_cast<fn_Anchor7fb0_t>(ResolveAddr(Module::Exe, 0x140087fb0));
    s_calcDist8830 = reinterpret_cast<fn_CalcDist8830_t>(ResolveAddr(Module::Exe, 0x140008830));
    s_assignWorkplace = reinterpret_cast<fn_AssignWorkplace_t>(ResolveAddr(Module::Exe, 0x1401163d0));
    if (s_tidResidence == 0) s_tidResidence = ResidenceComponent::TypeId();
    s_init = true;
}

inline uintptr_t GW()          { return reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton); }
inline int64_t   TownMgr()     { return *reinterpret_cast<int64_t*>(GW() + 0xf0); }
inline int*      GameObj()     { return *reinterpret_cast<int**>(GW() + 0xf8); }
inline int64_t   LaborerKey()  { return *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(GW() + 0xb0) + 0x30); }
inline void Ctor(void* self, void* obj) { if (fn_InternalBasePtr_Ctor) new (self) ::Object::InternalBasePtr(
    reinterpret_cast<RtObject::Internal*>(obj)); }   // B.3.3 bridge

typedef uint32_t (*GetTypeId_t)(void*);

// citizen adult (desc+0x44 <= cz+0x3c) && idle (cz+0x208 == 0). No CheckPtr (caller
// already CheckPtr'd; the exe reads these two fields before the mid-check CheckPtr).
inline bool AdultIdle(int64_t cz) {
    return (*reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0x44)
              <= *reinterpret_cast<float*>(cz + 0x3c))
        && (*reinterpret_cast<int64_t*>(cz + 0x208) == 0);
}

// Find the ResidenceComponent (c7cf0) on an entity — mirrors the exe's inline scan
// (stop at first GetTypeId match; return it only if IsDerivedFrom too, else null).
void* FindResidenceComp(int64_t entity) {
    int i = 0;
    int64_t arr = *reinterpret_cast<int64_t*>(entity + 0x30);
    int cnt = *reinterpret_cast<int*>(entity + 0x38);
    int64_t off = 0;
    while (i < cnt) {
        void* elem = reinterpret_cast<void*>(arr + off);
        void* comp = *reinterpret_cast<void**>(elem);
        uint32_t tid = ((GetTypeId_t)(*reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(comp) + 0x10)))(comp);
        if (tid == s_tidResidence) {
            void* elem2 = reinterpret_cast<void*>(arr + off);
            void* compVal = *reinterpret_cast<void**>(elem2);
            return (fn_IsDerivedFrom && fn_IsDerivedFrom(compVal, s_tidResidence)) ? compVal : nullptr;
        }
        ++i; off += 8;
        cnt = *reinterpret_cast<int*>(entity + 0x38);   // faithful re-read
    }
    return nullptr;
}

// Seat epilogue shared by all four strategies: if the citizen isn't already this
// profession at this building, AssignHousing(citizen,&building) (-> +0x248) and set
// +0x1f0 = profKey. `avail` = the workplace building IBP value.
inline void SeatWorker(int64_t citizen, int64_t avail, int64_t profKey) {
    int64_t bldg = avail;
    if ((*reinterpret_cast<int64_t*>(citizen + 0x1f0) != profKey) ||
        (*reinterpret_cast<int64_t*>(citizen + 0x248) != bldg)) {
        int64_t local = bldg;
        CitizenComp_AssignHousing(reinterpret_cast<CitizenComponent*>(citizen), &local);
        *reinterpret_cast<int64_t*>(citizen + 0x1f0) = profKey;
    }
}

// ============================================================================
// 0x1400896a0 — Fisher-Yates shuffle of the town house list (townMgr+0x20 array,
// townMgr+0x28 count). Uses System::Random::Rand(*(gw+0xd8), n).
// ============================================================================
void Shuffle896a0(int64_t townMgr) {
    uint32_t n = *reinterpret_cast<uint32_t*>(townMgr + 0x28);
    void* rng = *reinterpret_cast<void**>(GW() + 0xd8);
    if ((int)n <= 1) return;
    for (int64_t i = 0; (int64_t)i < (int)n; ++i) {
        uint32_t j = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(n));   // B.3.4b bridge
        if ((uint32_t)i != j) {
            int64_t base = *reinterpret_cast<int64_t*>(townMgr + 0x20);
            int64_t tmp = *reinterpret_cast<int64_t*>(base + i * 8);
            *reinterpret_cast<int64_t*>(base + i * 8) = *reinterpret_cast<int64_t*>(base + (int64_t)(int)j * 8);
            *reinterpret_cast<int64_t*>(base + (int64_t)(int)j * 8) = tmp;
        }
    }
}

// ============================================================================
// 0x14008c4b0 — strategy 4: first adult+idle+laborer in townMgr+0xa0 -> seat.
// ============================================================================
void* Strat_c4b0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey) {
    int cnt = *reinterpret_cast<int*>(townMgr + 0xa8);
    int64_t off = 0;
    for (int i = 0; i < cnt; ++i, off += 8) {
        int64_t cz = *reinterpret_cast<int64_t*>(off + *reinterpret_cast<int64_t*>(townMgr + 0xa0));
        if (AdultIdle(cz)) {
            if (*reinterpret_cast<int64_t*>(cz + 0x1f0) == LaborerKey()) {
                SeatWorker(cz, *availPtr, profKey);
                *reinterpret_cast<int64_t*>(out) = cz;
                return out;
            }
        }
    }
    Ctor(out, nullptr);
    return out;
}

// ============================================================================
// 0x14008ca20 — strategy 2: nearest (to avail) adult+idle laborer that already HAS
// a residence (+0x238) -> seat.
// ============================================================================
void* Strat_ca20(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey) {
    int* game = GameObj();
    int64_t availAnchorArg = *availPtr;
    uint32_t availCell = s_anchor7fb0 ? (uint32_t)s_anchor7fb0(&availAnchorArg, nullptr) : 0xffffffff;
    int bestDist = 0x7ffffff;
    int64_t best = 0;
    int cnt = *reinterpret_cast<int*>(townMgr + 0xa8);
    int64_t off = 0;
    for (int i = 0; i < cnt; ++i, off += 8) {
        int64_t cz = *reinterpret_cast<int64_t*>(off + *reinterpret_cast<int64_t*>(townMgr + 0xa0));
        if (AdultIdle(cz)) {
            if (*reinterpret_cast<int64_t*>(cz + 0x1f0) == LaborerKey()) {
                if (*reinterpret_cast<int64_t*>(cz + 0x238) != 0) {
                    int64_t resAnchor = *reinterpret_cast<int64_t*>(cz + 0x238);
                    uint32_t czCell = s_anchor7fb0 ? (uint32_t)s_anchor7fb0(&resAnchor, nullptr) : 0;
                    int d = s_calcDist8830 ? s_calcDist8830(game, czCell, availCell) : 0x7ffffff;
                    if (d <= bestDist) { best = cz; bestDist = d; }
                }
            }
        }
    }
    if (best != 0) {
        SeatWorker(best, *availPtr, profKey);
        *reinterpret_cast<int64_t*>(out) = best;
        return out;
    }
    Ctor(out, nullptr);
    return out;
}

// ============================================================================
// 0x14008cbf0 — strategy 1: distance-optimized relocate. Scan townMgr+0xa0 for a
// citizen already of THIS profession with a residence (+0x238) whose c7cf0 roster
// holds an idle laborer; pick the nearest such donor; seat one of its laborers.
// (If *availPtr==0 seats immediately during the scan.)
// ============================================================================
void* Strat_cbf0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey) {
    int* game = GameObj();
    int bestRecordIdx = -1;               // local_74
    uint32_t availCell = 0xffffffff;      // local_78
    if (*availPtr != 0) {
        int64_t availAnchorArg = *availPtr;
        availCell = s_anchor7fb0 ? (uint32_t)s_anchor7fb0(&availAnchorArg, nullptr) : 0xffffffff;
    }
    int bestDist = 0x7ffffff;             // iVar14
    int64_t bestRes = 0;                  // local_70

    int cnt = *reinterpret_cast<int*>(townMgr + 0xa8);
    int64_t off = 0;                      // local_60
    for (int i = 0; i < cnt; ++i, off += 8) {
        int64_t cz = *reinterpret_cast<int64_t*>(off + *reinterpret_cast<int64_t*>(townMgr + 0xa0));   // local_80
        if (!AdultIdle(cz)) continue;
        if (*reinterpret_cast<int64_t*>(cz + 0x1f0) != profKey) continue;

        int64_t res = *reinterpret_cast<int64_t*>(cz + 0x238);   // local_90 = their residence
        if (res == 0) continue;
        int64_t resAnchor = res;                                 // local_68
        uint32_t resCell = s_anchor7fb0 ? (uint32_t)s_anchor7fb0(&resAnchor, nullptr) : 0;
        int d = bestDist;
        if ((int)availCell >= 0) {
            d = s_calcDist8830 ? s_calcDist8830(game, resCell, availCell) : bestDist;
            if (d > bestDist) continue;
        }

        // walk res's c7cf0 roster for an idle laborer. Entity_FindComponent_Storage
        // writes the found component into its out-IBP directly (hidden return).
        int64_t c7cf0 = 0;                                       // local_98 (IBP)
        CitizenComp_Entity_FindComponent_Storage(res, &c7cf0);
        int recCount = *reinterpret_cast<int*>(c7cf0 + 0x58);
        int64_t recBase = 0;
        for (int r = 0; r < recCount; ++r) {
            recBase = *reinterpret_cast<int64_t*>(c7cf0 + 0x50);
            int slotCount = *reinterpret_cast<int*>(recBase + 8 + (int64_t)r * 0x10);
            bool matched = false;
            int64_t memberData = *reinterpret_cast<int64_t*>(recBase + (int64_t)r * 0x10);
            for (int m = 0; m < slotCount; ++m) {
                recBase = *reinterpret_cast<int64_t*>(c7cf0 + 0x50);
                memberData = *reinterpret_cast<int64_t*>(recBase + (int64_t)r * 0x10);
                int64_t member = *reinterpret_cast<int64_t*>((int64_t)m * 8 + memberData);
                if (AdultIdle(member)) {
                    if (*reinterpret_cast<int64_t*>(member + 0x1f0) == LaborerKey()) {
                        if (*availPtr == 0) {
                            SeatWorker(member, *availPtr, profKey);
                            *reinterpret_cast<int64_t*>(out) = member;
                            return out;
                        }
                        bestRes = res; bestDist = d; bestRecordIdx = r;
                        matched = true;
                        break;
                    }
                }
                slotCount = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + 8 + (int64_t)r * 0x10);
            }
            (void)matched;
            recCount = *reinterpret_cast<int*>(c7cf0 + 0x58);
        }
    }

    // Post-scan: seat from the best donor residence's record bestRecordIdx.
    if (bestRes != 0) {
        void* comp = FindResidenceComp(bestRes);
        int64_t c7cf0 = 0;
        Ctor(&c7cf0, comp);
        int64_t recAddr = *reinterpret_cast<int64_t*>(c7cf0 + 0x50) + (int64_t)bestRecordIdx * 0x10;
        int slotCount = *reinterpret_cast<int*>(recAddr + 8);
        for (int m = 0; m < slotCount; ++m) {
            int64_t memberData = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + (int64_t)bestRecordIdx * 0x10);
            int64_t member = *reinterpret_cast<int64_t*>((int64_t)m * 8 + memberData);
            if (*reinterpret_cast<int64_t*>(member + 0x1f0) == LaborerKey()) {
                if (AdultIdle(member)) {
                    SeatWorker(member, *availPtr, profKey);
                    *reinterpret_cast<int64_t*>(out) = member;
                    return out;
                }
            }
            slotCount = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + 8 + (int64_t)bestRecordIdx * 0x10);
        }
    }

    Ctor(out, nullptr);
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
void* Strat_c5f0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey) {
    int64_t bestHouse = 0;   // local_58
    int bestCount = 0;       // iVar9
    int bestRecordIdx = -1;  // iVar13

    Shuffle896a0(townMgr);

    int houseCnt = *reinterpret_cast<int*>(townMgr + 0x28);
    int64_t hoff = 0;        // local_50[0]
    for (int h = 0; h < houseCnt; ++h, hoff += 8) {
        int64_t house = *reinterpret_cast<int64_t*>(hoff + *reinterpret_cast<int64_t*>(townMgr + 0x20));   // local_60
        void* comp = FindResidenceComp(house);
        int64_t c7cf0 = 0;   // local_70
        Ctor(&c7cf0, comp);
        int recCount = *reinterpret_cast<int*>(c7cf0 + 0x58);
        for (int r = 0; r < recCount; ++r) {
            int laborerN = 0;   // iVar6
            int slotCount = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + 8 + (int64_t)r * 0x10);
            for (int m = 0; m < slotCount; ++m) {
                int64_t memberData = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + (int64_t)r * 0x10);
                int64_t member = *reinterpret_cast<int64_t*>((int64_t)m * 8 + memberData);
                if (AdultIdle(member)) {
                    if (*reinterpret_cast<int64_t*>(member + 0x1f0) == LaborerKey()) laborerN++;
                }
                slotCount = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + 8 + (int64_t)r * 0x10);
            }
            if (bestCount < laborerN) { bestHouse = house; bestCount = laborerN; bestRecordIdx = r; }
            recCount = *reinterpret_cast<int*>(c7cf0 + 0x58);
        }
    }

    if (bestHouse != 0) {
        void* comp = FindResidenceComp(bestHouse);
        int64_t c7cf0 = 0;
        Ctor(&c7cf0, comp);
        int slotCount = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + 8 + (int64_t)bestRecordIdx * 0x10);
        for (int m = 0; m < slotCount; ++m) {
            int64_t memberData = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + (int64_t)bestRecordIdx * 0x10);
            int64_t member = *reinterpret_cast<int64_t*>((int64_t)m * 8 + memberData);
            if (AdultIdle(member)) {
                if (*reinterpret_cast<int64_t*>(member + 0x1f0) == LaborerKey()) {
                    SeatWorker(member, *availPtr, profKey);
                    *reinterpret_cast<int64_t*>(out) = member;
                    return out;
                }
            }
            slotCount = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + 8 + (int64_t)bestRecordIdx * 0x10);
        }
    }

    Ctor(out, nullptr);
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
    int64_t townMgr = 0;
    int before = -1, after = -1;

    if (!houseEntityIBP)              fail = "houseEntityIBP == 0";
    else if (!g_pGameWorldSingleton)  fail = "g_pGameWorldSingleton pointer is null";
    else if (!*g_pGameWorldSingleton) fail = "*g_pGameWorldSingleton == 0 (no GameWorld yet)";
    else {
        townMgr = TownMgr();
        if (townMgr == 0)             fail = "townMgr (gameWorld+0xf0) == 0";
    }

    if (!fail) {
        before = *reinterpret_cast<int*>(townMgr + 0x28);
        // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
        GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(townMgr + 0x20))
            .SetSizeFill(before + 1, &houseEntityIBP);
        after = *reinterpret_cast<int*>(townMgr + 0x28);
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
    if (townMgr == 0 || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 0;
    if (professionKey == 0) return 0;
    int64_t profKey = (int64_t)professionKey;

    int seated = 0;
    for (int i = 0; i < count; ++i) {
        int64_t availOut[2] = { 0, 0 };
        FindAvailableBuilding(townMgr, availOut, (uint64_t)profKey);
        int64_t avail = availOut[0];

        int64_t result[2];
        int64_t availArg;

        result[0] = 0; availArg = avail;
        Strat_cbf0(townMgr, result, &availArg, profKey);
        int64_t seatedCz = result[0];
        if (seatedCz == 0) {
            if (avail != 0) {
                result[0] = 0; availArg = avail;
                Strat_ca20(townMgr, result, &availArg, profKey);
                seatedCz = result[0];
            }
            if (seatedCz == 0) {
                result[0] = 0; availArg = avail;
                Strat_c5f0(townMgr, result, &availArg, profKey);
                seatedCz = result[0];
                if (seatedCz == 0) {
                    result[0] = 0; availArg = avail;
                    Strat_c4b0(townMgr, result, &availArg, profKey);
                    seatedCz = result[0];
                }
            }
        }

        if (seatedCz != 0) {
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
    if (townMgr == 0 || availEntity == 0 || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 0;
    if (professionKey == 0) return 0;
    int64_t profKey = static_cast<int64_t>(professionKey);

    int seated = 0;
    for (int i = 0; i < count; ++i) {
        int64_t result[2];
        int64_t availArg;

        result[0] = 0; availArg = availEntity;
        Strat_cbf0(townMgr, result, &availArg, profKey);
        int64_t seatedCz = result[0];
        if (seatedCz == 0) {
            result[0] = 0; availArg = availEntity;
            Strat_ca20(townMgr, result, &availArg, profKey);
            seatedCz = result[0];
            if (seatedCz == 0) {
                result[0] = 0; availArg = availEntity;
                Strat_c5f0(townMgr, result, &availArg, profKey);
                seatedCz = result[0];
                if (seatedCz == 0) {
                    result[0] = 0; availArg = availEntity;
                    Strat_c4b0(townMgr, result, &availArg, profKey);
                    seatedCz = result[0];
                }
            }
        }

        if (seatedCz != 0) {
            ++seated;
            // A real retail Builder-profession citizen has NO producing workplace: work flows
            // through the task-reservation system (+0x208 / town task-mgr), not AssignWorkplace,
            // so citizen+0x238 == 0 for them (Ghidra-verified). Our idle-laborer seed carries a
            // stale +0x238 (old/residence workplace) which makes the CitizenAI candidate brain
            // offer GatherResources/2 (restock-your-workplace) forever — the "firewood at the
            // build site" distraction. Clear it faithfully via AssignWorkplace(&null,-1) (proper
            // worker-list detach + +0x238=0 / +0x240=-1) so the healthy-branch gather block,
            // gated `if(+0x238!=0)`, is skipped and the builder falls through to Idle -> build.
            int64_t prevWp = *reinterpret_cast<int64_t*>(seatedCz + 0x238);
            if (prevWp != 0 && s_assignWorkplace) {
                int64_t nullIBP = 0;
                s_assignWorkplace(reinterpret_cast<void*>(seatedCz), &nullIBP, -1);
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
extern "C" void* CitizenEmploy_Strat_cbf0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey) {
    EnsureFns(); return Strat_cbf0(townMgr, out, availPtr, profKey);
}
extern "C" void* CitizenEmploy_Strat_ca20(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey) {
    EnsureFns(); return Strat_ca20(townMgr, out, availPtr, profKey);
}
extern "C" void* CitizenEmploy_Strat_c5f0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey) {
    EnsureFns(); return Strat_c5f0(townMgr, out, availPtr, profKey);
}
extern "C" void* CitizenEmploy_Strat_c4b0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey) {
    EnsureFns(); return Strat_c4b0(townMgr, out, availPtr, profKey);
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

extern "C" void* CitizenList_AssignWorker_c3d0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey) {
    EnsureFns();
    alignas(8) uint8_t local[16] = {0};
    int64_t avail = *availPtr;
    void* r = Strat_cbf0(townMgr, local, &avail, profKey);
    int64_t won = *reinterpret_cast<int64_t*>(r);
    if (won == 0) {
        avail = *availPtr;
        if (avail != 0) {
            r = Strat_ca20(townMgr, local, &avail, profKey);
            won = *reinterpret_cast<int64_t*>(r);
        }
        if (won == 0) {
            avail = *availPtr;
            r = Strat_c5f0(townMgr, local, &avail, profKey);
            won = *reinterpret_cast<int64_t*>(r);
            if (won == 0) {
                avail = *availPtr;
                Strat_c4b0(townMgr, out, &avail, profKey);
                return out;
            }
        }
    }
    *reinterpret_cast<int64_t*>(out) = won;
    return out;
}

// [6] 0x140114000 — the ADDED-to-world broadcast (ComponentEntity_OnAddedPass,
// flags bit 0): append to the town citizen list townMgr+0xa0, the list the
// census, the town-hall stats and every population system read.
void CitizenComponent::OnAdded() {
    EnsureFns();
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    int64_t townMgr = TownMgr();
    if (townMgr == 0) return;
    int64_t ref[2] = { 0, 0 };
    Ctor(ref, this);
    // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
    GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(townMgr + 0xa0))
        .SetSizeFill(*reinterpret_cast<int*>(townMgr + 0xa8) + 1, ref);
}
