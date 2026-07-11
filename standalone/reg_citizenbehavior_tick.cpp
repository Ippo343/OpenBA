// standalone/reg_citizenbehavior_tick.cpp
// Register the CitizenBehavior per-tick update (0x14008c2b0) so the standalone
// GameUpdateLoop::Update sim step actually drives it. Port lives in
// code/Game/Entity/citizenbehavior.cpp (CitizenBehavior_Update).
//
// This is the town-manager tick: CountResources (re-tallies the townMgr+0x170
// per-category stock table from the storage-location list at +0x00 and the
// market list at +0x10 — the values the StatusBar "town status" dialog and the
// TownHallUI production/inventory tabs display), CheckThresholds (shortage
// notifications; fn_NotificationEvent 0x140095580 resolves NULL in standalone
// so those fire as guarded no-ops), the 300s path-distance-graph phase, and
// the 75s CollectStats history recorder that feeds the town-hall GRAPH tab
// series at townMgr+0x150.
//
// Before this registration, gameupdateloop.cpp's fn_CitizenList_Update
// (ResolveAddr 0x14008c2b0) was NULL and the whole tick was silently skipped —
// the +0x170 stock counters kept their ctor defaults, which is why the status
// dialog's resource counts never matched the actual barn/yard stores.
//
// Closure (all null-guarded in the port where unresolved):
//   registered: 0x14009f810 CalcPathCost (reg_pf_roots_route), 0x140087fb0
//     GetGridId (reg_pf_road), 0x14000e320 PathBST_FreeRecursive (reg_work_base),
//     0x140172234 = 300.0f path interval (reg_citizen_needsupdate),
//     type-ids c7e78 StorageComponent / c7cb8 RawMaterialDescription
//     (reg_economy_typeids / reg_action_child_ctors2), c7190 CitizenDescription
//     (added to reg_economy_typeids alongside this file).
//   NULL -> no-op: 0x140025a10 PathBST_Insert, 0x140025b30 PathPairs_Clear,
//     0x1400d62a0 ReassignWorkers, 0x1400d43b0 AssignCitizens,
//     0x140095580 NotificationEvent.
//
// rdata float const registered here (real retail value read with read_memory;
// not registered anywhere else):
//   0x1401721fc = 75.0f (CollectStats interval — without it the stats timer
//   never re-arms and the history recorder would run EVERY sim tick).
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

struct CitizenBehaviorRaw;
extern void CitizenBehavior_Update(CitizenBehaviorRaw* self, int64_t* deltaTime);  // 0x14008c2b0

// rdata float-const storage (filled with the real exe value).
static const float k_rdata_1721fc = 75.0f;

extern "C" void RegisterCitizenBehaviorTick() {
    RegisterExeAddr(0x14008c2b0, reinterpret_cast<uintptr_t>(&CitizenBehavior_Update));
    RegisterExeAddr(0x1401721fc, reinterpret_cast<uintptr_t>(&k_rdata_1721fc));
}
#endif // STANDALONE_EXE
