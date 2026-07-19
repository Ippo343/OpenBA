// standalone/reg_citizen_25_tick.cpp
// openfuncs step 2.5(b) — behavior-tick group registration wave.
//
// Registers the five formerly-NULL exe addresses reached from the CitizenBehavior
// per-tick update (0x14008c2b0) drain, lifting them from dispatcher-guarded no-ops
// to live faithful ports:
//
//   addr        symbol                       impl file
//   ----------  ---------------------------  --------------------------------------------
//   0x140025a10 PathBST_Insert_25a10         code/Game/Action/action_pf_avl.cpp
//   0x140025b30 PathPairs_Clear_25b30        code/Game/Action/action_pf_avl.cpp
//   0x1400d6c60 Entity_ReassignCost_6c60     code/Game/Component/citizen_reassign_workers.cpp
//   0x1400d62a0 PathGraph_ReassignWorkers    code/Game/Component/citizen_reassign_workers.cpp
//   0x140095580 EventLog_AddEvent            code/Game/Game/eventlog.cpp   (existing port)
//
// 0x140095580 is the shortage/notification event writer reached via
// fn_NotificationEvent (CitizenBehavior_CheckThresholds) and fn_EventNotifier_Fire
// (scenario-complete). Registering it lights up the tick's CheckThresholds shortage
// scanner; the duplicate BANISHED_LIVE_EVENTS scanner in gameupdateloop.cpp was
// retired at the same time so the two paths can never double-fire (see that file).
//
// The integrator wires RegisterCitizen25Tick() into RegisterCodeFunctions() in
// standalone/address_registry.cpp (done centrally — not here).
#include "resolve.h"
#include <cstdint>

// action_pf_avl.cpp
extern "C" uint64_t* PathBST_Insert_25a10(int64_t*, uint64_t*, uint32_t*);   // 0x140025a10
extern "C" void      PathPairs_Clear_25b30(int64_t*);                        // 0x140025b30
// citizen_reassign_workers.cpp
extern "C" uint64_t  Entity_ReassignCost_6c60(int64_t, void**, void**);      // 0x1400d6c60
extern "C" void      PathGraph_ReassignWorkers(void*);                       // 0x1400d62a0
// eventlog.cpp (existing port; signature-exact with fn_NotificationEvent)
extern "C" void      EventLog_AddEvent(void* eventLog, int eventType, int severity, void* entityIBP);  // 0x140095580

extern "C" void RegisterCitizen25Tick() {
    RegisterExeAddr(0x140025a10, reinterpret_cast<uintptr_t>(&PathBST_Insert_25a10));
    RegisterExeAddr(0x140025b30, reinterpret_cast<uintptr_t>(&PathPairs_Clear_25b30));
    RegisterExeAddr(0x1400d6c60, reinterpret_cast<uintptr_t>(&Entity_ReassignCost_6c60));
    RegisterExeAddr(0x1400d62a0, reinterpret_cast<uintptr_t>(&PathGraph_ReassignWorkers));
    RegisterExeAddr(0x140095580, reinterpret_cast<uintptr_t>(&EventLog_AddEvent));
}
