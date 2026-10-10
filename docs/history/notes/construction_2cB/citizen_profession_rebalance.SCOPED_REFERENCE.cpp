// code/Game/Component/citizen_profession_rebalance.cpp
//
// Profession/labor rebalancer — SA reimplementation of the tail of the town-tick
// labor allocator. In retail this is `CitizenComponent_impl_43b0` (0x1400d43b0),
// reached each town tick via CitizenBehavior_Update (0x14008c2b0) -> ProcessPaths
// (0x1400d6f00) -> fn_PathGraph_AssignCitizens. That function resolves to NULL in the
// standalone exe (unregistered — reg_citizenbehavior_tick.cpp lists it "NULL -> no-op"),
// so idle Laborers are NEVER assigned a profession. The construction pipeline depends on
// this: a citizen only claims a BuildStructure work when its profession key
// `citizen+0x1f0` equals the Builder marker `*(*(gw+0xb0)+0x38)` (the idle claim walk in
// citizenai_housing_idle.cpp routes such a citizen, via helper_7550 state 0x0d, to the
// scheduler's construction category tree). With the rebalancer a no-op, no laborer ever
// becomes a Builder, so build labor never starts (BuildStructure::Execute fires 0x).
//
// ============================================================================
// SCOPE / FIDELITY NOTE (read before extending):
//   The retail 0x1400d43b0 is a ~1351-line AVL-tree labor allocator: it builds four
//   balanced trees (idle-laborer set, per-profession need counts, a distance-scored
//   priority queue, and a per-profession workplace-array table), then pops the highest
//   priority workplace and assigns the NEAREST available idle laborer, generically across
//   ALL professions (farmer/forester/builder/...), maintaining housing/workplace indices.
//   Porting all of that faithfully (~15 tree/helper deps, several unregistered in SA) was
//   out of scope for one pass. THIS file reproduces the observable effect for the
//   CONSTRUCTION case only, faithfully to the key assignment the retail function performs
//   (`citizen+0x1f0 = <workplace-required-profession>` + AssignHousing), but WITHOUT the
//   AVL distance/priority optimisation (it assigns the first eligible idle Laborers rather
//   than the nearest) and WITHOUT the generic per-profession handling. It only promotes
//   Laborers -> Builder while construction is pending; every other profession is left to
//   the existing paths. When the full allocator is ported, replace this file's body.
// ============================================================================
//
// Faithful pieces preserved from 0x1400d43b0:
//   * citizen iteration over townMgr(gw+0xf0)+0xa0 array / +0xa8 count.
//   * idle+healthy+laborer gate: *(citizen+0x288)->+0x44 <= *(citizen+0x3c) (health),
//     *(citizen+0x208) == 0 (not busy), *(citizen+0x1f0) == *(reg+0x30) (currently Laborer).
//   * assignment: *(citizen+0x1f0) = builderMarker (= a construction workplace's required
//     profession) paired with CitizenComponent_AssignHousing, exactly as every +0x1f0
//     writer in the retail code does.
//   * pending-construction gate via townMgr+0xd8 (the +0xd0 build registry count that
//     BuildComponent State1Enter fills and State8OneShot drains).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---- trace gate --------------------------------------------------------------
static int s_cpr_trace = -1;
static bool CPR_Trace() {
    if (s_cpr_trace < 0) {
        const char* e = std::getenv("BANISHED_BUILD_TRACE");
        s_cpr_trace = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s_cpr_trace == 1;
}

// ---- exe helpers -------------------------------------------------------------
typedef void (*CPR_AssignHousing_t)(void* self, void* housingIBP);   // 0x140111ee0
static CPR_AssignHousing_t s_fn_AssignHousing = nullptr;
static bool s_cpr_resolved = false;

static void CPR_Resolve() {
    if (s_cpr_resolved) return;
    s_fn_AssignHousing = reinterpret_cast<CPR_AssignHousing_t>(
        ResolveAddr(Module::Exe, 0x140111ee0));  // CitizenComponent::AssignHousing
    s_cpr_resolved = true;
}

// Cap so we never convert the entire town into builders (retail spreads work by
// priority; here we bound it to the number of pending sites, at least 1).
static int CPR_BuilderCap(uintptr_t townMgr) {
    int pending = *reinterpret_cast<int32_t*>(townMgr + 0xd8);
    if (pending < 1) pending = 0;
    return pending;
}

// ============================================================
// CitizenProfessionRebalance (stands in for 0x1400d43b0 / fn_PathGraph_AssignCitizens)
// ============================================================
extern "C" void CitizenProfessionRebalance(void* /*pathGraphCtx*/) {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    CPR_Resolve();

    uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    uintptr_t reg = *reinterpret_cast<uintptr_t*>(gw + 0xb0);   // profession registry
    uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(gw + 0xf0);
    if (!reg || !townMgr) return;

    uintptr_t laborerMarker = *reinterpret_cast<uintptr_t*>(reg + 0x30);  // Laborer profession
    uintptr_t builderMarker = *reinterpret_cast<uintptr_t*>(reg + 0x38);  // Builder profession
    if (!laborerMarker || !builderMarker) return;

    // Only act while there is construction to staff (the +0xd0 build registry).
    int needed = CPR_BuilderCap(townMgr);
    if (needed <= 0) return;

    uintptr_t citArray = *reinterpret_cast<uintptr_t*>(townMgr + 0xa0);
    int citCount = *reinterpret_cast<int32_t*>(townMgr + 0xa8);
    if (!citArray || citCount <= 0) return;

    // Count builders already assigned so we don't over-provision each tick.
    int alreadyBuilders = 0;
    for (int i = 0; i < citCount; ++i) {
        uintptr_t citizen = *reinterpret_cast<uintptr_t*>(citArray + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&citizen);
        if (!citizen) continue;
        if (*reinterpret_cast<uintptr_t*>(citizen + 0x1f0) == builderMarker) ++alreadyBuilders;
    }
    if (alreadyBuilders >= needed) return;

    int toAssign = needed - alreadyBuilders;
    int assigned = 0;

    for (int i = 0; i < citCount && assigned < toAssign; ++i) {
        uintptr_t citizen = *reinterpret_cast<uintptr_t*>(citArray + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&citizen);
        if (!citizen) continue;

        // health: *(citizen+0x288)->+0x44 <= *(citizen+0x3c)
        uintptr_t healthComp = *reinterpret_cast<uintptr_t*>(citizen + 0x288);
        if (!healthComp) continue;
        if (*reinterpret_cast<float*>(healthComp + 0x44) > *reinterpret_cast<float*>(citizen + 0x3c))
            continue;
        // not busy
        if (*reinterpret_cast<uintptr_t*>(citizen + 0x208) != 0) continue;
        // currently a Laborer (only promote the idle-default profession)
        if (*reinterpret_cast<uintptr_t*>(citizen + 0x1f0) != laborerMarker) continue;

        // Promote to Builder (faithful: citizen+0x1f0 = required profession, + AssignHousing).
        *reinterpret_cast<uintptr_t*>(citizen + 0x1f0) = builderMarker;
        if (s_fn_AssignHousing) {
            uintptr_t housing = *reinterpret_cast<uintptr_t*>(citizen + 0x248);  // keep current home
            s_fn_AssignHousing(reinterpret_cast<void*>(citizen), &housing);
        }
        ++assigned;
        if (CPR_Trace())
            std::fprintf(stderr,
                "[BuildTrace] Rebalance: citizen=%p -> Builder profession (marker=%p, %d/%d)\n",
                reinterpret_cast<void*>(citizen), reinterpret_cast<void*>(builderMarker),
                alreadyBuilders + assigned, needed);
    }

    if (assigned > 0 && CPR_Trace())
        std::fprintf(stderr,
            "[BuildTrace] Rebalance: assigned %d idle Laborer(s) to Builder (pending sites=%d)\n",
            assigned, needed);
}

// ============================================================
// Standalone registration
// ============================================================
#ifdef STANDALONE_EXE
#include "address_registry.h"
extern "C" void RegisterCitizenProfessionRebalance() {
    // fn_PathGraph_AssignCitizens (called from CitizenBehavior_ProcessPaths tail).
    RegisterExeAddr(0x1400d43b0, reinterpret_cast<uintptr_t>(&CitizenProfessionRebalance));
}
#endif // STANDALONE_EXE
