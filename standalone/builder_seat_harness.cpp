// standalone/builder_seat_harness.cpp
//
// BANISHED_BUILDERS=<n>[,<settleFrame>] — seat N citizens as BUILDERS, the way the
// town-hall Professions spinner does.
//
// WHY THIS EXISTS
// Road construction never happens in an unattended town, and the reason is not the
// road code. RoadTool's commit posts one BuildRoad work item per tile into the work
// scheduler (gw+0x118), and the scheduler files each item into a per-PROFESSION
// bucket chosen by the work's vtable slot +0x50. BuildRoad's slot 10 (0x1400e4960)
// returns *(*(gw+0xb0)+0x38) — the BUILDER key. A citizen's idle work search
// (CitizenAI_State_Idle -> 0x1400f7830) only ever looks in the bucket keyed by its
// OWN profession (citizen+0x1f0). So with no builder in town the items sit there
// forever: measured with BANISHED_ROAD_CENSUS, 620 tiles queued, 620 items in the
// (BUILDER) bucket, 0 ever taken.
//
// Builders are a player-set profession: retail seats them from the town-hall
// Professions spinner, which loops CitizenList::AddWorker (0x140091130) once per
// requested worker. That call is FindAvailableBuilding followed by the four-strategy
// AssignWorker cascade, and retail deliberately lets a NULL "available building"
// through it — c5f0/c4b0 only ever STORE the pointer, so the citizen's profession
// flips with no roster attach (see the note on Strat_c5f0 in
// code/Game/Component/citizen_employ_cascade.cpp). That is what makes a builder
// possible in a town whose only pending work is a road: there is no building to
// attach to, and retail does not require one.
//
// So this harness is the same class of stand-in as standalone/panel_staffing_driver.cpp
// — it presses a widget the unattended build has no player to press. The seat itself
// is retail's (0x140091130, already registered by reg_townhallui.cpp); only the
// pressing is ours.

#include "resolve.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" void CitizenList_AddWorker_91130(int64_t townMgr, uint64_t professionKey);

namespace {

int  s_want   = -2;    // -2 = env unread, -1 = disabled
int  s_settle = 300;
bool s_fired  = false;

void Init() {
    if (s_want != -2) return;
    s_want = -1;
    const char* e = std::getenv("BANISHED_BUILDERS");
    if (!e || !e[0]) return;
    int n = std::atoi(e);
    if (n <= 0) return;
    s_want = n;
    if (const char* comma = std::strchr(e, ',')) {
        int f = std::atoi(comma + 1);
        if (f > 0) s_settle = f;
    }
}

// Count citizens currently holding `key` as their profession — the same walk the
// Professions panel's "N" column does (townMgr+0xa0 array / +0xa8 count, profession
// at citizen+0x1f0).
int CountProfession(uint8_t* townMgr, uint64_t key) {
    void** arr = *reinterpret_cast<void***>(townMgr + 0xa0);
    int    n   = *reinterpret_cast<int*>(townMgr + 0xa8);
    if (!arr || n <= 0) return 0;
    int hits = 0;
    for (int i = 0; i < n; ++i) {
        void* c = arr[i];
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
        if (!c) continue;
        if (*reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(c) + 0x1f0) == key) ++hits;
    }
    return hits;
}

}  // namespace

extern "C" void BuilderSeat_Tick(int frameNum) {
    Init();
    if (s_want <= 0 || s_fired) return;
    if (frameNum < s_settle) return;

    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;
    uint8_t* townMgr = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(gw) + 0xf0);
    uint8_t* profMgr = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(gw) + 0xb0);
    if (!townMgr || !profMgr) return;

    s_fired = true;
    const uint64_t builderKey = *reinterpret_cast<uint64_t*>(profMgr + 0x38);
    const int before = CountProfession(townMgr, builderKey);

    for (int i = 0; i < s_want; ++i)
        CitizenList_AddWorker_91130(reinterpret_cast<int64_t>(townMgr), builderKey);

    const int after = CountProfession(townMgr, builderKey);

    // Correlation for the ValidateTask crash (BANISHED_WORKVALIDATE_TRACE): list the
    // citizens now holding the builder key, so a later [WorkValidate] BAD VTABLE obj=
    // can be matched against the set this harness actually converted.
    if (const char* t = std::getenv("BANISHED_WORKVALIDATE_TRACE")) if (t[0] && t[0] != '0') {
        void** carr = *reinterpret_cast<void***>(townMgr + 0xa0);
        int    cn   = *reinterpret_cast<int*>(townMgr + 0xa8);
        std::fprintf(stderr, "[BuilderSeat] converted citizens (roster %d):", cn);
        for (int i = 0; i < cn; ++i) {
            void* c = carr[i];
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
            if (!c) continue;
            if (*reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(c) + 0x1f0) == builderKey)
                std::fprintf(stderr, " %p", c);
        }
        std::fprintf(stderr, "\n");
        std::fflush(stderr);
    }
    std::fprintf(stderr, "[BuilderSeat] f=%d requested %d builders (key=%016llx): "
                         "profession count %d -> %d\n",
                 frameNum, s_want, (unsigned long long)builderKey, before, after);
    std::fflush(stderr);
}
