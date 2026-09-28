// code/Game/Component/citizen_reassign_workers.cpp
//
// Faithful port of the town-wide RESIDENCE RE-BALANCER pair:
//   Entity_impl_6c60   (0x1400d6c60)  — per-residence relocation cost
//   FUN_1400d62a0      (0x1400d62a0)  — the re-balancer itself
//
// Reached, once per town tick, via the ProcessPaths drain (before AssignCitizens):
//   CitizenBehavior_Update (0x14008c2b0)
//     -> CitizenBehavior_ProcessPaths (FUN_1400d6f00, citizenbehavior.cpp:402)
//        when pendingIdx < 0:
//          fn_PathGraph_ReassignWorkers(pg)  <-- 0x1400d62a0 (THIS)
//          fn_PathGraph_AssignCitizens(pg)   <-- 0x1400d43b0 (citizen_assign_citizens.cpp)
//          fn_PathPairs_Clear(pg)            <-- 0x140025b30
//
// ============================================================================
// WHAT IT DOES
//   The pass consolidates single-occupant residences: it collects every
//   capacity<2 residence in the town, splits them into
//     - EMPTY ones   (occupant count == 0)  -> "destination" set   (retail tree local_b8)
//     - OCCUPIED ones (occupant count != 0)  -> "candidate" set     (retail tree local_98),
//       keyed by a relocation cost = Entity_impl_6c60(residence -> its own cell),
//   then, walking the candidates from HIGHEST current cost down, tries to move a
//   candidate's occupants into the nearest cheaper empty residence. On a match it
//   re-homes every occupant (CitizenComponent_AssignWorkplace), migrates the
//   residence's stored goods into the destination (StorageComponent add/subtract),
//   and retires the now-occupied destination from the empty set.
//
//   The two retail scratch trees are AVL trees whose node memory nothing outside
//   this function observes, so — exactly as code/Game/Component/citizen_assign_citizens.cpp
//   does for the AssignCitizens allocator — they are modeled here as clean std::
//   containers with the SAME visiting order (pointer tie-breaks are non-observable).
//   Every touch of real game state (residence/occupant/storage fields) and every
//   leaf helper is called wholesale and byte-faithful.
//
//     retail tree local_b8  -> std::set<void*>              (empty residences, by ptr asc)
//     retail tree local_98  -> std::set<CostRes, CostCmp>   (occupied residences; in-order
//                                                            = cost DESC, ptr ASC on ties)
//
// FIELD MAP (verified against Application-x64.exe in Ghidra)
//   gameWorld (*g_pGameWorldSingleton)
//     +0xb0  profession registry (reg)   +0xf0  townManager   +0xf8  pathData(int*)
//   townManager
//     +0x20  residence entity array (ptr)   +0x28  residence count(int)
//   residence entity (ComponentEntity)
//     +0x30  component IBP array (ptr)       +0x38  component count(int)
//   residenceComponent (type-id ResidenceComponent 0x1401c7cf0)
//     +0x10  owner entity IBP                +0x50  occupant list {data@+0,count@+8}
//     +0x58  capacity(int)
//   occupant (CitizenComponent)
//     +0x3c   health value(float)     +0x1f0 profession key
//     +0x208  busy/action ptr         +0x248 workplace-assign IBP
//     +0x288  health component (+0x44 threshold)
//   storageComponent (type-id StorageComponent 0x1401c7e78)
//     +0x28  item-entry array (ptr)   +0x30  item count(int)
//     item entry (stride 0x10): +0x00 descriptor(ptr) +0x08 avail(int) +0x0c reserved(int)
//
// LEAF HELPERS (all reimplemented + registered elsewhere; resolved wholesale):
//   Entity_impl_6c60          0x1400d6c60  — reimplemented in THIS file (called direct)
//   CitizenComponent_AssignWorkplace 0x1401163d0  (reg_citizen_assignworkplace.cpp)
//   StorageComponent_impl_cd80 0x14012cd80  == fn_StorageCanAdd
//   FUN_14012d000              0x14012d000  == fn_StorageAddItem
//   FUN_14012d150              0x14012d150  — storage subtract (resolved here)
//   Entity_FindComponent_Position 0x140002ed0 == fn_Entity_FindComponent_Position
//   Entity_impl_2e20          0x140122e20  == fn_Entity_GetPosition
//   Entity_impl_7fb0          0x140087fb0  == fn_Entity_GetGridId
//   FUN_1400a00f0             0x1400a00f0  == fn_Map_CanPathTo
//   FUN_140008830             0x140008830  == fn_PathDistance
//   type-ids DWORD_1401c7cf0 / DWORD_1401c7e78 (reg_economy_typeids.cpp)
//
// The retail AVL insert/rebalance/remove/free leaves (a8c0/e360/a6e0/e320/14c30)
// only maintained the scratch-tree layout and are therefore intentionally NOT
// called — the std:: containers subsume them.

#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <utility>
#include <vector>
#include "Component/ResidenceComponent.h"   // ENDGAME B.3.9 C1: 0x1401c7cf0 cell class
#include "Component/StorageComponent.h"     // ENDGAME B.3.9 C1: 0x1401c7e78 cell class
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Work/exe_bodies.h"

// Build-time roster-only guard, shared with citizen_assign_citizens.cpp: while set,
// the workplace/housing attach-detach helpers skip the Graphics::Scene mesh toggle
// down to the bare roster mutation (the scene churn that can deadlock the build). We
// hold it across each relocation's AssignWorkplace churn for the same reason the
// AssignCitizens pass (which runs immediately after us on the same drain) holds it.
extern bool g_wpcRosterOnly;

namespace {


// The consumer leaves are called by name: CitizenComponent_AssignWorkplace
// (0x1401163d0) and ActRm_StorageSubtract_d150 (0x14012d150).
uint32_t             s_tidResidence    = 0;
uint32_t             s_tidStorage      = 0;
bool                 s_resolved        = false;

// Resolves no door: it only caches the two component type ids.
void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;
    // ENDGAME B.3.9 (B39_RULES.md D5): 0x1401c7cf0 / 0x1401c7e78 are registered
    // type-id cells (INVENTORY_C Table 1: ResidenceComponent, StorageComponent).
    s_tidResidence = ResidenceComponent::TypeId();
    s_tidStorage   = StorageComponent::TypeId();
}

// Find the first component on `entity` whose exact type-id == typeId and that passes
// IsDerivedFrom (retail's `GetTypeId()==DWORD && IsDerivedFrom` component scan).
void* FindComponentDerived(void* entity, uint32_t typeId) {
    if (!entity || typeId == 0) return nullptr;
    uintptr_t e = reinterpret_cast<uintptr_t>(entity);
    int count = *reinterpret_cast<int*>(e + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(e + 0x30);
    for (int i = 0; i < count; ++i) {
        void* comp = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (!comp) continue;
        uint32_t id = reinterpret_cast<Object::Base*>(comp)->Type();   // vtable slot +0x10   // B.3.8 bridge (slot 2)
        if (id == typeId) {
            return Object::IsDerivedFromNullSafe(comp, typeId) ? comp : nullptr;
        }
    }
    return nullptr;
}

// Candidate key: retail tree local_98 visits IN-ORDER as (cost DESC, residence ptr ASC).
struct CostRes { int cost; void* res; };
struct CostCmp {
    bool operator()(const CostRes& a, const CostRes& b) const {
        if (a.cost != b.cost) return a.cost > b.cost;   // higher cost visited first
        return a.res < b.res;                           // ties: ascending pointer
    }
};

}  // namespace

// ===========================================================================
// Entity_impl_6c60 (0x1400d6c60) — relocation cost for a residence's occupants
// ===========================================================================
// param_2 -> residence whose mis-professioned, employed, healthy, idle occupants
//            are summed; param_3 -> residence supplying the TARGET cell (its owner
//            entity's position). Returns the summed path distance (0 when none
//            qualify / unreachable). param_1 is dead (retail keeps it for ABI).
extern "C" uint64_t Entity_ReassignCost_6c60(int64_t /*param_1*/, void** param_2, void** param_3) {
    if (GameUpdateLoop::_singleton == nullptr) return 0;
    uintptr_t gw = reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton);
    int* pathData = *reinterpret_cast<int**>(gw + 0xf8);
    uintptr_t reg = *reinterpret_cast<uintptr_t*>(gw + 0xb0);
    if (!pathData || !reg) return 0;

    // --- target cell from param_3's owner-entity position ---
    void* ownerEntity = *reinterpret_cast<void**>(
        *reinterpret_cast<uintptr_t*>(param_3) + 0x10);
    int64_t posIBP[1] = {0};
    void* posComp = WorkBase_FindComponent_2ed0(reinterpret_cast<uintptr_t>(ownerEntity), posIBP);
    float pos[4] = {0.0f, 0.0f, 0.0f, 0.0f};   // pos[0]=x (local_48), pos[1]=y (local_44)
    if (posComp)
        Pf_EntityResolvePos_2e20(*reinterpret_cast<void**>(posComp), pos, 0, 0, nullptr);

    int width = *pathData;
    if (width <= 0) return 0;
    int cellX = static_cast<int>(pos[0] + static_cast<float>(width >> 1));
    int cellY = static_cast<int>(pos[1] + static_cast<float>(width >> 1));
    if (cellX < 0) cellX = 0;
    if (width <= cellX) cellX = width - 1;
    if (cellY < 0) cellY = 0;
    if (width <= cellY) cellY = width - 1;
    uint32_t targetCell = static_cast<uint32_t>(width * cellY + cellX);

    // --- sum path cost of each qualifying occupant of param_2 to targetCell ---
    uint32_t total = 0;
    int i = 0;
    for (;;) {
        uintptr_t comp2   = reinterpret_cast<uintptr_t>(*param_2);
        uintptr_t occList = *reinterpret_cast<uintptr_t*>(comp2 + 0x50);
        if (!(i < *reinterpret_cast<int*>(occList + 8))) break;
        uintptr_t data = *reinterpret_cast<uintptr_t*>(occList);
        uint8_t*  worker = *reinterpret_cast<uint8_t**>(data + static_cast<uintptr_t>(i) * 8);

        void* healthComp = *reinterpret_cast<void**>(worker + 0x288);
        bool healthy = healthComp &&
            *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(healthComp) + 0x44) <=
            *reinterpret_cast<float*>(worker + 0x3c);
        if (healthy &&
            *reinterpret_cast<int64_t*>(worker + 0x208) == 0 &&               // not busy
            *reinterpret_cast<int64_t*>(worker + 0x248) != 0 &&               // has a workplace
            *reinterpret_cast<int64_t*>(worker + 0x1f0) !=                    // profession != default
                *reinterpret_cast<int64_t*>(reg + 0x30)) {
            void* workplaceIBP = *reinterpret_cast<void**>(worker + 0x248);
            uint32_t wpCell = static_cast<uint32_t>(Pf_EntityAnchorCell_7fb0(&workplaceIBP, nullptr));
            // WorkBase_ValidateCell_a00f0 returns its bool in the low byte of a uint64.
            if (static_cast<uint8_t>(WorkBase_ValidateCell_a00f0(pathData, 0xe1f, targetCell,
                                                                  static_cast<int>(wpCell)))) {
                total += static_cast<uint32_t>(WorkBase_CalcDistance_8830(pathData, targetCell, wpCell));
            }
        }
        ++i;
    }
    return total;
}

// ===========================================================================
// FUN_1400d62a0 (0x1400d62a0) — town residence re-balancer
// ===========================================================================
extern "C" void PathGraph_ReassignWorkers(void* ctxV) {
    if (GameUpdateLoop::_singleton == nullptr) return;
    EnsureResolved();
    if (!s_tidResidence) {
        return;
    }

    uintptr_t gw       = reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton);
    uintptr_t townMgr  = *reinterpret_cast<uintptr_t*>(gw + 0xf0);
    if (!townMgr) return;
    int       resCount = *reinterpret_cast<int*>(townMgr + 0x28);
    uintptr_t resArray = *reinterpret_cast<uintptr_t*>(townMgr + 0x20);

    std::set<void*>            empties;   // retail local_b8: available (occupant==0) residences
    std::set<CostRes, CostCmp> occupied;  // retail local_98: keyed by relocation cost

    // ---------- Phase 1: bucket every capacity<2 residence ----------
    for (int r = 0; r < resCount; ++r) {
        void* resEntity = *reinterpret_cast<void**>(resArray + static_cast<uintptr_t>(r) * 8);
        void* resComp = FindComponentDerived(resEntity, s_tidResidence);
        if (!resComp) continue;                       // retail assumes present; guard for safety

        if (*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(resComp) + 0x58) >= 2)
            continue;                                 // only single-occupant residences

        uintptr_t occList  = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(resComp) + 0x50);
        int       occCount = *reinterpret_cast<int*>(occList + 8);
        if (occCount == 0) {
            empties.insert(resComp);
        } else {
            void* rc2 = resComp;
            void* rc3 = resComp;
            uint32_t cost = static_cast<uint32_t>(
                Entity_ReassignCost_6c60(static_cast<int64_t>(occList), &rc2, &rc3));
            occupied.insert(CostRes{ static_cast<int>(cost), resComp });
        }
    }

    int relocations = 0;

    // ---------- Phase 2: greedily consolidate ----------
    // Hold the roster-only guard across every AssignWorkplace churn below.
    if (!empties.empty() && !occupied.empty()) {
        bool prevRoster = g_wpcRosterOnly;
        g_wpcRosterOnly = true;

        for (auto it = occupied.begin(); it != occupied.end(); ++it) {
            void* srcRes    = it->res;            // residence to (maybe) vacate (tree local_98 node)
            int   threshold = it->cost;           // its own current cost (retail uVar13)
            void* bestDest  = nullptr;            // retail local_res18

            // nearest empty residence whose relocation cost beats `threshold`
            for (void* destRes : empties) {       // ascending pointer == retail in-order walk
                void* b = srcRes;
                void* a = destRes;
                int c = static_cast<int>(
                    Entity_ReassignCost_6c60(reinterpret_cast<int64_t>(ctxV), &b, &a));
                if (c < threshold) { threshold = c; bestDest = destRes; }
            }
            if (!bestDest) continue;

            // --- re-home every occupant of srcRes into bestDest's owner entity ---
            // Re-read the list header/count each step: AssignWorkplace may mutate the
            // occupant roster, exactly as the retail index-by-8 loop tolerates.
            int j = 0;
            for (;;) {
                uintptr_t occList = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(srcRes) + 0x50);
                if (!(j < *reinterpret_cast<int*>(occList + 8))) break;
                uintptr_t data   = *reinterpret_cast<uintptr_t*>(occList);
                void* worker     = *reinterpret_cast<void**>(data + static_cast<uintptr_t>(j) * 8);
                void* targetOwner = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(bestDest) + 0x10);
                CitizenComponent_AssignWorkplace(worker, &targetOwner, 0);
                ++j;
            }

            // --- migrate stored goods: srcRes owner storage -> bestDest owner storage ---
            void* srcOwner  = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(srcRes) + 0x10);
            void* srcStorage = FindComponentDerived(srcOwner, s_tidStorage);
            void* dstOwner  = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(bestDest) + 0x10);
            void* dstStorage = FindComponentDerived(dstOwner, s_tidStorage);

            if (srcStorage && dstStorage) {
                std::vector<std::pair<void*, int>> toSubtract;
                int itemCount = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(srcStorage) + 0x30);
                uintptr_t itemBase = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(srcStorage) + 0x28);
                for (int k = 0; k < itemCount; ++k) {
                    uintptr_t entry = itemBase + static_cast<uintptr_t>(k) * 0x10;
                    void* descriptor = *reinterpret_cast<void**>(entry + 0x00);
                    int   avail      = *reinterpret_cast<int*>(entry + 0x08);
                    int   reserved   = *reinterpret_cast<int*>(entry + 0x0c);
                    int   amount     = avail - reserved;
                    if (Pf_StorageCanAccept_cd80(reinterpret_cast<int64_t>(dstStorage),
                                                 reinterpret_cast<int64_t>(descriptor), amount)) {
                        ActRm_StorageAdd_d000(reinterpret_cast<uintptr_t>(dstStorage),
                                              reinterpret_cast<int64_t>(descriptor), amount);
                        toSubtract.emplace_back(descriptor, amount);
                    }
                }
                for (auto& s : toSubtract)
                    ActRm_StorageSubtract_d150(reinterpret_cast<uintptr_t>(srcStorage),
                                               reinterpret_cast<int64_t>(s.first), s.second);
            }

            // --- retire the now-occupied destination from the empty set ---
            empties.erase(bestDest);
            ++relocations;
            if (empties.empty()) break;           // retail: local_b0 == 0
        }

        g_wpcRosterOnly = prevRoster;
    }
}
