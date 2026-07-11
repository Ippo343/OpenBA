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
//
// This file must NOT depend on hooks.h — portable for standalone.

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ---- deps ported elsewhere (link to them) ----
struct CitizenComponent;   // opaque — only for matching the AssignHousing mangled name
extern "C" void* FindAvailableBuilding(int64_t townMgr, void* out, uint64_t professionKey); // 0x14008d100
extern void  CitizenComp_AssignHousing(CitizenComponent* self, int64_t* buildingIBPptr);    // 0x140111ee0 -> +0x248
extern void* CitizenComp_Entity_FindComponent_Storage(int64_t entity, void* outIBP);        // 0x140008090 -> c7cf0

namespace {

// 0x140087fb0 — Entity_impl_7fb0: resolve an entity IBP's anchor cell (used for distance).
typedef int   (*fn_Anchor7fb0_t)(void* entityIBPptr, float* out);
// 0x140008830 — WorkBase distance metric between two cells (game = *(gw+0xf8)).
typedef int   (*fn_CalcDist8830_t)(int* game, uint32_t cellA, uint32_t cellB);
// 0x140003110 — growable 8-byte-element array: set-count + append *elemIBP.
typedef void  (*fn_ArrayAdd3110_t)(int64_t* arrayStruct, int newCount, void* elemIBP);

fn_Anchor7fb0_t   s_anchor7fb0   = nullptr;
fn_CalcDist8830_t s_calcDist8830 = nullptr;
fn_ArrayAdd3110_t s_arrayAdd3110 = nullptr;
uint32_t          s_tidResidence = 0;   // ResidenceComponent (c7cf0) — self-resolved
bool              s_init         = false;

void EnsureFns() {
    if (s_init) return;
    s_anchor7fb0   = reinterpret_cast<fn_Anchor7fb0_t>(ResolveAddr(Module::Exe, 0x140087fb0));
    s_calcDist8830 = reinterpret_cast<fn_CalcDist8830_t>(ResolveAddr(Module::Exe, 0x140008830));
    s_arrayAdd3110 = reinterpret_cast<fn_ArrayAdd3110_t>(ResolveAddr(Module::Exe, 0x140003110));
    if (s_tidResidence == 0) s_tidResidence = ::Object::GetTypeIdByName("ResidenceComponent");
    s_init = true;
}

inline uintptr_t GW()          { return reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton); }
inline int64_t   TownMgr()     { return *reinterpret_cast<int64_t*>(GW() + 0xf0); }
inline int*      GameObj()     { return *reinterpret_cast<int**>(GW() + 0xf8); }
inline int64_t   LaborerKey()  { return *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(GW() + 0xb0) + 0x30); }
inline void CheckPtr(void* p)  { if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p); }
inline void Ctor(void* self, void* obj) { if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(self, obj); }

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
        CheckPtr(elem);
        void* comp = *reinterpret_cast<void**>(elem);
        uint32_t tid = ((GetTypeId_t)(*reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(comp) + 0x10)))(comp);
        if (tid == s_tidResidence) {
            void* elem2 = reinterpret_cast<void*>(arr + off);
            CheckPtr(elem2);
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
    CheckPtr(&citizen);
    if ((*reinterpret_cast<int64_t*>(citizen + 0x1f0) != profKey) ||
        (CheckPtr(&bldg), *reinterpret_cast<int64_t*>(citizen + 0x248) != bldg)) {
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
        uint32_t j = fn_Random_Rand ? fn_Random_Rand(rng, n) : 0;
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
        CheckPtr(&cz);
        if (AdultIdle(cz)) {
            CheckPtr(&cz);
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
        CheckPtr(&cz);
        if (AdultIdle(cz)) {
            CheckPtr(&cz);
            if (*reinterpret_cast<int64_t*>(cz + 0x1f0) == LaborerKey()) {
                CheckPtr(&cz);
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
        CheckPtr(&cz);
        if (!AdultIdle(cz)) continue;
        CheckPtr(&cz);
        if (*reinterpret_cast<int64_t*>(cz + 0x1f0) != profKey) continue;

        CheckPtr(&cz);
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
        CheckPtr(&res);
        int64_t c7cf0 = 0;                                       // local_98 (IBP)
        CitizenComp_Entity_FindComponent_Storage(res, &c7cf0);
        CheckPtr(&c7cf0);
        int recCount = *reinterpret_cast<int*>(c7cf0 + 0x58);
        int64_t recBase = 0;
        for (int r = 0; r < recCount; ++r) {
            CheckPtr(&c7cf0);
            recBase = *reinterpret_cast<int64_t*>(c7cf0 + 0x50);
            int slotCount = *reinterpret_cast<int*>(recBase + 8 + (int64_t)r * 0x10);
            bool matched = false;
            int64_t memberData = *reinterpret_cast<int64_t*>(recBase + (int64_t)r * 0x10);
            for (int m = 0; m < slotCount; ++m) {
                CheckPtr(&c7cf0);
                recBase = *reinterpret_cast<int64_t*>(c7cf0 + 0x50);
                memberData = *reinterpret_cast<int64_t*>(recBase + (int64_t)r * 0x10);
                int64_t member = *reinterpret_cast<int64_t*>((int64_t)m * 8 + memberData);
                CheckPtr(&member);
                if (AdultIdle(member)) {
                    CheckPtr(&member);
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
                CheckPtr(&c7cf0);
                slotCount = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + 8 + (int64_t)r * 0x10);
            }
            (void)matched;
            CheckPtr(&c7cf0);
            recCount = *reinterpret_cast<int*>(c7cf0 + 0x58);
        }
    }

    // Post-scan: seat from the best donor residence's record bestRecordIdx.
    if (bestRes != 0) {
        CheckPtr(&bestRes);
        void* comp = FindResidenceComp(bestRes);
        int64_t c7cf0 = 0;
        Ctor(&c7cf0, comp);
        CheckPtr(&c7cf0);
        int64_t recAddr = *reinterpret_cast<int64_t*>(c7cf0 + 0x50) + (int64_t)bestRecordIdx * 0x10;
        int slotCount = *reinterpret_cast<int*>(recAddr + 8);
        for (int m = 0; m < slotCount; ++m) {
            CheckPtr(&c7cf0);
            int64_t memberData = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + (int64_t)bestRecordIdx * 0x10);
            int64_t member = *reinterpret_cast<int64_t*>((int64_t)m * 8 + memberData);
            CheckPtr(&member);
            if (*reinterpret_cast<int64_t*>(member + 0x1f0) == LaborerKey()) {
                CheckPtr(&member);
                if (AdultIdle(member)) {
                    SeatWorker(member, *availPtr, profKey);
                    *reinterpret_cast<int64_t*>(out) = member;
                    return out;
                }
            }
            CheckPtr(&c7cf0);
            slotCount = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + 8 + (int64_t)bestRecordIdx * 0x10);
        }
    }

    Ctor(out, nullptr);
    return out;
}

// ============================================================================
// 0x14008c5f0 — strategy 3: shuffle houses, pick the house whose c7cf0 roster record
// has the most idle laborers, then seat one from that record.
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
        CheckPtr(&house);
        void* comp = FindResidenceComp(house);
        int64_t c7cf0 = 0;   // local_70
        Ctor(&c7cf0, comp);
        CheckPtr(&c7cf0);
        int recCount = *reinterpret_cast<int*>(c7cf0 + 0x58);
        for (int r = 0; r < recCount; ++r) {
            int laborerN = 0;   // iVar6
            CheckPtr(&c7cf0);
            int slotCount = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + 8 + (int64_t)r * 0x10);
            for (int m = 0; m < slotCount; ++m) {
                CheckPtr(&c7cf0);
                int64_t memberData = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + (int64_t)r * 0x10);
                int64_t member = *reinterpret_cast<int64_t*>((int64_t)m * 8 + memberData);
                CheckPtr(&member);
                if (AdultIdle(member)) {
                    CheckPtr(&member);
                    if (*reinterpret_cast<int64_t*>(member + 0x1f0) == LaborerKey()) laborerN++;
                }
                CheckPtr(&c7cf0);
                slotCount = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + 8 + (int64_t)r * 0x10);
            }
            if (bestCount < laborerN) { bestHouse = house; bestCount = laborerN; bestRecordIdx = r; }
            CheckPtr(&c7cf0);
            recCount = *reinterpret_cast<int*>(c7cf0 + 0x58);
        }
    }

    if (bestHouse != 0) {
        CheckPtr(&bestHouse);
        void* comp = FindResidenceComp(bestHouse);
        int64_t c7cf0 = 0;
        Ctor(&c7cf0, comp);
        CheckPtr(&c7cf0);
        int slotCount = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + 8 + (int64_t)bestRecordIdx * 0x10);
        for (int m = 0; m < slotCount; ++m) {
            CheckPtr(&c7cf0);
            int64_t memberData = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + (int64_t)bestRecordIdx * 0x10);
            int64_t member = *reinterpret_cast<int64_t*>((int64_t)m * 8 + memberData);
            CheckPtr(&member);
            if (AdultIdle(member)) {
                CheckPtr(&member);
                if (*reinterpret_cast<int64_t*>(member + 0x1f0) == LaborerKey()) {
                    SeatWorker(member, *availPtr, profKey);
                    *reinterpret_cast<int64_t*>(out) = member;
                    return out;
                }
            }
            CheckPtr(&c7cf0);
            slotCount = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(c7cf0 + 0x50) + 8 + (int64_t)bestRecordIdx * 0x10);
        }
    }

    Ctor(out, nullptr);
    return out;
}

bool s_traceInit = false, s_trace = false;
bool Trace() {
    if (!s_traceInit) { const char* e = std::getenv("BANISHED_EMPLOY_TRACE"); s_trace = e && e[0] && e[0] != '0'; s_traceInit = true; }
    return s_trace;
}

} // namespace

// ============================================================================
// 0x140114000 — CitizenComponent_OnAdd: append the citizen to townMgr+0xa0.
// ============================================================================
extern "C" void CitizenComponent_OnAdd(void* citizenComp) {
    EnsureFns();
    if (!citizenComp || !g_pGameWorldSingleton || !*g_pGameWorldSingleton || !s_arrayAdd3110) return;
    int64_t townMgr = TownMgr();
    if (townMgr == 0) return;
    int64_t ref[2] = { 0, 0 };
    Ctor(ref, citizenComp);
    s_arrayAdd3110(reinterpret_cast<int64_t*>(townMgr + 0xa0),
                   *reinterpret_cast<int*>(townMgr + 0xa8) + 1, ref);
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
            if (Trace()) {
                fprintf(stderr, "[EmployCascade] seat #%d citizen=%p prof(+0x1f0)=0x%llx "
                        "workplace(+0x248)=%p residence(+0x238)=%p avail=%p\n",
                        seated, reinterpret_cast<void*>(seatedCz),
                        (unsigned long long)*reinterpret_cast<int64_t*>(seatedCz + 0x1f0),
                        reinterpret_cast<void*>(*reinterpret_cast<int64_t*>(seatedCz + 0x248)),
                        reinterpret_cast<void*>(*reinterpret_cast<int64_t*>(seatedCz + 0x238)),
                        reinterpret_cast<void*>(avail));
                fflush(stderr);
            }
        } else {
            if (Trace()) { fprintf(stderr, "[EmployCascade] seat #%d FAILED (no candidate, avail=%p)\n",
                                   i, reinterpret_cast<void*>(avail)); fflush(stderr); }
            break;   // no candidate this round -> stop
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
