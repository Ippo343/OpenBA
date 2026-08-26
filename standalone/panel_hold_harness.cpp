// standalone/panel_hold_harness.cpp
//
// Open one building's info panel and LEAVE IT OPEN, so the layout can be looked at.
//
// Every existing way a panel opens in an unattended run closes it again immediately:
// panel_staffing_driver.cpp acquires the edit tool, selects the entity, clicks
// workerCountUp and calls EditTool_CloseInfoDialog on the way out. That is correct for
// seating, but it means a rendering or layout defect on a panel -- the tavern's squashed
// barrel icon, say -- can only be seen by a human clicking the building, which an
// autostart run has nobody to do.
//
// This does the same select, skips the close, and then stops. One panel, held open for
// the rest of the run, ready for a window capture.
//
// BANISHED_PANEL_HOLD=<ComponentTypeName>   e.g. ConsumeProduceComponent
// BANISHED_PANEL_HOLD_FRAME=<n>             when to open it (default 3000; leave time
//                                           for staffing and, for a tavern, for the
//                                           inventory to have something in it)
//
// Diagnostic harness: it drives the same retail path a player click drives and writes
// nothing of its own.

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include "Player/edittool_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" int   EditTool_HarnessSelectEntity(void* editTool, void* entity);
extern "C" void* EditTool_HarnessAcquire(void* entityUI);

namespace {

uint32_t Tid(const char* n) { return ::Object::GetTypeIdByName(n); }

void* FindComp(void* ent, uint32_t tid) {
    if (!ent || !tid) return nullptr;
    const uintptr_t e = reinterpret_cast<uintptr_t>(ent);
    const int n = *reinterpret_cast<int32_t*>(e + 0x38);
    const uintptr_t arr = *reinterpret_cast<uintptr_t*>(e + 0x30);
    if (!arr || n <= 0 || n > 4096) return nullptr;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* c = *slot;
        if (!c) continue;
        typedef uint32_t (*GetTypeId_t)(void*);
        const uint32_t t = (reinterpret_cast<GetTypeId_t>(
            *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(c) + 0x10)))(c);
        if (t == tid) return (!fn_IsDerivedFrom || fn_IsDerivedFrom(c, tid)) ? c : nullptr;
    }
    return nullptr;
}

}  // namespace

extern "C" void PanelHoldHarness_Tick(void* entityUI, int frameNum) {
    static int  s_at   = -2;
    static bool s_done = false;
    static const char* s_want = nullptr;

    if (s_at == -2) {
        s_want = std::getenv("BANISHED_PANEL_HOLD");
        if (!s_want || !s_want[0]) { s_at = -1; return; }
        const char* f = std::getenv("BANISHED_PANEL_HOLD_FRAME");
        s_at = (f && f[0]) ? std::atoi(f) : 3000;
        if (s_at < 1) s_at = 3000;
    }
    if (s_at < 0 || s_done || !entityUI || frameNum < s_at) return;

    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;
    const uintptr_t el = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xe8);
    if (!el) return;
    const uintptr_t arr = *reinterpret_cast<uintptr_t*>(el + 0x08);
    const int n = *reinterpret_cast<int32_t*>(el + 0x10);
    const uint32_t tid = Tid(s_want);
    if (!arr || n <= 0 || !tid) {
        s_done = true;
        std::fprintf(stderr, "[PanelHold] no type id for \"%s\" — nothing opened\n", s_want);
        std::fflush(stderr);
        return;
    }

    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* ent = *slot;
        if (!ent || !FindComp(ent, tid)) continue;

        void* tool = EditTool_HarnessAcquire(entityUI);
        if (!tool) return;                    // tool not ready yet; try again next frame
        EditTool_HarnessSelectEntity(tool, ent);
        void* dialog = *reinterpret_cast<void**>(static_cast<uint8_t*>(tool) + 0x28);
        s_done = true;
        // Deliberately NOT calling EditTool_CloseInfoDialog — the whole point is that the
        // panel stays up.
        std::fprintf(stderr, "[PanelHold] frame %d: opened %s panel ent=%p dialog=%p "
                             "(HELD OPEN — capture the window now)\n",
                     frameNum, s_want, ent, dialog);
        std::fflush(stderr);
        return;
    }
}
