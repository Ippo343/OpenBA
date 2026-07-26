// standalone/death_harness.cpp
// Diagnostic driver: kill N citizens on a chosen frame so the removal path
// (HandleLifeEvent -> MarkForRemoval -> the +0x38 broadcast -> CitizenComponent::
// OnRemove) can be exercised deterministically. Natural deaths are decades of game
// time away in a fresh start-town, which makes the citizen-removal teardown
// effectively untestable otherwise.
//
// Same shape as tool_harness.cpp: env-gated, one-shot, ticked from
// GameUpdateLoop::Update. No-op unless BANISHED_KILL_CITIZENS is set.
//
//   BANISHED_KILL_CITIZENS=<n>   how many citizens to kill (0 / unset = disabled)
//   BANISHED_KILL_AT_FRAME=<f>   frame to fire on (default 900)
//   BANISHED_FAMILY_CENSUS=<f>   every f frames, report how many citizens carry family
//                                links — spouse +0x250, ring head +0x260, partner +0x208
//   BANISHED_KILL_FAMILY=<n>     poll until citizens with a family link EXIST, then kill
//                                up to n of them. This is the only practical way to reach
//                                CitizenComponent::OnRemove's step-4 family branch: the
//                                start town begins with every link null, so an untargeted
//                                kill (or a natural death) exercises the empty path.
//
// Pair it with BANISHED_LIVE_CITIZEN_ONREMOVE=1 and BANISHED_ONREMOVE_TRACE=1 to see
// each teardown, and with BANISHED_OBJAUDIT=1 to watch the object tally.
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" uint64_t CitizenComponent_HandleLifeEvent(void*, int, const char*); // 0x140116dd0

namespace {
int  s_count       = -1;   // -1 = not yet read from the environment
int  s_atFrame     = 900;
int  s_censusEvery = 0;
int  s_killFamily  = 0;
bool s_fired       = false;
bool s_firedFamily = false;

void Init() {
    if (s_count >= 0) return;
    const char* n = std::getenv("BANISHED_KILL_CITIZENS");
    s_count = (n && n[0]) ? std::atoi(n) : 0;
    const char* f = std::getenv("BANISHED_KILL_AT_FRAME");
    if (f && f[0]) s_atFrame = std::atoi(f);
    const char* c = std::getenv("BANISHED_FAMILY_CENSUS");
    s_censusEvery = (c && c[0]) ? std::atoi(c) : 0;
    const char* kf = std::getenv("BANISHED_KILL_FAMILY");
    s_killFamily = (kf && kf[0]) ? std::atoi(kf) : 0;
    if (s_count > 0) {
        std::fprintf(stderr, "[DeathHarness] ENABLED — killing %d citizen(s) at frame %d\n",
                     s_count, s_atFrame);
    }
    if (s_censusEvery > 0)
        std::fprintf(stderr, "[DeathHarness] family census every %d frames\n", s_censusEvery);
    if (s_killFamily > 0)
        std::fprintf(stderr, "[DeathHarness] will kill %d citizen(s) WITH family links "
                     "once any exist\n", s_killFamily);
    std::fflush(stderr);
}

// The town citizen list: array at townMgr+0xa0, count at +0xa8.
bool TownList(int64_t** outList, int* outCount, int64_t* outTownMgr) {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return false;
    uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    int64_t townMgr = *reinterpret_cast<int64_t*>(gw + 0xf0);
    if (!townMgr) return false;
    *outTownMgr = townMgr;
    *outList  = *reinterpret_cast<int64_t**>(townMgr + 0xa0);
    *outCount = *reinterpret_cast<int*>(townMgr + 0xa8);
    return *outList != nullptr && *outCount > 0;
}

inline bool HasFamily(int64_t c) {
    return c && (*reinterpret_cast<int64_t*>(c + 0x250) != 0 ||
                 *reinterpret_cast<int64_t*>(c + 0x260) != 0);
}

void Census(int frameNum) {
    int64_t* list; int n; int64_t townMgr;
    if (!TownList(&list, &n, &townMgr)) return;
    int spouse = 0, ring = 0, partner = 0, host = 0;
    // Gender (+0x38): 0 = female, non-zero = male. Retail's per-citizen init writes it for
    // every colonist; the founding spawner reproduces it from the mesh variant. A town that
    // reports female == citizens is one where nobody can marry (the opposite-gender test in
    // helper_fe40 can never pass) and every adult passes the reproduction gate — the exact
    // symptom of the gender write being skipped. Tracked here so the A/B is measurable.
    int female = 0, male = 0;
    for (int i = 0; i < n; i++) {
        int64_t c = list[i];
        if (!c) continue;
        if (*reinterpret_cast<int64_t*>(c + 0x250) != 0) spouse++;
        if (*reinterpret_cast<int64_t*>(c + 0x260) != 0) ring++;
        if (*reinterpret_cast<int64_t*>(c + 0x208) != 0) partner++;
        if (*reinterpret_cast<int64_t*>(c + 0x258) != 0) host++;
        if (*reinterpret_cast<int32_t*>(c + 0x38) == 0) female++; else male++;
    }
    std::fprintf(stderr, "[FamilyCensus] frame %d: %d citizens — spouse(+0x250)=%d "
                 "ring(+0x260)=%d partner(+0x208)=%d host(+0x258)=%d "
                 "gender(+0x38) female=%d male=%d\n",
                 frameNum, n, spouse, ring, partner, host, female, male);
    std::fflush(stderr);
}
}  // namespace

extern "C" void CitizenDeathHarness_Tick(int frameNum) {
    Init();

    if (s_censusEvery > 0 && frameNum > 0 && (frameNum % s_censusEvery) == 0)
        Census(frameNum);

    // Targeted kill: wait for family links to exist, then take those citizens.
    if (s_killFamily > 0 && !s_firedFamily && frameNum > 300 && (frameNum % 300) == 0) {
        int64_t* list; int n; int64_t townMgr;
        if (TownList(&list, &n, &townMgr)) {
            int64_t victims[16];
            int want = 0;
            for (int i = 0; i < n && want < s_killFamily && want < 16; i++)
                if (HasFamily(list[i])) victims[want++] = list[i];
            if (want > 0) {
                s_firedFamily = true;
                std::fprintf(stderr, "[DeathHarness] frame %d: %d citizen(s) WITH family "
                             "links found — killing them (list=%d entries)\n",
                             frameNum, want, n);
                std::fflush(stderr);
                for (int i = 0; i < want; i++) {
                    int64_t v = victims[i];
                    std::fprintf(stderr, "[DeathHarness] killing family citizen comp=%p "
                                 "spouse=%p ring=%p\n", reinterpret_cast<void*>(v),
                                 *reinterpret_cast<void**>(v + 0x250),
                                 *reinterpret_cast<void**>(v + 0x260));
                    std::fflush(stderr);
                    CitizenComponent_HandleLifeEvent(reinterpret_cast<void*>(v), 3, nullptr);
                }
                std::fprintf(stderr, "[DeathHarness] family kill done — list = %d entries "
                             "(was %d)\n", *reinterpret_cast<int*>(townMgr + 0xa8), n);
                std::fflush(stderr);
            }
        }
    }

    if (s_count <= 0 || s_fired || frameNum < s_atFrame) return;
    s_fired = true;

    int64_t* list; int n; int64_t townMgr;
    if (!TownList(&list, &n, &townMgr)) return;
    std::fprintf(stderr, "[DeathHarness] frame %d: town citizen list = %d entries\n",
                 frameNum, n);

    // Snapshot first: each kill tears the victim out of this very list.
    int want = (s_count < n) ? s_count : n;
    int64_t victims[64];
    if (want > 64) want = 64;
    for (int i = 0; i < want; i++) victims[i] = list[i];

    for (int i = 0; i < want; i++) {
        if (!victims[i]) continue;
        std::fprintf(stderr, "[DeathHarness] killing citizen %d/%d comp=%p\n",
                     i + 1, want, reinterpret_cast<void*>(victims[i]));
        std::fflush(stderr);
        CitizenComponent_HandleLifeEvent(reinterpret_cast<void*>(victims[i]), 3, nullptr);
    }

    std::fprintf(stderr, "[DeathHarness] done — town citizen list = %d entries "
                 "(was %d; expect %d if OnRemove is live, unchanged if it is not)\n",
                 *reinterpret_cast<int*>(townMgr + 0xa8), n, n - want);
    std::fflush(stderr);
}
