// standalone/reg_destroy_workers.cpp
// Registration wave — the WorkPlaceComponent profession re-seat / worker release pair the
// DestroyComponent state machine drives on demolish + upgrade.
// See code/Game/Component/destroycomponent_workerrelease.cpp for the full analysis.
//
//   0x14012f670  LEAVE the builder profession ("worker release").
//                Body is the ALREADY-EXISTING port WorkPlaceComponent_LeaveBuilderProfession
//                (code/Game/Component/workplacecomponent.cpp), written for the build-
//                completion hand-off and simply never registered at its exe address — so
//                the DestroySM's ResolveAddr handed back 0 and skipped the call. No second
//                copy is introduced.
//   0x14012f520  ENTER the builder profession ("state-4 workplace re-seat").
//                New port, destroycomponent_workerrelease.cpp.
//
// Registering only makes the addresses RESOLVABLE. Both DestroySM call sites are gated at
// runtime by BANISHED_LIVE_DESTROY_WORKERS (opt-in; see destroycomponent_states.cpp).
//
// Deliberately NOT registered here:
//   0x14012fb00  WorkPlaceUI_helper_fb00 (capacity publish + excess eviction) — ported as a
//                file-local static in destroycomponent_workerrelease.cpp. Its other retail
//                caller is the WorkPlaceUI worker up/down spinner, an UNGATED path; giving
//                that the eviction loop is a separate change with its own risk.
//   0x1400f8400  work-scheduler profession re-key — unported in both directions, logged once.

#include "resolve.h"
#include <cstdint>

// workplacecomponent.cpp — 0x14012f670.
extern "C" void WorkPlaceComponent_LeaveBuilderProfession(void* wpc);
// destroycomponent_workerrelease.cpp — 0x14012f520.
extern "C" void WorkPlaceComponent_EnterBuilderProfession(int64_t wpc);

void RegisterDestroyWorkersBatch() {
    RegisterExeAddr(0x14012f670, reinterpret_cast<uintptr_t>(&WorkPlaceComponent_LeaveBuilderProfession));
    RegisterExeAddr(0x14012f520, reinterpret_cast<uintptr_t>(&WorkPlaceComponent_EnterBuilderProfession));
}
