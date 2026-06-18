// standalone/reg_work_grid.cpp
// Wave 2 (Work) — grid / work-assignment helpers registration.
//
// Hand-ported + Ghidra-verified (bodies in code/Game/Work/work_base_grid.cpp). Integrator
// wires RegisterWorkGridBatch() into RegisterCodeFunctions() in standalone/address_registry.cpp.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern int      WorkGrid_FindCell_9ccf0(int*, int, int, uint32_t);          // 0x14009ccf0
extern uint64_t WorkGrid_TryAssignWorkplace_f8620(uintptr_t, void*, int, char); // 0x1400f8620
extern int      WorkGrid_AutoPickup_e3db0(uintptr_t, void*);                // 0x1400e3db0

void RegisterWorkGridBatch() {
    RegisterExeAddr(0x14009ccf0, reinterpret_cast<uintptr_t>(&WorkGrid_FindCell_9ccf0));
    RegisterExeAddr(0x1400f8620, reinterpret_cast<uintptr_t>(&WorkGrid_TryAssignWorkplace_f8620));
    RegisterExeAddr(0x1400e3db0, reinterpret_cast<uintptr_t>(&WorkGrid_AutoPickup_e3db0));
}
#endif
