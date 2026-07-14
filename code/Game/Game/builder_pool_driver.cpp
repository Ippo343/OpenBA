// code/Game/Game/builder_pool_driver.cpp
//
// Tick-driven Builder pool — the headless stand-in for the player-set Builder count.
//
// In this game construction is gated on a Builder count that the player sets via the
// town-hall Professions spinner (UI msg 0x15 -> CitizenComponent_impl_63c0 / dd10 ->
// FindAvailableBuilding + the cbf0/ca20/c5f0/c4b0 laborer-conversion cascade). There is
// NO player-free path in the engine that seats the FIRST builder at a fresh construction
// site: the periodic employment allocator (0x1400d43b0) only REBALANCES workplaces that
// already have >=1 worker (its Phase-2 need-table skips any workplace whose live roster
// count WPC+0x60 == 0 — Ghidra-verified). See reference_builder_profession_bst_flow.
//
// So "fully organic" is driven here: each town tick we keep the town's construction sites
// staffed by running the exact retail seat cascade against the sites FindAvailableBuilding
// returns (the sites State1Enter registered into townMgr+0x100 under the Builder key), up
// to each site's WorkComponent capacity, nearest-first. It is self-limiting: once every
// registered site's roster is full FindAvailableBuilding returns null and we stop; once no
// idle Laborer remains the cascade returns null and we stop. This mirrors what the player's
// Builder count would do, minus the UI.
//
// FREEZE SAFETY: seating calls AssignHousing (0x140111ee0) -> the WPC attach/detach helpers
// (fa20/f7e0) whose Graphics::Scene mesh-visual toggle deadlocks under per-tick churn. We
// hold g_wpcRosterOnly across the seat loop so those helpers do the bare roster-list mutation
// only (same guard the allocator body and the legacy direct-seat use).
//
// Gated behind BANISHED_BUILDER_NATURAL (opt-in); the whole active body is standalone-only.

#include "resolve.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#ifdef STANDALONE_EXE

// FindAvailableBuilding (0x14008d100) — town_workplace_registry.cpp. Walks townMgr+0x100[key]
// and writes an available workplace ENTITY into *out (empty IBP if the key is absent or every
// registered site is full: WPC+0x60 >= WorkComponent+0x30). Capacity is computed lazily here.
extern "C" void* FindAvailableBuilding(int64_t townMgr, void* out, uint64_t professionKey);

// The four retail seat strategies (citizen_employ_cascade.cpp) — each seats one idle Laborer
// at the given available workplace: +0x1f0 = Builder key, +0x248 = site (via AssignHousing).
extern "C" void* CitizenEmploy_Strat_cbf0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey);
extern "C" void* CitizenEmploy_Strat_ca20(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey);
extern "C" void* CitizenEmploy_Strat_c5f0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey);
extern "C" void* CitizenEmploy_Strat_c4b0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey);

// Roster-only freeze guard (workplacecomponent.cpp).
extern bool g_wpcRosterOnly;

namespace {

int s_natural = -1;
bool BuilderNaturalOn() {
    if (s_natural < 0) {
        const char* e = std::getenv("BANISHED_BUILDER_NATURAL");
        s_natural = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s_natural == 1;
}

int s_trace = -1;
bool Trace() {
    if (s_trace < 0) {
        const char* e = std::getenv("BANISHED_BUILD_TRACE");
        s_trace = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s_trace == 1;
}

// Per-tick iteration bound (safety cap; the real cap is total registered site capacity).
int s_capPerTick = -1;
int CapPerTick() {
    if (s_capPerTick < 0) {
        const char* e = std::getenv("BANISHED_BUILDER_COUNT");
        s_capPerTick = (e && e[0]) ? std::atoi(e) : 32;
        if (s_capPerTick < 1) s_capPerTick = 32;
    }
    return s_capPerTick;
}

// CitizenComponent::AssignWorkplace (0x1401163d0) — used to clear a seated Laborer's stale
// +0x238 (old residence-workplace link) so the CitizenAI candidate brain stops offering it
// GatherResources("restock your workplace") forever and lets it fall through to build. Same
// post-seat fix the legacy direct-seat (CitizenEmploy_SeatWorkersAt) applies.
typedef void (*fn_AssignWorkplace_t)(void* self, void* workplaceIBP, int slotIndex);
fn_AssignWorkplace_t s_assignWp = nullptr;
bool s_resolved = false;
void EnsureResolved() {
    if (s_resolved) return;
    s_assignWp = reinterpret_cast<fn_AssignWorkplace_t>(ResolveAddr(Module::Exe, 0x1401163d0));
    s_resolved = true;
}

// Run the cbf0 -> ca20 -> c5f0 -> c4b0 cascade for one seat at `avail`. Returns the seated
// citizen (0 if none). Mirrors CitizenEmploy_SeatWorkers' per-iteration fallback order.
int64_t SeatOneAt(int64_t townMgr, int64_t avail, int64_t builderKey) {
    int64_t result[2];
    int64_t availArg;

    result[0] = 0; availArg = avail;
    CitizenEmploy_Strat_cbf0(townMgr, result, &availArg, builderKey);
    if (result[0]) return result[0];
    result[0] = 0; availArg = avail;
    CitizenEmploy_Strat_ca20(townMgr, result, &availArg, builderKey);
    if (result[0]) return result[0];
    result[0] = 0; availArg = avail;
    CitizenEmploy_Strat_c5f0(townMgr, result, &availArg, builderKey);
    if (result[0]) return result[0];
    result[0] = 0; availArg = avail;
    CitizenEmploy_Strat_c4b0(townMgr, result, &availArg, builderKey);
    return result[0];
}

}  // namespace

// Called from CitizenBehavior_Update (the town tick, 0x14008c2b0). Lightly throttled.
extern "C" void BuilderPool_Tick() {
    if (!BuilderNaturalOn()) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    // Throttle: seating is cheap once sites are full (FindAvailableBuilding returns null fast),
    // but the BST walk + component scans need not run at full frame rate. ~every 20 ticks.
    static int s_throttle = 0;
    if (++s_throttle < 20) return;
    s_throttle = 0;

    EnsureResolved();

    uintptr_t gw      = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    uintptr_t profReg = *reinterpret_cast<uintptr_t*>(gw + 0xb0);
    if (!profReg) return;
    uint64_t builderKey = *reinterpret_cast<uint64_t*>(profReg + 0x38);
    int64_t  townMgr    = *reinterpret_cast<int64_t*>(gw + 0xf0);
    if (!builderKey || !townMgr) return;

    const int cap = CapPerTick();
    bool prevRoster = g_wpcRosterOnly;
    g_wpcRosterOnly = true;

    int seated = 0;
    for (int i = 0; i < cap; ++i) {
        int64_t availOut[2] = { 0, 0 };
        FindAvailableBuilding(townMgr, availOut, builderKey);
        int64_t avail = availOut[0];
        if (avail == 0) break;                    // every registered site is full -> stop

        int64_t cz = SeatOneAt(townMgr, avail, static_cast<int64_t>(builderKey));
        if (cz == 0) break;                        // no idle Laborer left -> stop

        // Clear the freshly-seated Laborer's stale +0x238 (see s_assignWp note) so the
        // builder isn't diverted into GatherResources and instead does the build work.
        int64_t prevWp = *reinterpret_cast<int64_t*>(cz + 0x238);
        if (prevWp != 0 && s_assignWp) {
            int64_t nullIBP = 0;
            s_assignWp(reinterpret_cast<void*>(cz), &nullIBP, -1);
        }
        ++seated;
    }

    g_wpcRosterOnly = prevRoster;

    if (seated && Trace())
        std::fprintf(stderr, "[BuildTrace] BuilderPool_Tick: seated %d Builder(s) this pass (builderKey=0x%llx)\n",
                     seated, static_cast<unsigned long long>(builderKey));
}

#else   // !STANDALONE_EXE — no-op in DLL mode (retail drives its own builder seating).

extern "C" void BuilderPool_Tick() {}

#endif  // STANDALONE_EXE
