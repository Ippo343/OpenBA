// code/Game/Component/citizen_assign_citizens.cpp
//
// Full faithful port of CitizenComponent_impl_43b0 (0x1400d43b0) — the town-wide
// EMPLOYMENT ALLOCATOR. Runs once per town tick, reached via
//   CitizenBehavior_Update (0x14008c2b0)
//     -> ProcessPaths (FUN_1400d6f00, resolves BuildStructure paths first)
//       -> CitizenComponent_impl_43b0(ctx)   <-- THIS function
//       -> FUN_140025b30 (clear the path pairs scratch array)
//
// The single argument `ctx` is the PathGraph context. Its only field this function
// touches is the shared cost cache tree at ctx+0x18, which is owned/read by the
// caller (FUN_1400d6f00) and by sub_6060; we never touch it directly — we hand `ctx`
// straight to sub_6060, which maintains it.
//
// ============================================================================
// WHAT IT DOES (6 phases)
//   The retail function is a ~1351-line inlined-STL allocator building four
//   red-black/AVL scratch containers. Nothing outside the function reads their
//   memory layout, so this port represents them as clean std:: containers with the
//   same comparators (behavior-faithful; pointer tie-breaks are non-observable and
//   differ from retail's heap addresses anyway). Every interaction with real game
//   state (citizen/townMgr/reg/workplace fields) and every leaf helper is called
//   wholesale and byte-faithful.
//
//   Phase 1  Housing-sync: for each idle+healthy citizen already assigned to a
//            workplace (+0x248) whose required profession (workplaceComp+0x48) no
//            longer matches the citizen's profession (+0x1f0), unassign it —
//            AssignHousing(citizen, null) + set +0x1f0 = default key (reg+0x30).
//   Phase 2  Need-table: for every profession in the registry (reg+0x20 array,
//            reg+0x28 count), look it up in the town workplace map (townMgr+0x100)
//            to get the list of workplace entities for that profession; for each
//            that has a WorkPlaceComponent with capacity (+0x60 != 0) push a
//            NeedEntry{workplaceComp, cellId, remaining=cap, capacity=cap, costTree}.
//   Phase 3  Idle pass: for each idle+healthy citizen — normalize (same as phase 1),
//            insert into the idle-laborer set, bump the per-profession count map
//            (keyed by +0x1f0), and add a {cost,citizen} candidate (cost from
//            sub_6060) to every NeedEntry's cost-tree.
//   Phase 4  Priority: score each NeedEntry = (int)(minCost * (uvScale - filled)),
//            filled = (cap-remaining)/cap; insert into a max-priority queue.
//   Phase 5A Assign loop: while the queue is non-empty, pop the highest-score need,
//            take its nearest available candidate (min cost), assign the citizen —
//            AssignHousing(citizen, workplaceEntity) + set +0x1f0 = reqProf — then
//            remove the citizen from the idle set, decrement its profession count,
//            drop that citizen from EVERY need's cost-tree, decrement the need's
//            remaining, and re-score any need whose best candidate changed.
//   Phase 5B Cleanup loop: drain the leftover profession-count map, clearing the
//            workplace (AssignHousing null) of each still-idle laborer.
//
// FIELD MAP (verified against Application-x64.exe in Ghidra; see below)
//   gameWorld (*g_pGameWorldSingleton)
//     +0xb0  profession registry   +0xf0  townManager   +0xf8  pathData(int*)
//   registry (reg)
//     +0x20  professionId array (ptr)   +0x28  count(int)   +0x30  default key
//   townManager
//     +0xa0  citizen array (ptr)   +0xa8  citizen count(int)
//     +0x100 workplace map root (key=professionId; node +0x00 key, +0x08 list,
//            +0x10 count, +0x20 left, +0x28 right; list = array of workplace IBPs)
//   citizen (CitizenComponent)
//     +0x3c   health value(float)     +0x1f0 profession key
//     +0x208  busy/action ptr         +0x238 residence entity IBP (source cell)
//     +0x248  workplace-assign IBP    +0x288 health component (+0x44 threshold)
//   workplaceComponent (the c80e0 component FindWorkplaceComponent returns)
//     +0x10   owner entity IBP        +0x48  required profession key   +0x60 capacity
//
//   AssignHousing (0x140111ee0) writes citizen+0x248: detaches from the old
//   workplace's worker roster (FUN_14012f7e0) and attaches to the new (FUN_14012fa20).
//
// LEAF HELPERS called wholesale (resolved via ResolveAddr(Module::Exe, ...)):
//   sub_6060  0x1400d6060  path-cost record builder; fills {citizen, fromCell,
//             toCell, cost}; caches by region-pair in ctx+0x18 (shared w/ caller).
//   AssignHousing        0x140111ee0
//   FindWorkplaceComponent 0x140002e10  (member ABI: RCX=entity, RDX=out IBP)
//   Entity_impl_7fb0     0x140087fb0  grid cell id of an entity (Position component)
//   g_fUVScale           0x14017214c  (float const in the score formula)
//
// WIRING NOTE: replaces the notes/ scoped stand-in (construction-only Laborer->Builder).
// In SA this must be registered (RegisterExeAddr 0x1400d43b0) AND the caller chain
// FUN_1400d6f00 + ctx must reach it (see task #16). It is a direct (non-vtable) call,
// exercised via SA registration / call-site redirection.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <map>
#include <set>
#include <vector>

// ---------------------------------------------------------------------------
// trace gate (shares BANISHED_BUILD_TRACE with the construction pipeline)
// ---------------------------------------------------------------------------
static int s_cac_trace = -1;
static bool CAC_Trace() {
    if (s_cac_trace < 0) {
        const char* e = std::getenv("BANISHED_BUILD_TRACE");
        s_cac_trace = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s_cac_trace == 1;
}

namespace {

// ---- leaf helper typedefs --------------------------------------------------
typedef void  (*fn_AssignHousing_t)(void* citizen, void* ibpArg);   // 0x140111ee0
typedef void* (*fn_FindWorkplace_t)(int64_t entity, void* outIBP);  // 0x140002e10 member ABI
typedef int   (*fn_CellOf_t)(void* entityIBP, float* outPos);       // 0x140087fb0

fn_AssignHousing_t s_assignHousing = nullptr;
fn_FindWorkplace_t s_findWorkplace = nullptr;
fn_CellOf_t        s_cellOf        = nullptr;
const float*       s_uvScale       = nullptr;
bool               s_resolved      = false;

template <class T> inline T R(uintptr_t a) {
    return reinterpret_cast<T>(ResolveAddr(Module::Exe, a));
}

void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;
    s_assignHousing = R<fn_AssignHousing_t>(0x140111ee0);
    s_findWorkplace = R<fn_FindWorkplace_t>(0x140002e10);
    s_cellOf        = R<fn_CellOf_t>(0x140087fb0);
    s_uvScale       = R<const float*>(0x14017214c);
}

// Path cost between two grid cells — self-contained equivalent of sub_6060
// (0x1400d6060) minus its region-pair cache (a pure optimization that returns the
// same value). Uses the same helpers ProcessPaths uses, so it works in standalone.
//   - unreachable source, or Map_CanPathTo says no route -> 0x1000000 (never chosen)
//   - either endpoint in an invalid region -> Euclidean grid distance
//   - otherwise CalcPathCost; if < 2 (adjacent/degenerate) -> Euclidean
static const int kUnreachable = 0x1000000;
int ComputeCost(int32_t* pathData, int fromCell, int toCell) {
    if (fromCell < 0 || toCell < 0) return kUnreachable;
    if (fn_Map_CanPathTo &&
        !fn_Map_CanPathTo(pathData, 0xe1f, static_cast<uint32_t>(fromCell),
                          static_cast<uint32_t>(toCell)))
        return kUnreachable;

    int W = pathData[0];
    if (W <= 0) return kUnreachable;
    auto euclid = [&]() -> int {
        int dx = fromCell / W - toCell / W;
        int dy = fromCell % W - toCell % W;
        int d = static_cast<int>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));
        return d < 0 ? kUnreachable : d;
    };

    int64_t nav = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(pathData) + 0x80);
    int regionA = *reinterpret_cast<int*>(nav + 0x18 + static_cast<int64_t>(fromCell) * 0x30);
    int regionB = *reinterpret_cast<int*>(nav + 0x18 + static_cast<int64_t>(toCell)   * 0x30);
    if (regionA < 0 || regionB < 0) return euclid();

    int cost = fn_Map_CalcPathCost
                 ? fn_Map_CalcPathCost(pathData, static_cast<uint32_t>(fromCell), toCell, 0)
                 : 0;
    if (cost < 2) cost = euclid();
    return cost;
}

inline void CheckPtr(void* ibp) {
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ibp);
}

// Read a citizen out of the townMgr citizen array, resolving the lazy IBP.
inline void* CitizenAt(uintptr_t citArray, int i) {
    void* c = *reinterpret_cast<void**>(citArray + static_cast<int64_t>(i) * 8);
    CheckPtr(&c);
    return c;
}

// idle + healthy + not-busy gate (identical in phase 1 and phase 3).
inline bool IsIdleHealthy(void* citizenV) {
    uint8_t* c = reinterpret_cast<uint8_t*>(citizenV);
    void* healthComp = *reinterpret_cast<void**>(c + 0x288);
    if (!healthComp) return false;
    if (*reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(healthComp) + 0x44) >
        *reinterpret_cast<float*>(c + 0x3c))
        return false;                                   // sick/starving
    if (*reinterpret_cast<void**>(c + 0x208) != 0) return false;  // busy in an action
    return true;
}

// Find the workplace component inside an assigned entity (+0x248 value), or null.
inline void* FindWorkplaceComp(void* entityRaw) {
    if (!entityRaw || !s_findWorkplace) return nullptr;
    void* raw = entityRaw;
    CheckPtr(&raw);
    int64_t outIBP[2] = {0, 0};
    s_findWorkplace(reinterpret_cast<int64_t>(raw), outIBP);
    void* comp = reinterpret_cast<void*>(outIBP[0]);
    CheckPtr(&comp);
    return comp;
}

// Grid cell id of an entity (its Position component). Returns -1 when absent.
inline int CellOfEntity(void* entityRaw) {
    if (!entityRaw || !s_cellOf) return -1;
    void* raw = entityRaw;
    return s_cellOf(&raw, nullptr);
}

// Phase-1/3 normalization: an idle citizen whose current workplace requires a
// profession different from its own is unassigned back to the default laborer key.
void NormalizeMismatched(void* citizenV, uintptr_t reg) {
    uint8_t* c = reinterpret_cast<uint8_t*>(citizenV);
    void* assigned = *reinterpret_cast<void**>(c + 0x248);
    if (!assigned) return;
    void* wpComp = FindWorkplaceComp(assigned);
    if (!wpComp) return;                                // no workplace comp -> nothing to sync
    int64_t reqProf   = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(wpComp) + 0x48);
    int64_t curProf   = *reinterpret_cast<int64_t*>(c + 0x1f0);
    if (reqProf == curProf) return;                     // already matches

    int64_t defaultKey = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(reg) + 0x30);
    // skip if already reset (default profession AND no workplace)
    if (curProf == defaultKey && *reinterpret_cast<int64_t*>(c + 0x248) == 0) return;

    int64_t nullWp = 0;
    if (s_assignHousing) s_assignHousing(citizenV, &nullWp);   // clear +0x248
    *reinterpret_cast<int64_t*>(c + 0x1f0) = defaultKey;
}

// Walk the town workplace map (townMgr+0x100), keyed by profession id.
// Returns {listPtr, count} for `key`, or {0,0} if absent.
struct WorkplaceList { uintptr_t list; int count; };
WorkplaceList LookupWorkplaces(uintptr_t mapRoot, uint64_t key) {
    uint64_t* node = reinterpret_cast<uint64_t*>(mapRoot);
    while (node) {
        if (key < node[0])       node = reinterpret_cast<uint64_t*>(node[4]);   // left
        else if (key > node[0])  node = reinterpret_cast<uint64_t*>(node[5]);   // right
        else return { static_cast<uintptr_t>(node[1]),                          // list @ +0x08
                      static_cast<int>(node[2]) };                              // count @ +0x10
    }
    return { 0, 0 };
}

// ---- scratch containers ----------------------------------------------------
struct Candidate {
    int32_t cost;
    void*   citizen;
    int32_t fromCell;
    int32_t toCell;
    // Ordered by (cost, citizen) — matches the retail cost-tree comparator; the
    // nearest laborer is begin() (min cost), the priority score reads begin()->cost.
    bool operator<(const Candidate& o) const {
        if (cost != o.cost) return cost < o.cost;
        return citizen < o.citizen;
    }
};

struct NeedEntry {
    void* workplaceComp;   // c80e0 component (owner entity @ +0x10, reqProf @ +0x48)
    int   cellId;          // workplace grid cell
    int   remaining;       // open slots (starts = capacity, decremented on assign)
    int   capacity;        // total slots
    std::multiset<Candidate> candidates;   // available laborers, by (cost, citizen)

    int score(float uvScale) const {
        if (candidates.empty() || capacity <= 0) return 0;
        int minCost = candidates.begin()->cost;
        float filled = static_cast<float>(capacity - remaining) / static_cast<float>(capacity);
        return static_cast<int>(static_cast<float>(minCost) * (uvScale - filled));
    }
};

// Remove the candidate for `citizen` from a need's cost-tree (linear; N is small).
// Returns true if the removed candidate had been the need's best (begin()).
bool DropCitizen(NeedEntry& e, void* citizen) {
    for (auto it = e.candidates.begin(); it != e.candidates.end(); ++it) {
        if (it->citizen == citizen) {
            bool wasBest = (it == e.candidates.begin());
            e.candidates.erase(it);
            return wasBest;
        }
    }
    return false;
}

}  // namespace

// Build-time roster-only guard (workplacecomponent.cpp). When set, the WPC worker
// attach/detach helpers (fa20/f7e0, reached through AssignHousing 0x140111ee0) skip
// the Graphics::Scene mesh-visual toggle (Scene::Add/RemoveObject on gw+0x60 +
// FUN_1400c3470 re-bucket + component-event broadcast) down to the bare roster-list
// mutation. This allocator runs every town tick once townMgr+0x100 has any employer
// workplace (e.g. a registered construction site), and its Phase 1/3/5A/5B AssignHousing
// churn is O(idle-laborers) per tick — the exact scene churn that deadlocks. We hold the
// guard across the whole allocation body so none of those AssignHousing calls touch the
// scene. Verified sufficient in Ghidra (see reference_builder_profession_bst_flow).
extern bool g_wpcRosterOnly;

// ===========================================================================
// CitizenComponent_impl_43b0 (0x1400d43b0)
// ===========================================================================
extern "C" void CitizenAssignCitizens(void* ctxV) {
    if (CAC_Trace()) std::fprintf(stderr, "[BuildTrace] AssignCitizens: ENTER\n");
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    EnsureResolved();
    if (!s_assignHousing || !s_findWorkplace || !s_cellOf) {
        if (CAC_Trace()) std::fprintf(stderr, "[BuildTrace] AssignCitizens: early-return (helpers ah=%p fw=%p cell=%p)\n",
                                      (void*)s_assignHousing, (void*)s_findWorkplace, (void*)s_cellOf);
        return;
    }
    (void)ctxV;   // ctx (PathGraph) — its cache is subsumed by ComputeCost

    uintptr_t gw  = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    uintptr_t reg      = *reinterpret_cast<uintptr_t*>(gw + 0xb0);
    uintptr_t townMgr  = *reinterpret_cast<uintptr_t*>(gw + 0xf0);
    int32_t*  pathData = *reinterpret_cast<int32_t**>(gw + 0xf8);
    if (!reg || !townMgr || !pathData) return;

    uintptr_t citArray = *reinterpret_cast<uintptr_t*>(townMgr + 0xa0);
    int       citCount = *reinterpret_cast<int32_t*>(townMgr + 0xa8);
    uintptr_t mapRoot  = *reinterpret_cast<uintptr_t*>(townMgr + 0x100);
    float     uvScale  = s_uvScale ? *s_uvScale : 1.0f;

    // Hold the roster-only guard for every AssignHousing call below (no early returns
    // remain past this point). Restored just before the trace/return.
    bool prevRoster = g_wpcRosterOnly;
    g_wpcRosterOnly = true;

    // ---------- Phase 1: housing-sync ----------
    for (int i = 0; i < citCount; ++i) {
        void* c = CitizenAt(citArray, i);
        if (!c) continue;
        if (!IsIdleHealthy(c)) continue;
        NormalizeMismatched(c, reg);
    }

    // ---------- Phase 2: need-table ----------
    std::vector<NeedEntry> needs;
    uintptr_t profArray = *reinterpret_cast<uintptr_t*>(reg + 0x20);
    int       profCount = *reinterpret_cast<int32_t*>(reg + 0x28);
    for (int p = 0; p < profCount; ++p) {
        uint64_t key = *reinterpret_cast<uint64_t*>(profArray + static_cast<int64_t>(p) * 8);
        WorkplaceList wl = LookupWorkplaces(mapRoot, key);
        for (int j = 0; j < wl.count; ++j) {
            void* entity = *reinterpret_cast<void**>(wl.list + static_cast<int64_t>(j) * 8);
            CheckPtr(&entity);
            void* wpComp = FindWorkplaceComp(entity);
            if (!wpComp) continue;
            int cap = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(wpComp) + 0x60);
            if (cap == 0) continue;
            NeedEntry e;
            e.workplaceComp = wpComp;
            e.cellId    = CellOfEntity(entity);
            e.remaining = cap;
            e.capacity  = cap;
            needs.push_back(std::move(e));
        }
    }

    // ---------- Phase 3: idle set + profession counts + candidates ----------
    std::set<void*> idle;
    std::map<int64_t, int> profNeed;    // profession key -> idle-laborer count
    for (int i = 0; i < citCount; ++i) {
        void* c = CitizenAt(citArray, i);
        if (!c) continue;
        if (!IsIdleHealthy(c)) continue;

        idle.insert(c);
        int64_t prof = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(c) + 0x1f0);
        ++profNeed[prof];

        NormalizeMismatched(c, reg);    // embedded normalization (mirrors LAB_1400d4e57)

        // source cell = the citizen's residence (+0x238) grid cell, or -1
        void* residence = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(c) + 0x238);
        int srcCell = residence ? CellOfEntity(residence) : -1;

        for (auto& e : needs) {
            int cost = ComputeCost(pathData, srcCell, e.cellId);
            e.candidates.insert(Candidate{ cost, c, srcCell, e.cellId });
        }
    }

    // ---------- Phase 4: max-priority queue ----------
    // multimap<score, needIndex>; the greatest key is the highest priority.
    std::multimap<int, size_t> pq;
    for (size_t n = 0; n < needs.size(); ++n) {
        if (needs[n].candidates.empty() || needs[n].remaining <= 0) continue;
        pq.emplace(needs[n].score(uvScale), n);
    }

    int assignedTotal = 0;

    // ---------- Phase 5A: assign loop ----------
    while (!pq.empty()) {
        auto top = std::prev(pq.end());        // greatest score
        size_t ni = top->second;
        pq.erase(top);
        NeedEntry& e = needs[ni];
        if (e.candidates.empty() || e.remaining <= 0) continue;

        void* citizen = e.candidates.begin()->citizen;   // nearest available laborer
        uint8_t* wp   = reinterpret_cast<uint8_t*>(e.workplaceComp);
        int64_t reqProf   = *reinterpret_cast<int64_t*>(wp + 0x48);
        int64_t wpEntity  = *reinterpret_cast<int64_t*>(wp + 0x10);   // owner entity IBP value

        uint8_t* c = reinterpret_cast<uint8_t*>(citizen);
        if (*reinterpret_cast<int64_t*>(c + 0x1f0) != reqProf ||
            *reinterpret_cast<int64_t*>(c + 0x248) != wpEntity) {
            int64_t wpArg = wpEntity;
            s_assignHousing(citizen, &wpArg);                        // set +0x248 = workplace
            *reinterpret_cast<int64_t*>(c + 0x1f0) = reqProf;        // set profession
        }

        // decrement this citizen's profession need
        // Retail decrements the count under the workplace's required profession
        // (= the citizen's post-assignment +0x1f0), which is what we look up here.
        auto pit = profNeed.find(*reinterpret_cast<int64_t*>(c + 0x1f0));
        if (pit != profNeed.end() && --pit->second == 0) profNeed.erase(pit);
        idle.erase(citizen);
        e.remaining -= 1;
        ++assignedTotal;

        // Remove the now-employed citizen from EVERY need's candidate set; re-score
        // any need whose best candidate changed (and still has open slots).
        for (size_t f = 0; f < needs.size(); ++f) {
            NeedEntry& nf = needs[f];
            if (nf.remaining <= 0) continue;
            bool wasBest = DropCitizen(nf, citizen);
            if (wasBest) {
                // erase nf's stale queue entries and re-insert with the new score
                for (auto it = pq.begin(); it != pq.end();) {
                    if (it->second == f) it = pq.erase(it); else ++it;
                }
                if (!nf.candidates.empty() && nf.remaining > 0)
                    pq.emplace(nf.score(uvScale), f);
            }
        }
    }

    // ---------- Phase 5B: cleanup — clear workplace of leftover idle laborers ----------
    while (!profNeed.empty() && !idle.empty()) {
        int64_t key = profNeed.rbegin()->first;      // greatest profession key
        void*   c   = *idle.begin();                 // smallest idle citizen ptr
        uint8_t* cb = reinterpret_cast<uint8_t*>(c);

        if (*reinterpret_cast<int64_t*>(cb + 0x1f0) != key ||
            *reinterpret_cast<int64_t*>(cb + 0x248) != 0) {
            int64_t nullWp = 0;
            s_assignHousing(c, &nullWp);
            *reinterpret_cast<int64_t*>(cb + 0x1f0) = key;
        }
        idle.erase(idle.begin());
        auto pit = profNeed.find(key);
        if (pit != profNeed.end() && --pit->second == 0) profNeed.erase(pit);
    }

    g_wpcRosterOnly = prevRoster;

    if (CAC_Trace())
        std::fprintf(stderr,
            "[BuildTrace] AssignCitizens: %zu needs, assigned %d worker(s)\n",
            needs.size(), assignedTotal);
}

// ===========================================================================
// Standalone registration
// ===========================================================================
#include "address_registry.h"
extern "C" void RegisterCitizenAssignCitizens() {
    RegisterExeAddr(0x1400d43b0, reinterpret_cast<uintptr_t>(&CitizenAssignCitizens));
}
