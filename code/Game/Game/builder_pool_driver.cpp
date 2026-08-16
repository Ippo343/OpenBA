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
// RE-VERIFIED 2026-08-09 from the decompile of 0x1400d43b0: the skip is RETAIL's, not ours.
// Phase 1 (the citizen sweep) only *evicts* — for a citizen whose site profession
// (WorkPlaceComponent+0x48) no longer matches its own (+0x1f0) it calls AssignHousing(null)
// and demotes it to laborer; it never seats. Phase 2 then walks the per-profession site
// lists out of townMgr+0x100 and builds its need table under a literal
// `if (*(int *)(workPlaceComponent + 0x60) != 0)` guard, so a roster-0 site is invisible to
// it. Nothing else in retail seats a first worker: the WorkPlaceUI workerCountUp click
// (0x1450 -> FUN_14008d100 + CitizenComponent_impl_c3d0) and the town-hall Professions
// spinner are the only first-seat paths, and both are ported + live in SA. So this stand-in
// exists because retail's UI is the allocator's missing half, not because our allocator
// diverges — do NOT "fix" 0x1400d43b0 by removing the roster==0 skip.
//
// WHAT THIS FILE STILL DOES: registration-side diagnostics only. It reports what the
// town-hall Professions panel would show for the Builder row (ProbeBuilderCapacity) and
// for every profession (ProbeAllProfessions), which is how "the site is registered but
// shows 'of 0'" gets told apart from "the site never registered".
//
// ── RETIRED 2026-08-15: BANISHED_BUILDER_AUTOPOOL ────────────────────────────────────
// This file used to carry a tick-driven auto-seat pool: each town tick it ran the retail
// seat cascade (FindAvailableBuilding + cbf0/ca20/c5f0/c4b0) DIRECTLY against every
// registered construction site, reaching around the UI. It existed because the widget
// that normally drives that cascade — the workplace panel's workerCountUp click — was
// unproven, and it was the documented "set this or nothing gets built" flag.
//
// The click is now measured on a construction site specifically: with AUTOPOOL off and
// only standalone/panel_staffing_driver.cpp running, a site placed by
// BANISHED_TOOL_HARNESS=placement went roster 0 -> 1 -> 3 (its full WorkComponent
// capacity) through WorkPlaceUI slot 9. So the bypass has no remaining job and is gone:
// builders are seated the same way every other profession is, by the panel driver
// standing in for the player on unattended runs.
//
// Site REGISTRATION into townMgr+0x100 (buildcomponent_state1.cpp, gated on
// BANISHED_BUILDER_NATURAL, default-on) is unchanged and still required — it is what
// makes a site visible to the finders behind the click.

#include "resolve.h"
#include "engine_funcs.h"
#include "Component/component_factory_base.h"   // ComponentFactory::LiveEnabled (default-on gates)
#include "Object/factory_registration.h"        // ::Object::GetTypeIdByName (capacity probe)
#include <cstdint>
#include <cstdio>
#include <cstdlib>


namespace {

// Default-ON via kDefaultOnFlags (graduated 2026-07-22); env var is a kill-switch (=0).
bool BuilderNaturalOn() {
    return ::ComponentFactory::LiveEnabled("BANISHED_BUILDER_NATURAL");
}

int s_trace = -1;
bool Trace() {
    if (s_trace < 0) {
        const char* e = std::getenv("BANISHED_BUILD_TRACE");
        s_trace = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s_trace == 1;
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
    // Seating itself is no longer done here — see the RETIRED note at the top of the
    // file. standalone/panel_staffing_driver.cpp clicks workerCountUp on Builder-keyed
    // construction sites exactly as it does on every other workplace.
}

