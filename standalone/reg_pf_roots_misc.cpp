// standalone/reg_pf_roots_misc.cpp
// PF porter wave 3 — registration of the three Cluster-C root sub-helpers ported in
// code/Game/Action/action_pf_roots_misc.cpp.
//
// The integrator wires RegisterPfRootsMiscBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp. Each address is a real function ENTRY and a
// faithful 1:1 port whose sub-callee closure is satisfied (all callees registered:
// f3e0, 18dd0, 18880, 053ca0, 003110, 0c2ea0, plus Math/Volume engine exports).
//
// NOTE: 0x140092260 was previously listed as "deliberately not registered" in
// reg_action_move.cpp because of its ~9.5KB frame-exact walker stack. This wave ports
// it properly: a same-size local walker buffer (0x2438) driven by the registered
// 18dd0/18880, with disasm-verified record-field and result-read offsets. The stale
// note in reg_action_move.cpp can be dropped by the integrator.
#include "resolve.h"
#include <cstdint>

extern int      Pf_FindPlacementCell_bd40(int*, int, char, int64_t*, uint32_t);     // 0x14009bd40
extern int64_t* Pf_SpatialProximityQuery_92260(float*, int64_t*, float*, void*, int, void*); // 0x140092260
extern void     Pf_HerdStartMigration_122340(void*);                                // 0x140122340

void RegisterPfRootsMiscBatch() {
    RegisterExeAddr(0x14009bd40, reinterpret_cast<uintptr_t>(&Pf_FindPlacementCell_bd40));
    RegisterExeAddr(0x140092260, reinterpret_cast<uintptr_t>(&Pf_SpatialProximityQuery_92260));
    RegisterExeAddr(0x140122340, reinterpret_cast<uintptr_t>(&Pf_HerdStartMigration_122340));
}
