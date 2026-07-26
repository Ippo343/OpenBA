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
//   0x14012fb00  WorkPlaceComponent::SetMaxWorkers (capacity publish + excess eviction).
//                Was deliberately left unregistered, so that the OTHER retail caller —
//                WorkPlaceUI_AdjustWorkerCount, via fn_WorkplaceComp_SetMaxWorkers — would
//                not newly pick up the eviction loop. That reasoning was backwards: the
//                eviction IS what retail's worker spinner does, and an unregistered address
//                means that call site resolves to 0. Registered now.
//   0x1400f8400  work-scheduler profession re-key. Ported (citizenai_work_scheduler.cpp,
//                CitizenAI_WorkScheduler_Rekey) and registered from that file's own batch.

#include "resolve.h"
#include <cstdint>

// workplacecomponent.cpp — 0x14012f670.
extern "C" void WorkPlaceComponent_LeaveBuilderProfession(void* wpc);
// destroycomponent_workerrelease.cpp — 0x14012f520 / 0x14012fb00.
extern "C" void WorkPlaceComponent_EnterBuilderProfession(int64_t wpc);
extern "C" void WorkPlaceComponent_SetMaxWorkers(int64_t wpc, int newMax);

void RegisterDestroyWorkersBatch() {
    RegisterExeAddr(0x14012f670, reinterpret_cast<uintptr_t>(&WorkPlaceComponent_LeaveBuilderProfession));
    RegisterExeAddr(0x14012f520, reinterpret_cast<uintptr_t>(&WorkPlaceComponent_EnterBuilderProfession));
    RegisterExeAddr(0x14012fb00, reinterpret_cast<uintptr_t>(&WorkPlaceComponent_SetMaxWorkers));
}
