// standalone/orchard_growth_harness.cpp
// BANISHED_ORCHARD_GROW=<n> — fast-forward orchard tree growth so the HARVEST half of the
// state machine can be exercised in a single run instead of a multi-hour soak.
//
// WHY THIS EXISTS: the orchard chain is now live end to end up to tending — 12 trees plant,
// bind to their tiles, and mature. But maturity climbs about 0.01 per ~200 diag ticks
// (measured 2026-08-10: 0.000 -> 0.0085 -> 0.0192 -> 0.0299), so a tree reaching 1.0 takes
// game-YEARS. Nothing downstream of maturity can be tested at that cadence: fruit ripening,
// TryHarvest's 0.99 threshold, the event-4 handler, state 11, HarvestOrchard.
//
// WHAT IT DOES *NOT* DO: it does not touch, rewrite or bypass any ported game code, and it
// does not fake a harvest. It advances exactly the two simulation VALUES the real code reads,
// in the real order, and then gets out of the way — every gate downstream still has to pass
// on its own:
//   1. tree maturity at NaturalResourceComponent(slot+0x10) + 0x28, up to 1.0. This is the
//      gate FruitComponent::Update tests as `growthRate >= 1.0f` before it will accumulate
//      ANY ripeness — which is why a young orchard correctly bears nothing.
//   2. only once a tree is mature, fruit ripeness at FruitComponent(slot+0x18) + 0x30, the
//      field OrchardComponent_GetGrowth averages and TryHarvest compares against 0.99.
// So a harvest driven by this harness is still a real harvest: TryHarvest still has to fire,
// the year check at +0x4c still has to pass, and state 11 still has to do its job.
//
// Env:
//   BANISHED_ORCHARD_GROW      unset / 0 / false  -> disabled (default; this is a test tool,
//                                                    never a default-on feature)
//                              1                  -> default step, 0.05 per tick
//                              <N> for N > 1      -> step = N/100 per tick (e.g. 25 -> 0.25)
//
// Cadence: every kTickFrames frames from kStartFrame. Slots are only advanced while their
// slot state (+0x04) is 1 (planted and bound) — a hole or an unplanted slot is left alone,
// so the harness cannot paper over a planting failure.
//
// Offsets, all already used by orchard_diag.cpp / orchardcomponent.cpp:
//   gameWorld+0xe8 EntityList {+0x08 data, +0x10 count}; entity+0x30/+0x38 component array
//   OrchardComponent +0x50 slot array, +0x58 slot count; slot stride 0x28
//   slot +0x04 state, +0x10 NaturalResourceComponent, +0x18 FruitComponent
//   natRes +0x28 maturity; fruit +0x30 ripeness

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include "options.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int   kStartFrame = 120;
constexpr int   kTickFrames = 60;
constexpr float kDefaultStep = 0.05f;
constexpr float kMature      = 1.0f;

// -1 = unchecked, 0 = off, 1 = on
int   s_state = -1;
float s_step  = kDefaultStep;

bool Enabled() {
    if (s_state >= 0) return s_state == 1;
    const char* e = Opt().orchardGrow;
    if (!e || !e[0] || std::strcmp(e, "0") == 0 || std::strcmp(e, "false") == 0) {
        s_state = 0;
        return false;
    }
    const double v = std::atof(e);
    if (v > 1.0) s_step = static_cast<float>(v / 100.0);
    s_state = 1;
    std::fprintf(stderr, "[OrchardGrow] ENABLED (BANISHED_ORCHARD_GROW=\"%s\") — advancing tree "
                 "maturity then fruit ripeness by %.3f every %d frames. This is a TEST HARNESS: "
                 "it moves two simulation values, it does not skip any gate downstream of them "
                 "(FruitComponent's maturity gate, TryHarvest's 0.99 threshold and its "
                 "once-per-year check all still apply).\n", e, s_step, kTickFrames);
    std::fflush(stderr);
    return true;
}

}  // namespace

extern "C" void OrchardGrowthHarness_Tick(int frameNum) {
    if (!Enabled()) return;
    if (frameNum < kStartFrame || (frameNum % kTickFrames) != 0) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    uintptr_t entityList = *reinterpret_cast<uintptr_t*>(gw + 0xe8);
    if (!entityList) return;
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entityList + 0x08);
    int       n   = *reinterpret_cast<int32_t*>(entityList + 0x10);
    const uint32_t tidOrchardComp = ::Object::GetTypeIdByName("OrchardComponent");
    if (!arr || n <= 0 || !tidOrchardComp) return;

    int orchards = 0, grown = 0, ripened = 0, mature = 0, slotsSeen = 0;
    float matMax = 0.0f, ripeMax = 0.0f;

    for (int i = 0; i < n; ++i) {
        void** eslot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* ent = *eslot;
        if (!ent) continue;

        int       cN = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
        uintptr_t cA = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
        void* orchComp = nullptr;
        for (int c = 0; c < cN && cA; ++c) {
            void** cs = reinterpret_cast<void**>(cA + static_cast<uintptr_t>(c) * 8);
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
        ++orchards;

        uintptr_t oc    = reinterpret_cast<uintptr_t>(orchComp);
        int       count = *reinterpret_cast<int32_t*>(oc + 0x58);
        uintptr_t slots = *reinterpret_cast<uintptr_t*>(oc + 0x50);
        if (!slots || count <= 0) continue;

        for (int s = 0; s < count; ++s) {
            uintptr_t slot = slots + static_cast<uintptr_t>(s) * 0x28;
            if (*reinterpret_cast<int32_t*>(slot + 0x04) != 1) continue;  // only planted+bound
            ++slotsSeen;

            void* natRes = *reinterpret_cast<void**>(slot + 0x10);
            void* fruit  = *reinterpret_cast<void**>(slot + 0x18);
            if (!natRes) continue;

            float* pMat = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(natRes) + 0x28);
            if (*pMat < kMature) {
                *pMat += s_step;
                if (*pMat > kMature) *pMat = kMature;
                ++grown;
            } else {
                ++mature;
                // Mature: let ripeness move. FruitComponent::Update also advances this field
                // on its own now that its gate passes; we simply add to it, so the two
                // sources compose rather than fight.
                if (fruit) {
                    float* pRipe = reinterpret_cast<float*>(
                        reinterpret_cast<uintptr_t>(fruit) + 0x30);
                    if (*pRipe < kMature) {
                        *pRipe += s_step;
                        if (*pRipe > kMature) *pRipe = kMature;
                        ++ripened;
                    }
                    if (*pRipe > ripeMax) ripeMax = *pRipe;
                }
            }
            if (*pMat > matMax) matMax = *pMat;
        }
    }

    // Report OUTCOMES, not intent: "the harness ran" is worthless, "it moved N maturities and
    // M ripenesses and the maxima are here" is what tells you whether harvest should be close.
    static int s_reports = 0;
    if (slotsSeen && (++s_reports <= 6 || (s_reports % 20) == 0)) {
        std::fprintf(stderr, "[OrchardGrow] frame=%d orchards=%d plantedSlots=%d | advanced "
                     "maturity=%d (max=%.3f) mature=%d | advanced ripeness=%d (max=%.3f)\n",
                     frameNum, orchards, slotsSeen, grown, matMax, mature, ripened, ripeMax);
        std::fflush(stderr);
    }
}
