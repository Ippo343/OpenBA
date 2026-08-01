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
#include "Component/component_factory_base.h"   // ComponentFactory::LiveEnabled (default-on gates)
#include "Object/factory_registration.h"        // ::Object::GetTypeIdByName (capacity probe)
#include <cstdint>
#include <cstdio>
#include <cstdlib>


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

// Default-ON via kDefaultOnFlags (graduated 2026-07-22); env var is a kill-switch (=0).
bool BuilderNaturalOn() {
    return ::ComponentFactory::LiveEnabled("BANISHED_BUILDER_NATURAL");
}

// The auto-increment pool (this file's tick auto-seating construction sites up to capacity)
// was a bring-up test to prove organic construction end-to-end. It is now OFF by default:
// builder assignment is player-driven through the town-hall Professions spinner
// (ProfessionUI_HandleSpinner 0x1400763c0 -> FindAvailableBuilding + the cbf0/ca20/c5f0/c4b0
// seat cascade, all registered in SA), which the always-topping-up pool would otherwise fight.
// Site registration into townMgr+0x100 still happens under BANISHED_BUILDER_NATURAL
// (buildcomponent_state1.cpp) so the spinner's cascade can find the sites. Set
// BANISHED_BUILDER_AUTOPOOL=1 to restore the old auto-seating behaviour for testing.
int s_autopool = -1;
bool AutoPoolOn() {
    if (s_autopool < 0) {
        const char* e = std::getenv("BANISHED_BUILDER_AUTOPOOL");
        s_autopool = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s_autopool == 1;
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

// --- Headless Builder-capacity probe (BANISHED_BUILD_TRACE, diagnostic-only) ------
// Computes what the town-hall Professions panel (ProfessionUI_Slot8, 0x1400765a0) would
// show as the Builder row's "M" (available worker slots) WITHOUT needing the dialog open.
// Slot8's per-profession capacity walk and FindAvailableBuilding use the SAME component
// path — WorkPlaceComponent (c80e0, +0x50 accepting / +0x10 owner) -> the owner's
// WorkComponent (c8070, capacity +0x30 / CalcMaxWorkers 0x14012e5c0). The globals Slot8
// reads (g_TypeId_WorkplaceComponent / g_TypeId_WorkComponent) are bound to
// the type NAMES "WorkPlaceComponent" / "WorkComponent" (see townhallui.cpp 2509-2510),
// so this walk is byte-faithful to what the UI sums. Confirms the site's capacity is
// visible to the UI now that BUILDER_NATURAL registers it into townMgr+0x100 by default.
typedef uint32_t (*ProbeGetTypeId_t)(void*);
static void* ProbeFindCompByTid(uintptr_t entity, uint32_t tid) {
    if (!entity || !tid) return nullptr;
    int count = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    if (!arr) return nullptr;
    for (int i = 0; i < count; ++i) {
        void* slot = reinterpret_cast<void*>(arr + static_cast<int64_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* comp = *reinterpret_cast<void**>(slot);
        if (!comp) continue;
        uint32_t t = ((ProbeGetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
        if (t == tid)
            return (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, tid)) ? comp : nullptr;
    }
    return nullptr;
}

typedef int (*ProbeCalcMaxWorkers_t)(void* workComp);
static void ProbeBuilderCapacity(uintptr_t gw) {
    if (!Trace()) return;
    static int probeThrottle = 0;
    if (++probeThrottle < 120) return;   // ~every 120 ticks — cheap, but not per-frame
    probeThrottle = 0;

    uintptr_t profReg = *reinterpret_cast<uintptr_t*>(gw + 0xb0);
    uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(gw + 0xf0);
    if (!profReg || !townMgr) return;
    uint64_t builderKey = *reinterpret_cast<uint64_t*>(profReg + 0x38);
    if (!builderKey) return;

    static uint32_t tidWpc = 0, tidWc = 0;
    if (!tidWpc) tidWpc = ::Object::GetTypeIdByName("WorkPlaceComponent");
    if (!tidWc)  tidWc  = ::Object::GetTypeIdByName("WorkComponent");
    static ProbeCalcMaxWorkers_t calcMax = nullptr;
    static bool calcRes = false;
    if (!calcRes) { calcMax = reinterpret_cast<ProbeCalcMaxWorkers_t>(ResolveAddr(Module::Exe, 0x14012e5c0)); calcRes = true; }

    // Walk townMgr+0x100 profession BST for the Builder key (same walk as FAB/Slot8).
    uint64_t* node = *reinterpret_cast<uint64_t**>(townMgr + 0x100);
    uint64_t* found = nullptr;
    while (node) {
        if (builderKey < node[0])       node = reinterpret_cast<uint64_t*>(node[4]);
        else if (builderKey <= node[0]) { found = node; break; }
        else                            node = reinterpret_cast<uint64_t*>(node[5]);
    }
    if (!found) {
        std::fprintf(stderr, "[BuildTrace][CapProbe] Builder key=0x%llx NOT in townMgr+0x100 -> UI 'of 0'\n",
                     (unsigned long long)builderKey);
        return;
    }

    int count = static_cast<int>(found[2]);
    uintptr_t list = static_cast<uintptr_t>(found[1]);
    int totalCap = 0, enabled = 0;
    for (int i = 0; i < count; ++i) {
        void* entity = *reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entity);
        if (!entity) continue;
        void* wpc = ProbeFindCompByTid(reinterpret_cast<uintptr_t>(entity), tidWpc);
        if (!wpc) continue;
        if (*reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(wpc) + 0x50) == 0) continue;  // Slot8 skips !accepting
        ++enabled;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(wpc) + 0x10));
        uintptr_t parent = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(wpc) + 0x10);
        if (!parent) continue;
        void* wc = ProbeFindCompByTid(parent, tidWc);
        if (!wc) continue;
        int cap = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(wc) + 0x30);
        if (cap == 0 && calcMax) cap = calcMax(wc);   // lazily, but DON'T persist in a probe
        totalCap += cap;
    }
    std::fprintf(stderr, "[BuildTrace][CapProbe] Builder key=0x%llx registeredSites=%d accepting=%d totalCapacity=%d -> UI would show 'X of %d'\n",
                 (unsigned long long)builderKey, count, enabled, totalCap, totalCap);
}

// --- Whole-registry employment probe (BANISHED_EMPLOY_PROBE, diagnostic-only) -----
// ProbeBuilderCapacity above only looks at the Builder key, which cannot answer the
// question that actually decides whether the Professions panel is usable: does a
// FINISHED NON-CONSTRUCTION workplace (pasture, woodcutter, gatherer...) register
// itself into townMgr+0x100 under its own profession key?
//
// This walks EVERY profession in the registry (reg+0x20 array / +0x28 count), looks
// each key up in the BST, and reports per-building roster/capacity/accepting. A
// profession line with sites=0 means no building of that trade ever registered — the
// panel would show "of 0" for it and no seat could ever land there.
//
// Separate env var from BANISHED_BUILD_TRACE on purpose: BUILD_TRACE floods ~60k
// lines, which is exactly the wrong shape for reading one table.
static void ProbeAllProfessions(uintptr_t gw) {
    static int s_on = -1;
    if (s_on < 0) {
        const char* e = std::getenv("BANISHED_EMPLOY_PROBE");
        s_on = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (s_on != 1) return;

    // Period in ticks; default 600 (~10s). A repeating dump shows registration
    // ARRIVING as buildings finish, which a one-shot would miss entirely.
    static int s_period = 0;
    if (!s_period) {
        const char* e = std::getenv("BANISHED_EMPLOY_PROBE");
        int v = e ? std::atoi(e) : 0;
        s_period = (v > 1) ? v : 600;
    }
    static int s_tick = 0;
    if (++s_tick < s_period) return;
    s_tick = 0;

    uintptr_t reg     = *reinterpret_cast<uintptr_t*>(gw + 0xb0);
    uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(gw + 0xf0);
    if (!reg || !townMgr) return;

    static uint32_t tidWpc = 0, tidWc = 0;
    if (!tidWpc) tidWpc = ::Object::GetTypeIdByName("WorkPlaceComponent");
    if (!tidWc)  tidWc  = ::Object::GetTypeIdByName("WorkComponent");
    static ProbeCalcMaxWorkers_t calcMax = nullptr;
    static bool calcRes = false;
    if (!calcRes) { calcMax = reinterpret_cast<ProbeCalcMaxWorkers_t>(ResolveAddr(Module::Exe, 0x14012e5c0)); calcRes = true; }

    uintptr_t profArray = *reinterpret_cast<uintptr_t*>(reg + 0x20);
    int       profCount = *reinterpret_cast<int32_t*>(reg + 0x28);
    uint64_t  laborerKey = *reinterpret_cast<uint64_t*>(reg + 0x30);

    uint64_t builderKey = *reinterpret_cast<uint64_t*>(reg + 0x38);
    std::fprintf(stderr, "[EmployProbe] ===== townMgr+0x100 registry: %d professions "
                 "(laborerKey=0x%llx builderKey=0x%llx, root=%p) =====\n", profCount,
                 (unsigned long long)laborerKey, (unsigned long long)builderKey,
                 *reinterpret_cast<void**>(townMgr + 0x100));

    int professionsWithSites = 0;
    for (int p = 0; p < profCount; ++p) {
        uint64_t key = *reinterpret_cast<uint64_t*>(profArray + static_cast<int64_t>(p) * 8);

        uint64_t* node = *reinterpret_cast<uint64_t**>(townMgr + 0x100);
        uint64_t* found = nullptr;
        while (node) {
            if (key < node[0])       node = reinterpret_cast<uint64_t*>(node[4]);
            else if (key <= node[0]) { found = node; break; }
            else                     node = reinterpret_cast<uint64_t*>(node[5]);
        }
        if (!found) {
            std::fprintf(stderr, "[EmployProbe]   prof[%2d] key=0x%llx  sites=0  <-- NOT REGISTERED"
                         "%s\n", p, (unsigned long long)key,
                         (key == laborerKey) ? "  (laborer/_default — correct, never registers)" : "");
            continue;
        }
        int count = static_cast<int>(found[2]);
        // Count professions that actually HAVE a site, not merely a BST node. A node is
        // created by the first registration under a key and is NEVER pruned — neither by
        // us nor by retail; f670's de-register calls DynArray8_EraseByValue on the node's
        // array (workplacecomponent.cpp) and leaves the empty node behind. Incrementing on
        // node existence therefore made a correctly-drained profession keep counting
        // forever, which is what produced the phantom "2/20 professions have >=1
        // registered site" after a completed build and sent an investigation looking for a
        // registry leak that did not exist (2026-07-30).
        if (count > 0) ++professionsWithSites;

        uintptr_t list = static_cast<uintptr_t>(found[1]);
        int totalCap = 0, totalRoster = 0, accepting = 0;
        for (int i = 0; i < count; ++i) {
            void* entity = *reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entity);
            if (!entity) continue;
            void* wpc = ProbeFindCompByTid(reinterpret_cast<uintptr_t>(entity), tidWpc);
            if (!wpc) continue;
            uintptr_t w = reinterpret_cast<uintptr_t>(wpc);
            int roster = *reinterpret_cast<int32_t*>(w + 0x60);
            totalRoster += roster;
            if (*reinterpret_cast<uint8_t*>(w + 0x50) != 0) ++accepting;
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(w + 0x10));
            uintptr_t parent = *reinterpret_cast<uintptr_t*>(w + 0x10);
            if (!parent) continue;
            void* wc = ProbeFindCompByTid(parent, tidWc);
            if (!wc) continue;
            int cap = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(wc) + 0x30);
            if (cap == 0 && calcMax) cap = calcMax(wc);   // probe only — do NOT persist
            totalCap += cap;
        }
        std::fprintf(stderr, "[EmployProbe]   prof[%2d] key=0x%llx%s  sites=%d accepting=%d "
                     "roster=%d capacity=%d  -> panel shows '%d of %d'%s\n",
                     p, (unsigned long long)key,
                     (key == builderKey) ? " (BUILDER)" : "",
                     count, accepting, totalRoster, totalCap, totalRoster, totalCap,
                     (totalCap == 0) ? "   <-- 'of 0': no seat can land here" : "");
    }
    std::fprintf(stderr, "[EmployProbe] ===== %d/%d professions have >=1 registered site =====\n",
                 professionsWithSites, profCount);
    std::fflush(stderr);
}

}  // namespace

// Called from CitizenBehavior_Update (the town tick, 0x14008c2b0). Lightly throttled.
extern "C" void BuilderPool_Tick() {
    // Diagnostic one-shot (BANISHED_EMPLOY_PROBE): distinguishes "tick never called"
    // from "BuilderNaturalOn() false" from "probe logic wrong" when the registry dump
    // below goes missing. Prints once, before any gate.
    if (std::getenv("BANISHED_EMPLOY_PROBE")) {
        static bool s_said = false;
        if (!s_said) {
            s_said = true;
            std::fprintf(stderr, "[EmployProbe] BuilderPool_Tick REACHED (first call). "
                         "BuilderNaturalOn=%d gw=%p\n", (int)BuilderNaturalOn(),
                         g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr);
            std::fflush(stderr);
        }
    }
    if (!BuilderNaturalOn()) return;                    // registration gate (default-on)
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    ProbeBuilderCapacity(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton));  // diagnostic (trace-gated)
    ProbeAllProfessions(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton));   // diagnostic (probe-gated)
    if (!AutoPoolOn()) return;                          // auto-increment off by default (spinner-driven now)

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

