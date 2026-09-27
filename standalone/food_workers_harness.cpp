// standalone/food_workers_harness.cpp
// BANISHED_FOOD_WORKERS=<n> — staffing stand-in for the food-production buildings that
// have no tool yet: the Fishing Dock, Hunter Lodge, Gatherer's Hut and Crop Field.
// Direct analogue of BANISHED_ORCHARD_WORKERS / BANISHED_PASTURE_HERDERS, generalised
// over the four component types instead of copied a fourth and fifth time.
//
// WHY THIS IS NEEDED AND NOT A CHEAT: the periodic employment allocator (0x1400d43b0,
// citizen_assign_citizens.cpp) gates on WPC+0x60 != 0 — the LIVE ROSTER COUNT — so it
// rebalances and refills already-staffed workplaces and structurally cannot seat worker
// #1. In retail that first seat comes from a player spinner click (TownHallUI /
// ProfessionUI). This fakes exactly that one click by driving the real seat cascade
// (CitizenEmploy_SeatWorkers), and everything after the seat is organic: the worker's
// CitizenAI claims the Work the state machine posted and runs it.
//
// MEASURED 2026-08-11: with the three ResourceProduce buildings live, all three sat in
// state 13 (producing) with accepting=1 and staffed=0 — the state machine had posted the
// work and opened the workplace, and no citizen was ever assigned. Nothing could be
// produced, and the town-storage total only fell (citizens eating).
//
// Cadence: from kStartFrame, re-checked every kRetry frames, at most once per building.
// One-shot is wrong for the same reason it was wrong for orchards and pastures — a
// building is only staffable once its workplace has been switched on, which for the
// ResourceProduce family happens on state-13 enter.
//
// Do NOT touch the component ports from here; this file only pokes published entry points.

#include "resolve.h"
#include "engine_funcs.h"
#include "options.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" int CitizenEmploy_SeatWorkers(int64_t townMgr, uint64_t professionKey, int count);

namespace {

constexpr int kStartFrame  = 400;
constexpr int kRetry       = 120;
constexpr int kMaxTracked  = 32;

const char* const kClasses[] = {
    "FishingComponent",
    "HuntComponent",
    "HerbalistComponent",
    "CropFieldComponent",
    // 2026-08-22: added. MarketComponent shares this family's staffing route exactly —
    // same RPC state table, same code=1 ledger post, same per-profession bucket keyed on
    // WPC+0x48 — but it was missing here, so BANISHED_FOOD_WORKERS never seated a market
    // and every market measured staffed=0. That is why MarketWork's vtable slot 4
    // (Execute) recorded ZERO dispatches: slot 4 is purely claim-driven (retail has no
    // unconditional per-tick call of it anywhere), and an unstaffed workplace is invisible
    // to the citizen claim walk. It was never a dispatch defect.
    "MarketComponent",
};
constexpr int kClassCount = (int)(sizeof(kClasses) / sizeof(kClasses[0]));

bool  s_initialized = false;
int   s_count       = 0;             // 0 = disabled
void* s_seated[kMaxTracked];
int   s_seatedCount = 0;

void Init() {
    if (s_initialized) return;
    s_initialized = true;
    const char* env = Opt().foodWorkers;
    if (!env || env[0] == '\0' ||
        std::strcmp(env, "0") == 0 || std::strcmp(env, "false") == 0) {
        s_count = 0;
        std::fprintf(stderr, "[FoodWorkers] disabled (BANISHED_FOOD_WORKERS unset)\n");
        std::fflush(stderr);
        return;
    }
    s_count = std::atoi(env);
    if (s_count <= 0) s_count = 1;
    std::fprintf(stderr, "[FoodWorkers] ENABLED (BANISHED_FOOD_WORKERS=\"%s\") — seat %d "
                 "worker(s) per food building, re-checked every %d frames from frame %d\n",
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
        std::fprintf(stderr, "[FoodWorkers] frame=%d: gameWorld null — cannot run\n", frameNum);
        return;
    }
    uintptr_t gwB = reinterpret_cast<uintptr_t>(gw);
    uintptr_t townMgr    = *reinterpret_cast<uintptr_t*>(gwB + 0xf0);
    uintptr_t entityList = *reinterpret_cast<uintptr_t*>(gwB + 0xe8);
    if (!townMgr || !entityList) {
        std::fprintf(stderr, "[FoodWorkers] frame=%d: townMgr=%p entityList=%p — cannot run\n",
                     frameNum, (void*)townMgr, (void*)entityList);
        return;
    }
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entityList + 0x08);
    int       n   = *reinterpret_cast<int32_t*>(entityList + 0x10);
    if (!arr || n <= 0) {
        std::fprintf(stderr, "[FoodWorkers] frame=%d: cannot scan (arr=%p n=%d)\n",
                     frameNum, (void*)arr, n);
        return;
    }

    uint32_t tids[kClassCount];
    for (int k = 0; k < kClassCount; ++k) tids[k] = ::Object::GetTypeIdByName(kClasses[k]);

    // 0x140002e10 is FindComponent_WorkPlace whatever Ghidra called it — see
    // reference_misnamed_component_finders.
    typedef void* (*fn_FindWPC_t)(int64_t entity, void* outIBP);
    uintptr_t f2e10 = ResolveAddr(Module::Exe, 0x140002e10);
    if (!f2e10) {
        std::fprintf(stderr, "[FoodWorkers] frame=%d: FindComponent_WorkPlace (0x140002e10) "
                     "unresolved — cannot staff\n", frameNum);
        return;
    }

    int found = 0, considered = 0;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* ent = *slot;
        if (!ent) continue;

        const int cN = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
        uintptr_t  cA = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
        void*       foodComp = nullptr;
        const char* foodName = nullptr;
        for (int c = 0; c < cN && cA && !foodComp; ++c) {
            void** cs = reinterpret_cast<void**>(cA + static_cast<uintptr_t>(c) * 8);
            void* comp = *cs;
            if (!comp) continue;
            const uint32_t t = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
            for (int k = 0; k < kClassCount; ++k)
                if (tids[k] && t == tids[k]) { foodComp = comp; foodName = kClasses[k]; break; }
        }
        if (!foodComp) continue;
        ++found;

        // TEMPORARY INSTRUMENTATION (2026-08-15). The hunting cabin's job-count up-arrow
        // is greyed out, so a laborer can never be assigned to the hunt work class from
        // the UI. WorkplaceUI::Startup (retail 0x140087080) enables it with exactly
        // `workComp+0x30 < workDesc+0x2c`, where workDesc = workComp+0x18. Reading those
        // through the UI needs the panel open, which no soak does -- so sample them
        // straight off the entity here instead, which isolates the DATA question (did the
        // WorkDescription deserialize?) from the UI question.
        //
        // HunterLodge.rsc ships _defaultWorkers=3 _minWorkerCount=1 _maxWorkerCount=4,
        // so retail expects wpMax=3 < max=4 -> ENABLED. Print the ctor defaults' tell too:
        // an undeserialized WorkDescription keeps min=1/max=100.
        {
            static long long s_dumpN = 0;
            typedef void* (*fn_FindWork_t)(int64_t entity, void* outIBP);
            uintptr_t f3050 = ResolveAddr(Module::Exe, 0x140003050);  // FindComponent_Work
            if (f3050 && (s_dumpN++ % 5) == 0) {
                uint64_t wIBP = 0;
                reinterpret_cast<fn_FindWork_t>(f3050)(reinterpret_cast<int64_t>(ent), &wIBP);
                if (!wIBP) {
                    std::fprintf(stderr, "[JobArrowData] frame=%d: %s ent=%p has NO "
                                 "WorkComponent (0x140003050 found nothing) — the panel's "
                                 "jobType would be null\n", frameNum, foodName, ent);
                } else {
                    const uint64_t wd = *reinterpret_cast<uint64_t*>(wIBP + 0x18);
                    const int32_t  cur = *reinterpret_cast<int32_t*>(wIBP + 0x30);
                    if (!wd) {
                        std::fprintf(stderr, "[JobArrowData] frame=%d: %s workComp=%p "
                                     "+0x30=%d but DESCRIPTION (+0x18) IS NULL — jobType "
                                     "null, arrows dead\n",
                                     frameNum, foodName, (void*)wIBP, cur);
                    } else {
                        const int32_t dflt = *reinterpret_cast<int32_t*>(wd + 0x24);
                        const int32_t mn   = *reinterpret_cast<int32_t*>(wd + 0x28);
                        const int32_t mx   = *reinterpret_cast<int32_t*>(wd + 0x2c);
                        const int32_t area = *reinterpret_cast<int32_t*>(wd + 0x30);
                        const uint8_t allow = *reinterpret_cast<uint8_t*>(wd + 0x20);
                        const uint8_t scale = *reinterpret_cast<uint8_t*>(wd + 0x34);
                        std::fprintf(stderr,
                            "[JobArrowData] frame=%d: %s workComp=%p desc=%p | "
                            "wpMax(+0x30)=%d | _allowCountChange(+0x20)=%d "
                            "_defaultWorkers(+0x24)=%d _minWorkerCount(+0x28)=%d "
                            "_maxWorkerCount(+0x2c)=%d _maxArea(+0x30)=%d "
                            "_scaleWithSize(+0x34)=%d -> upEnabled=%d downEnabled=%d%s\n",
                            frameNum, foodName, (void*)wIBP, (void*)wd, cur,
                            (int)allow, dflt, mn, mx, area, (int)scale,
                            cur < mx ? 1 : 0, (cur != mn && cur > mn) ? 1 : 0,
                            (mn == 1 && mx == 100 && dflt == 0)
                                ? "  <-- CTOR DEFAULTS: WorkDescription never deserialized"
                                : "");
                    }
                }
                std::fflush(stderr);
            }
        }

        if (AlreadySeated(foodComp)) continue;
        ++considered;

        uint64_t wpcIBP = 0;
        reinterpret_cast<fn_FindWPC_t>(f2e10)(reinterpret_cast<int64_t>(ent), &wpcIBP);
        if (!wpcIBP) {
            std::fprintf(stderr, "[FoodWorkers] frame=%d: %s comp=%p has NO WorkPlaceComponent "
                         "— cannot staff\n", frameNum, foodName, foodComp);
            continue;
        }
        if (*reinterpret_cast<char*>(wpcIBP + 0x50) == 0) {
            std::fprintf(stderr, "[FoodWorkers] frame=%d: %s comp=%p workplace=%p is OFF "
                         "(+0x50=0) — not staffable yet (state-13 enter turns it on)\n",
                         frameNum, foodName, foodComp, (void*)wpcIBP);
            continue;
        }
        uint64_t desc    = *reinterpret_cast<uint64_t*>(wpcIBP + 0x18);
        uint64_t profKey = desc ? *reinterpret_cast<uint64_t*>(desc + 0x20) : 0;
        if (!profKey) {
            std::fprintf(stderr, "[FoodWorkers] frame=%d: %s comp=%p workplace=%p has no "
                         "profession key (desc=%p) — cannot staff\n",
                         frameNum, foodName, foodComp, (void*)wpcIBP, (void*)desc);
            continue;
        }

        const int32_t before = *reinterpret_cast<int32_t*>(wpcIBP + 0x60);
        const int seated = CitizenEmploy_SeatWorkers(static_cast<int64_t>(townMgr),
                                                     profKey, s_count);
        const int32_t after = *reinterpret_cast<int32_t*>(wpcIBP + 0x60);

        if (s_seatedCount < kMaxTracked) s_seated[s_seatedCount++] = foodComp;

        // Report the ROSTER, not just the cascade's return: the cascade can report a seat
        // while the worker lands on a different workplace of the same profession, and the
        // roster is what the rest of the chain actually reads.
        std::fprintf(stderr, "[FoodWorkers] frame=%d: %s comp=%p workplace=%p profKey=0x%llx "
                     "cascade seated %d/%d — roster(+0x60) %d -> %d%s\n",
                     frameNum, foodName, foodComp, (void*)wpcIBP,
                     (unsigned long long)profKey, seated, s_count, before, after,
                     after > before ? "" :
                        "  <-- NO SEAT LANDED HERE (no idle laborer? the panel-staffing "
                        "driver may already have seated them; or the seat went to another "
                        "workplace on the same key)");
        std::fflush(stderr);
    }

    if (found == 0) {
        std::fprintf(stderr, "[FoodWorkers] frame=%d: scanned %d entities, found ZERO food "
                     "building (set BANISHED_BUILD_DEMO_FISHINGDOCK / _HUNTERLODGE / "
                     "_GATHERERHUT / _CROPFIELD, and the matching BANISHED_LIVE_* flag?)\n",
                     frameNum, n);
        std::fflush(stderr);
    } else if (considered == 0) {
        // Silence here would read as "nothing to do" when it means "all done".
        static bool s_saidDone = false;
        if (!s_saidDone) {
            s_saidDone = true;
            std::fprintf(stderr, "[FoodWorkers] frame=%d: all %d food building(s) already "
                         "handled — no further staffing attempts\n", frameNum, found);
            std::fflush(stderr);
        }
    }
}

}  // namespace

extern "C" void FoodWorkers_Tick(int frameNum) {
    Init();
    if (s_count <= 0) return;
    if (frameNum < kStartFrame) return;
    if (((frameNum - kStartFrame) % kRetry) != 0) return;
    Fire(frameNum);
}
