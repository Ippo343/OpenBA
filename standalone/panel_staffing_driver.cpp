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
// Enabled automatically for UNATTENDED runs (BANISHED_AUTOQUIT), where there is
// provably no player. Attended runs leave seating to the human — including
// BANISHED_AUTOSTART runs, which only skip the menu and are the normal way a person
// launches the game (see the long note in Init()).
//   BANISHED_AUTOSTAFF=0        force off even when unattended
//   BANISHED_AUTOSTAFF=<n>      force on, re-check every n frames (n>1)
//   BANISHED_AUTOSTAFF_TRACE=1  log every considered building, not just seats

#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Object/factory_registration.h"
#include "Player/edittool_funcs.h"
#include "options.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Component/StatusIconComponent.h"
#include "Component/WorkComponent.h"
#include "Component/WorkPlaceComponent.h"
#include "Dialog/WorkPlaceUI.h"
#include "Dialog/exe_bodies.h"   // EntityUI_SetEditMode_Impl

extern "C" int EditTool_HarnessSelectEntity(void* editTool, void* entity);
extern "C" void* EditTool_HarnessAcquire(void* entityUI);
// EntityUI_SetEditMode_Impl: Dialog/exe_bodies.h.

namespace {

const int kFirstFrame   = 600;    // let the world and its buildings settle first
const int kIntervalDflt = 300;

// Same truthy test the driver has always used, but over a value already read (the RAW Options
// fields hold exactly what getenv returned).
bool EnvOnValue(const char* v) {
    return v && v[0] && std::strcmp(v, "0") != 0 && std::strcmp(v, "false") != 0;
}

int  s_interval = -2;             // -2 = env unread, -1 = disabled
bool s_trace    = false;

void Init() {
    if (s_interval != -2) return;
    s_trace = EnvOnValue(Opt().autoStaffTrace);
    const char* e = Opt().autoStaff;
    if (e && e[0]) {
        if (e[0] == '0') { s_interval = -1; return; }
        const int v = std::atoi(e);
        s_interval = (v > 1) ? v : kIntervalDflt;
    } else {
        // No explicit request: stand in for the player only when nobody is at the
        // keyboard. An attended session must keep its own agency over staffing.
        //
        // BANISHED_AUTOSTART is NOT part of that test, despite naming an automated
        // thing. It means only "skip the menu and start a game" — `make sa-run` sets
        // it for every ordinary interactive session, so treating it as "no player"
        // armed this driver underneath a human
        // and made every panel in the game close by itself.
        //
        // The cost of that miscategorisation was entirely diagnostic: SeatViaPanel
        // acquires the SHARED EditTool, selects its own subject and then calls
        // EditTool_CloseInfoDialog, so it silently destroyed whatever panel the player
        // had open, roughly every five seconds. It logs only when it actually gains a
        // seat, so a workplace permanently under capacity (roster 6 of cap 12) re-ran
        // the whole open/click/close cycle forever while printing nothing at all —
        // indistinguishable, from the outside, from the panels being broken.
        //
        // BANISHED_AUTOQUIT is the honest signal: a run that intends to exit on its own
        // is one nobody is watching. An interactive session that still wants the virtual
        // player can ask for it explicitly with BANISHED_AUTOSTAFF=1.
        const bool unattended = Opt().autoquit;
        s_interval = unattended ? kIntervalDflt : -1;
    }
    if (s_interval > 0) {
        std::fprintf(stderr, "[AutoStaff] armed: workerCountUp click on every under-capacity "
                     "workplace, every %d frames from frame %d (retail first-seat path; "
                     "set BANISHED_AUTOSTAFF=0 to disable)\n", s_interval, kFirstFrame);
        std::fflush(stderr);
    }
}

void* FindComp(void* ent, uint32_t tid);

// The building's status-icon bitfield (StatusIconComponent+0xF0). Bit 4 (0x10) is the
// "no workers" warning: raised by WorkPlaceComponent_SetAccepting on an unstaffed
// workplace and cleared ONLY by WorkplaceComponent_Attach. Printed either side of the
// click so a seat that fills the roster while leaving the icon up is legible as such
// rather than being reported as a working seat. -1 = no StatusIconComponent.
int IconMask(void* ent) {
    void* si = FindComp(ent, StatusIconComponent::TypeId());
    return si ? (int)*reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(si) + 0xF0) : -1;
}

void* FindComp(void* ent, uint32_t tid) {
    if (!ent || !tid) return nullptr;
    const int n = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
    uintptr_t a = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
    for (int i = 0; i < n && a; ++i) {
        void** cs = reinterpret_cast<void**>(a + static_cast<uintptr_t>(i) * 8);
        void* c = *cs;
        if (!c) continue;
        if (reinterpret_cast<Object::Base*>(c)->Type() == tid) return c;   // B.3.8 bridge
    }
    return nullptr;
}

// Open ent's info panel, click workerCountUp until the roster stops moving, close it.
// Returns the number of seats actually gained (0 = the click had no effect, which is a
// real answer: no idle laborer, or no WorkPlaceUI on this building's panel).
int SeatViaPanel(void* entityUI, void* ent, uintptr_t wpc, int want) {
    // Not just "is a tool active" — is it an EDIT tool. A build tool left armed by a
    // placement harness sits in the same slot, and its +0x28/+0x30 are drag state, not
    // dialog/entity refs (the FencedTool crash, 2026-08-19). Acquire switches edit mode
    // back the way a player would.
    void* tool = EditTool_HarnessAcquire(entityUI);
    if (!tool) return 0;

    EditTool_HarnessSelectEntity(tool, ent);
    void* dialog = *reinterpret_cast<void**>(static_cast<uint8_t*>(tool) + 0x28);
    if (!dialog) return 0;

    int gained = 0;
    void** ctrlArr = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(dialog) + 0x78);
    const int cnt  = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(dialog) + 0x80);
    const uint32_t tidWpUi = WorkPlaceUI::TypeId();
    WorkPlaceUI* wpUi = nullptr;
    for (int i = 0; ctrlArr && i < cnt && !wpUi; ++i) {
        void* c = ctrlArr[i];
        if (!c) continue;
        if (reinterpret_cast<Object::Base*>(c)->Type() == tidWpUi)   // B.3.8 bridge
            wpUi = reinterpret_cast<WorkPlaceUI*>(c);                // B.3.8 bridge
    }
    if (wpUi) {
        // slot 9: WorkPlaceUI's OnElementEvent override (real class, ENDGAME B.3.8).
        {
            // BANISHED_AUTOSTAFF_UNSEAT=1 — click workerCountDown (0x1451) once on a
            // staffed workplace before seating. The DOWN click is the path the old
            // g_wpcRosterOnly guards were justified by ("READ of 0x10 in Detach's e710
            // getter, hit when the player LOWERS a count"), and an add-only soak never
            // touches it, so removing those guards can only be called measured if this
            // runs. The seat loop below immediately refills the seat.
            static const int unseat = EnvOnValue(Opt().autoStaffUnseat) ? 1 : 0;
            if (unseat && *reinterpret_cast<int32_t*>(wpc + 0x60) > 0) {
                const int b = *reinterpret_cast<int32_t*>(wpc + 0x60);
                wpUi->OnElementEvent(nullptr, 0x12, 0x1451, 0, 0, 0, nullptr);
                std::fprintf(stderr, "[AutoStaff] UNSEAT click: roster %d -> %d\n",
                             b, *reinterpret_cast<int32_t*>(wpc + 0x60));
                std::fflush(stderr);
                ++want;   // refill the seat we just freed
            }
            for (int i = 0; i < want; ++i) {
                const int before = *reinterpret_cast<int32_t*>(wpc + 0x60);
                // msg 0x12 = button command, id 0x1450 = workerCountUp.
                wpUi->OnElementEvent(nullptr, 0x12, 0x1450, 0, 0, 0, nullptr);
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

    void* gw = (GameUpdateLoop::_singleton != nullptr) ? GameUpdateLoop::_singleton : nullptr;
    if (!gw) return;
    const uintptr_t el = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xe8);
    if (!el) return;
    const uintptr_t arr = *reinterpret_cast<uintptr_t*>(el + 0x08);
    const int n = *reinterpret_cast<int32_t*>(el + 0x10);
    const uint32_t tidWpc = WorkPlaceComponent::TypeId();
    const uint32_t tidWc  = WorkComponent::TypeId();
    if (!tidWpc || !arr) return;

    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
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
        // With UNSEAT armed a FULL workplace is still worth visiting — the point of that
        // mode is to drive the down-click, and a building at capacity is exactly where a
        // player lowers a count.
        const bool wants = key != 0 && accept != 0 &&
                           (roster < cap || (EnvOnValue(Opt().autoStaffUnseat) && roster > 0));
        if (s_trace)
            std::fprintf(stderr, "[AutoStaff]   ent=%p key=0x%llx accepting=%d roster=%d cap=%d %s\n",
                         ent, (unsigned long long)key, accept, roster, cap,
                         wants ? "-> clicking" : "(skip)");
        if (!wants) continue;

        const int iconBefore = IconMask(ent);
        const int gained = SeatViaPanel(entityUI, ent, wpc, cap - roster);
        const int iconAfter  = IconMask(ent);
        const int rosterNow  = *reinterpret_cast<int32_t*>(wpc + 0x60);
        if (gained > 0 || s_trace)
            std::fprintf(stderr, "[AutoStaff] frame %d: ent=%p key=0x%llx roster %d -> %d "
                         "(cap %d) icon 0x%x -> 0x%x%s%s\n", frameNum, ent,
                         (unsigned long long)key, roster, rosterNow, cap,
                         iconBefore, iconAfter,
                         gained == 0 ? "  <-- click made no seat (no idle laborer, or this "
                                       "panel has no WorkPlaceUI)" : "",
                         (rosterNow > 0 && iconAfter > 0 && (iconAfter & 0x10))
                             ? "  <-- STUCK: roster filled but the no-workers bit 4 is "
                               "still set (the ? icon stays up)" : "");
    }
    std::fflush(stderr);
}
