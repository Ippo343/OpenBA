// standalone/orchard_workers_harness.cpp
// BANISHED_ORCHARD_WORKERS=<n> — orchard STAFFING stand-in: the one player action the
// chain still lacks. Direct analogue of BANISHED_PASTURE_HERDERS (standalone/pasture_harness.cpp),
// pointed at OrchardComponent instead of PastureComponent.
//
// WHY THIS IS NEEDED AND NOT A CHEAT: the periodic employment allocator (0x1400d43b0,
// citizen_assign_citizens.cpp) gates on WPC+0x60 != 0 — the LIVE ROSTER COUNT — so it
// rebalances and refills already-staffed workplaces and structurally cannot seat worker #1.
// In retail that first seat comes from a player spinner click (TownHallUI::OnProfessionSpinner
// 0x14007dd10 / ProfessionUI 0x1400763c0). This harness fakes exactly that one click by
// driving the real seat cascade (CitizenEmploy_SeatWorkers = FindAvailableBuilding +
// cbf0->ca20->c5f0->c4b0, the same calls dd10 makes per worker) — every step after the seat
// is organic: the orchardist's CitizenAI claims the PlantOrchard the state machine posted,
// works it, and the slots advance.
//
// Env:
//   BANISHED_ORCHARD_WORKERS=<n>   seat n worker(s) per orchard; unset/0 = disabled
//
// Cadence: from frame kStartFrame, re-checked every kRetry frames, at most once per orchard.
// One-shot is wrong here for the same reason it was wrong for pastures — an orchard is only
// staffable AFTER its workplace has been switched on (state 9 enter calls SetWorkplaceActive),
// and in manual play the player types it minutes in. Re-checking costs nothing and each
// orchard is remembered once seated.
//
// Offsets, all already used by neighbouring harness/diag code:
//   gameWorld+0xe8 EntityList, +0x08/+0x10 {data,count}; entity+0x30/+0x38 component array
//   gameWorld+0xf0 town manager (the townMgr CitizenEmploy_SeatWorkers takes)
//   WorkPlaceComponent+0x50 accepting/on, +0x18 description, desc+0x20 profession key,
//   +0x60 live roster count
//
// Do NOT touch code/Game/Component/orchardcomponent*.cpp from here — this file only pokes
// already-published entry points.

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// The seat cascade (code/Game/Component/citizen_employ.cpp) — the headless equivalent of
// the per-worker inner loop of CitizenComponent_impl_dd10.
extern "C" int CitizenEmploy_SeatWorkers(int64_t townMgr, uint64_t professionKey, int count);

namespace {

constexpr int kStartFrame = 400;    // well past state 9 enter (which runs ~frame 180+)
constexpr int kRetry      = 120;
constexpr int kMaxTracked = 16;

bool s_initialized = false;
int  s_count       = 0;             // 0 = disabled
void* s_seated[kMaxTracked];
int   s_seatedCount = 0;

void Init() {
    if (s_initialized) return;
    s_initialized = true;

    const char* env = std::getenv("BANISHED_ORCHARD_WORKERS");
    if (!env || env[0] == '\0' ||
        std::strcmp(env, "0") == 0 || std::strcmp(env, "false") == 0) {
        s_count = 0;
        fprintf(stderr, "[OrchardWorkers] disabled (BANISHED_ORCHARD_WORKERS unset)\n");
        return;
    }
    s_count = std::atoi(env);
    if (s_count <= 0) s_count = 1;
    fprintf(stderr, "[OrchardWorkers] ENABLED (BANISHED_ORCHARD_WORKERS=\"%s\") — seat %d "
                    "worker(s) per orchard, re-checked every %d frames from frame %d\n",
            env, s_count, kRetry, kStartFrame);
    std::fflush(stderr);
}

bool AlreadySeated(void* comp) {
    for (int i = 0; i < s_seatedCount; ++i)
        if (s_seated[i] == comp) return true;
    return false;
}

void Fire(int frameNum) {
    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) {
        fprintf(stderr, "[OrchardWorkers] frame=%d: gameWorld singleton null — cannot run\n", frameNum);
        return;
    }
    uintptr_t gwB = reinterpret_cast<uintptr_t>(gw);
    uintptr_t townMgr    = *reinterpret_cast<uintptr_t*>(gwB + 0xf0);
    uintptr_t entityList = *reinterpret_cast<uintptr_t*>(gwB + 0xe8);
    if (!townMgr || !entityList) {
        fprintf(stderr, "[OrchardWorkers] frame=%d: townMgr(gw+0xf0)=%p entityList(gw+0xe8)=%p — "
                        "cannot run\n", frameNum, reinterpret_cast<void*>(townMgr),
                reinterpret_cast<void*>(entityList));
        return;
    }
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entityList + 0x08);
    int       n   = *reinterpret_cast<int32_t*>(entityList + 0x10);
    const uint32_t tidOrchardComp = ::Object::GetTypeIdByName("OrchardComponent");
    if (!arr || n <= 0 || !tidOrchardComp) {
        fprintf(stderr, "[OrchardWorkers] frame=%d: cannot scan (arr=%p n=%d "
                        "tidOrchardComponent=%u)\n", frameNum, reinterpret_cast<void*>(arr), n,
                tidOrchardComp);
        return;
    }

    // Shared component finder — the same one the pasture herder stand-in uses to reach a
    // WorkPlaceComponent from an entity (see reference_misnamed_component_finders: 0x140002e10
    // is FindComponent_WorkPlace, whatever Ghidra called it).
    typedef void* (*fn_FindWPC_t)(int64_t entity, void* outIBP);
    uintptr_t f2e10 = ResolveAddr(Module::Exe, 0x140002e10);
    if (!f2e10) {
        fprintf(stderr, "[OrchardWorkers] frame=%d: FindComponent_WorkPlace (0x140002e10) "
                        "unresolved — cannot staff\n", frameNum);
        return;
    }

    int found = 0, considered = 0;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* ent = *slot;
        if (!ent) continue;

        int       cN = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
        uintptr_t cA = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
        void* orchComp = nullptr;
        for (int c = 0; c < cN && cA; ++c) {
            void** cs = reinterpret_cast<void**>(cA + static_cast<uintptr_t>(c) * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(cs);
            void* comp = *cs;
            if (!comp) continue;
            void** vt = *reinterpret_cast<void***>(comp);
            typedef uint32_t (*GetTypeId_t)(void*);
            if (vt && vt[2] && reinterpret_cast<GetTypeId_t>(vt[2])(comp) == tidOrchardComp) {
                orchComp = comp;
                break;
            }
        }
        if (!orchComp) continue;
        ++found;
        if (AlreadySeated(orchComp)) continue;
        ++considered;

        uint64_t wpcIBP = 0;
        reinterpret_cast<fn_FindWPC_t>(f2e10)(reinterpret_cast<int64_t>(ent), &wpcIBP);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpcIBP);
        if (!wpcIBP) {
            fprintf(stderr, "[OrchardWorkers] frame=%d: orchard comp=%p has NO "
                            "WorkPlaceComponent — cannot staff\n", frameNum, orchComp);
            continue;
        }
        const char accepting = *reinterpret_cast<char*>(wpcIBP + 0x50);
        if (accepting == 0) {
            fprintf(stderr, "[OrchardWorkers] frame=%d: orchard comp=%p workplace=%p is OFF "
                            "(+0x50=0) — not staffable yet (state 9 enter turns it on; is the "
                            "orchard typed and started?)\n", frameNum, orchComp,
                    reinterpret_cast<void*>(wpcIBP));
            continue;
        }
        uint64_t desc    = *reinterpret_cast<uint64_t*>(wpcIBP + 0x18);
        uint64_t profKey = desc ? *reinterpret_cast<uint64_t*>(desc + 0x20) : 0;
        if (!profKey) {
            fprintf(stderr, "[OrchardWorkers] frame=%d: orchard comp=%p workplace=%p has no "
                            "profession key (desc=%p) — cannot staff\n", frameNum, orchComp,
                    reinterpret_cast<void*>(wpcIBP), reinterpret_cast<void*>(desc));
            continue;
        }

        const int32_t rosterBefore = *reinterpret_cast<int32_t*>(wpcIBP + 0x60);
        int seated = CitizenEmploy_SeatWorkers(static_cast<int64_t>(townMgr), profKey, s_count);
        const int32_t rosterAfter  = *reinterpret_cast<int32_t*>(wpcIBP + 0x60);

        if (s_seatedCount < kMaxTracked) s_seated[s_seatedCount++] = orchComp;

        // Report the ROSTER, not just the cascade's return: the cascade can report a seat
        // while the worker lands on a different workplace of the same profession, and the
        // roster is the thing the rest of the chain actually reads.
        fprintf(stderr, "[OrchardWorkers] frame=%d: orchard comp=%p workplace=%p profKey=0x%llx "
                        "cascade seated %d/%d — roster(+0x60) %d -> %d%s\n",
                frameNum, orchComp, reinterpret_cast<void*>(wpcIBP),
                static_cast<unsigned long long>(profKey), seated, s_count,
                rosterBefore, rosterAfter,
                rosterAfter > rosterBefore ? "" :
                    "  <-- NO SEAT LANDED HERE (no idle laborer? the panel-staffing driver "
                    "may already have seated them; "
                    "or the seat went to another workplace on the same profession key)");
        std::fflush(stderr);
    }

    if (found == 0) {
        fprintf(stderr, "[OrchardWorkers] frame=%d: scanned %d entities, found ZERO with an "
                        "OrchardComponent (set BANISHED_BUILD_DEMO_ORCHARD=1?)\n", frameNum, n);
        std::fflush(stderr);
    } else if (considered == 0) {
        // Silence here would read as "nothing to do" when it really means "all done".
        static bool s_saidDone = false;
        if (!s_saidDone) {
            s_saidDone = true;
            fprintf(stderr, "[OrchardWorkers] frame=%d: all %d orchard(s) already handled — "
                            "no further staffing attempts\n", frameNum, found);
            std::fflush(stderr);
        }
    }
}

}  // namespace

extern "C" void OrchardWorkersHarness_Tick(int frameNum) {
    Init();
    if (s_count <= 0) return;
    if (frameNum < kStartFrame || ((frameNum - kStartFrame) % kRetry) != 0) return;
    Fire(frameNum);
}
