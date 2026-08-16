// standalone/panel_staffing_driver.cpp
//
// THE VIRTUAL PLAYER for worker seating.
//
// Retail has exactly one first-seat mechanism and it is a player click: the workplace
// info panel's workerCountUp element (id 0x1450) dispatched through WorkPlaceUI vtable
// slot 9 (the town-hall Professions spinner is the same cascade behind a different
// widget). Nothing in the engine opens seat #1 by itself — the periodic employment
// allocator (0x1400d43b0) builds its Phase-2 need table under a literal
// `if (WorkPlaceComponent+0x60 != 0)` guard, so a roster-0 workplace is invisible to it,
// and a laborer's own per-tick self-seat searches only the bucket for the profession key
// it already carries. Both facts are retail's, not ours.
//
// An unattended run has no player, so something has to make that click. Until
// 2026-08-15 that "something" was a family of ad-hoc stand-ins that each called the seat
// cascade (FindAvailableBuilding + cbf0/ca20/c5f0/c4b0) DIRECTLY, bypassing the UI:
// BANISHED_PASTURE_HERDERS (pasture_harness.cpp) and BANISHED_BUILDER_AUTOPOOL
// (builder_pool_driver.cpp). Those existed only because the click itself was unproven.
// It is now measured — a schoolhouse went roster 0 -> 1 on the dispatch and held for
// 3600 frames — so both are RETIRED and replaced by this file, which drives the real
// widget instead of reaching around it. Every seat this driver makes now travels the
// same code path a human click travels, which is the whole point: a stand-in that
// bypasses the UI cannot regress when the UI breaks.
//
// Behaviour: every kInterval frames, walk the entity list; for each workplace that is
// accepting (WPC+0x50), carries a profession key (WPC+0x48) and is under capacity
// (roster WPC+0x60 < WorkComponent+0x30), open its info panel, click workerCountUp
// until the roster stops moving or capacity is reached, then close the panel. It is
// self-limiting in both directions: full buildings are skipped, and when no idle
// laborer remains the click stops moving the roster and this pass gives up on that
// building for the interval.
//
// Enabled automatically for UNATTENDED runs (BANISHED_AUTOSTART / BANISHED_AUTOQUIT),
// where there is provably no player. Attended runs leave seating to the human.
//   BANISHED_AUTOSTAFF=0        force off even when unattended
//   BANISHED_AUTOSTAFF=<n>      force on, re-check every n frames (n>1)
//   BANISHED_AUTOSTAFF_TRACE=1  log every considered building, not just seats

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include "Player/edittool_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" int EditTool_HarnessSelectEntity(void* editTool, void* entity);
void EntityUI_SetEditMode_Impl(void* playerController, uint64_t mode);

namespace {

const int kFirstFrame   = 600;    // let the world and its buildings settle first
const int kIntervalDflt = 300;

bool EnvOn(const char* n) {
    const char* v = std::getenv(n);
    return v && v[0] && std::strcmp(v, "0") != 0 && std::strcmp(v, "false") != 0;
}

int  s_interval = -2;             // -2 = env unread, -1 = disabled
bool s_trace    = false;

void Init() {
    if (s_interval != -2) return;
    s_trace = EnvOn("BANISHED_AUTOSTAFF_TRACE");
    const char* e = std::getenv("BANISHED_AUTOSTAFF");
    if (e && e[0]) {
        if (e[0] == '0') { s_interval = -1; return; }
        const int v = std::atoi(e);
        s_interval = (v > 1) ? v : kIntervalDflt;
    } else {
        // No explicit request: stand in for the player only when nobody is at the
        // keyboard. An attended session must keep its own agency over staffing.
        const bool unattended = EnvOn("BANISHED_AUTOSTART") || EnvOn("BANISHED_AUTOQUIT");
        s_interval = unattended ? kIntervalDflt : -1;
    }
    if (s_interval > 0) {
        std::fprintf(stderr, "[AutoStaff] armed: workerCountUp click on every under-capacity "
                     "workplace, every %d frames from frame %d (retail first-seat path; "
                     "set BANISHED_AUTOSTAFF=0 to disable)\n", s_interval, kFirstFrame);
        std::fflush(stderr);
    }
}

uint32_t Tid(const char* n) { return n ? ::Object::GetTypeIdByName(n) : 0; }

void* FindComp(void* ent, uint32_t tid) {
    if (!ent || !tid) return nullptr;
    const int n = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
    uintptr_t a = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
    for (int i = 0; i < n && a; ++i) {
        void** cs = reinterpret_cast<void**>(a + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(cs);
        void* c = *cs;
        if (!c) continue;
        void** vt = *reinterpret_cast<void***>(c);
        if (!vt || !vt[2]) continue;
        if (reinterpret_cast<uint32_t(*)(void*)>(vt[2])(c) == tid) return c;
    }
    return nullptr;
}

// Open ent's info panel, click workerCountUp until the roster stops moving, close it.
// Returns the number of seats actually gained (0 = the click had no effect, which is a
// real answer: no idle laborer, or no WorkPlaceUI on this building's panel).
int SeatViaPanel(void* entityUI, void* ent, uintptr_t wpc, int want) {
    void* tool = *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28);
    if (!tool) {
        EntityUI_SetEditMode_Impl(entityUI, 0);
        tool = *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28);
    }
    if (!tool) return 0;

    EditTool_HarnessSelectEntity(tool, ent);
    void* dialog = *reinterpret_cast<void**>(static_cast<uint8_t*>(tool) + 0x28);
    if (!dialog) return 0;

    int gained = 0;
    void** ctrlArr = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(dialog) + 0x78);
    const int cnt  = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(dialog) + 0x80);
    const uint32_t tidWpUi = Tid("WorkPlaceUI");
    void* wpUi = nullptr;
    for (int i = 0; ctrlArr && i < cnt && !wpUi; ++i) {
        void* c = ctrlArr[i];
        if (!c) continue;
        void** vt = *reinterpret_cast<void***>(c);
        if (vt && vt[2] && reinterpret_cast<uint32_t(*)(void*)>(vt[2])(c) == tidWpUi) wpUi = c;
    }
    if (wpUi) {
        void** uvt = *reinterpret_cast<void***>(wpUi);
        void* slot9 = uvt ? uvt[9] : nullptr;
        if (slot9) {
            for (int i = 0; i < want; ++i) {
                const int before = *reinterpret_cast<int32_t*>(wpc + 0x60);
                // msg 0x12 = button command, id 0x1450 = workerCountUp.
                reinterpret_cast<void (*)(void*, void*, int, int, int, int)>(slot9)(
                    wpUi, nullptr, 0x12, 0x1450, 0, 0);
                const int after = *reinterpret_cast<int32_t*>(wpc + 0x60);
                if (after <= before) break;      // nobody left to seat — stop asking
                gained += after - before;
            }
        }
    }
    EditTool_CloseInfoDialog(reinterpret_cast<EditTool*>(tool),
                             reinterpret_cast<uintptr_t>(dialog));
    return gained;
}

}  // namespace

extern "C" void PanelStaffingDriver_Tick(void* entityUI, int frameNum) {
    Init();
    if (s_interval <= 0 || !entityUI) return;
    if (frameNum < kFirstFrame || ((frameNum - kFirstFrame) % s_interval) != 0) return;

    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;
    const uintptr_t el = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xe8);
    if (!el) return;
    const uintptr_t arr = *reinterpret_cast<uintptr_t*>(el + 0x08);
    const int n = *reinterpret_cast<int32_t*>(el + 0x10);
    const uint32_t tidWpc = Tid("WorkPlaceComponent");
    const uint32_t tidWc  = Tid("WorkComponent");
    if (!tidWpc || !arr) return;

    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* ent = *slot;
        if (!ent) continue;
        void* wpcP = FindComp(ent, tidWpc);
        if (!wpcP) continue;
        const uintptr_t wpc = reinterpret_cast<uintptr_t>(wpcP);
        const uint64_t key    = *reinterpret_cast<uint64_t*>(wpc + 0x48);
        const int      accept = *reinterpret_cast<uint8_t*>(wpc + 0x50);
        const int      roster = *reinterpret_cast<int32_t*>(wpc + 0x60);
        void* wcP = FindComp(ent, tidWc);
        // Capacity is filled lazily by FindAvailableBuilding / the citizen finder, so a
        // 0 here means "not computed yet", not "no seats". Ask for one seat in that case
        // and let the next pass see the real number.
        int cap = wcP ? *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(wcP) + 0x30) : 0;
        if (cap <= 0) cap = 1;
        const bool wants = key != 0 && accept != 0 && roster < cap;
        if (s_trace)
            std::fprintf(stderr, "[AutoStaff]   ent=%p key=0x%llx accepting=%d roster=%d cap=%d %s\n",
                         ent, (unsigned long long)key, accept, roster, cap,
                         wants ? "-> clicking" : "(skip)");
        if (!wants) continue;

        const int gained = SeatViaPanel(entityUI, ent, wpc, cap - roster);
        if (gained > 0 || s_trace)
            std::fprintf(stderr, "[AutoStaff] frame %d: ent=%p key=0x%llx roster %d -> %d "
                         "(cap %d)%s\n", frameNum, ent, (unsigned long long)key, roster,
                         *reinterpret_cast<int32_t*>(wpc + 0x60), cap,
                         gained == 0 ? "  <-- click made no seat (no idle laborer, or this "
                                       "panel has no WorkPlaceUI)" : "");
    }
    std::fflush(stderr);
}
