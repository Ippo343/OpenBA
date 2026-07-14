// code/Game/Game/construction_force_harness.cpp
// Organic-construction test driver (env BANISHED_FORCE_CONSTRUCT, default OFF).
//
// Built on the generic ForceWork core (force_work.h), the same way forester_force_harness.cpp
// drives ForesterWork. Instead of the town's auto-assignment (empty registry / freezes at the
// pathfinder frontier), we point ONE citizen at the demo construction foundation and drive its
// construction Work directly, mirroring what the site's inert controller would do organically:
//
//   PHASE GATHER (BuildComponent state 1): construct a GatherRawMaterials bound to the site
//     (GatherRawMaterials_ConstructForSite → the faithful site ctor seeds per-material slots
//     from the site's BuildDescription + already-in-storage counts) and drive its Execute when
//     the citizen's action stack is idle. Execute finds a town StorageComponent with the needed
//     material, reserves it, pathfinds, and pushes a pickup/carry/drop action chain onto the
//     citizen's stack; MoveTick's executor runs it. As materials physically arrive in the site
//     storage the state-1 update (already ported) transitions the BuildComponent 1 → 8.
//
//   PHASE BUILD (state 8): TODO (task 5) — drive BuildStructure::Execute so build labor raises
//     BuildComp+0x48 and the site completes 8 → 2.
//
// DELIVERED-SYNC: GatherRawMaterials credits delivered (field_48) from its OnComplete, which
// fires from the citizen's DropOff→CompleteTask action — a node the standalone CitizenAI
// executor doesn't yet reach. So we force BANISHED_BUILD_SYNC_DELIVERED on (gatherrawmaterials.cpp
// re-syncs field_48 from the live site storage each Execute), which lets the gather advance
// through materials as they are physically delivered without the deep executor completion.
//
// CRASH SAFETY: single observable subject; refuse to drive until deps resolve; all foreign
// reads go through ForceWork's VirtualQuery-backed probes.

#ifdef STANDALONE_EXE

#include "engine_funcs.h"
#include "resolve.h"
#include "force_work.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// The demo foundation's OWNER entity (starttown.cpp, set alongside g_pConstructionDemoSite).
extern "C" void* g_pConstructionDemoEntity;

// Construction Work wrappers (gatherrawmaterials.cpp).
extern "C" void* GatherRawMaterials_ConstructForSite(void* buildEntityIBP);
extern "C" int   GatherRawMaterials_ForceDrive(void* self, void* citizenIBP);
extern "C" bool  GatherRawMaterials_DepsReady();

static bool ForceConstructEnvOn() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_FORCE_CONSTRUCT");
        s = (e && e[0] && e[0] != '0') ? 1 : 0;
        // Bridge the DropOff→CompleteTask executor gap so delivered credits from the live
        // site storage each Execute (see file header). Set before GatherRawMaterials::Execute
        // caches its own getenv, so the harness is self-contained.
        if (s == 1) _putenv_s("BANISHED_BUILD_SYNC_DELIVERED", "1");
    }
    return s == 1;
}

// Driver. Called from the CitizenAI UpdateAdapter for ONE force-worked citizen (same hook as
// StartTown_ForcePushForesterWork). Returns true if it "owns" the citizen this tick (caller
// skips the competing AI state machine so the construction work keeps the idle stack).
extern "C" bool StartTown_ForcePushConstructionWork(void* citizen) {
    if (!ForceConstructEnvOn()) return false;
    void* site = g_pConstructionDemoEntity;
    if (!site || !citizen) return false;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return false;
    if (!fn_InternalBasePtr_Ctor || !fn_MemAlloc || !g_pMemAllocSingleton) return false;

    // Bind to the FIRST force-worked citizen only (single, observable subject).
    static void* s_owner = nullptr;
    if (!s_owner) s_owner = citizen;
    if (citizen != s_owner) return false;

    // One-time readiness gate: refuse to drive until every GatherRawMaterials::Execute
    // dependency resolves, else Execute would null-deref a sub-fn / type-id.
    static int s_verified = -1;   // -1 unchecked, 0 unusable, 1 usable
    if (s_verified < 0) {
        bool ok = GatherRawMaterials_DepsReady();
        s_verified = ok ? 1 : 0;
        fprintf(stderr, "[ForceConstruct] site=%p depsReady=%d -> %s\n", site, (int)ok,
                s_verified ? "will drive" : "report-only (deps missing)");
        fflush(stderr);
    }
    if (s_verified != 1) return false;

    // Construct the site GatherRawMaterials ONCE and keep it (its per-material slot arrays
    // @+0x48/+0x58 must survive across drives). Faithful site ctor seeds the slots.
    static void* s_gather = nullptr;
    if (!s_gather) {
        int64_t siteIBP = 0;
        fn_InternalBasePtr_Ctor(&siteIBP, site);
        s_gather = GatherRawMaterials_ConstructForSite(&siteIBP);
        if (!s_gather) return true;   // own it, retry construction next tick
        fprintf(stderr, "[ForceConstruct] constructed GatherRawMaterials=%p for site=%p\n",
                s_gather, site);
        fflush(stderr);
    }

    // Drive the gather Execute when the citizen's action stack is idle (mirrors the site
    // controller's Work-vtable call with the worker IBP). Own the citizen every tick.
    static int s_driveCount = 0;
    ForceWork::DriveWhenIdle(citizen, s_gather, &GatherRawMaterials_ForceDrive,
                             "ForceConstruct", &s_driveCount, 12);
    return true;
}

#endif  // STANDALONE_EXE
