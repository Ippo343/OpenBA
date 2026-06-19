// standalone/reg_pf_roots_drop.cpp
// Wave 3 (Action) — DropOff / RemoveAnimal-ctor ROOT-caller registration.
//
// Registers the 4 root callers in code/Game/Action/action_pf_roots_drop.cpp. The
// integrator wires RegisterPfRootsDropBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Every address is a real function ENTRY (Ghidra Entry == address) and a faithful 1:1
// port. Closure: all exe sub-callees these reach are registered elsewhere or in earlier
// batches (notably 0x14009bd40 FindPlacementCell, ported by another agent THIS wave),
// plus the action ctors in action_pf_ctors (this wave). See the header comment in
// action_pf_roots_drop.cpp for the full sub-callee list.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern void     Pf_SpawnRawMaterialOnCell_78c0(int64_t, int64_t, int);        // 0x1400c78c0
extern void     Pf_SpillAllStorage_7570(int64_t);                             // 0x1400c7570
extern uint64_t Pf_DropOffExec_7620(int64_t, int64_t*, void*, uint64_t);      // 0x1400c7620
extern void     Pf_RemoveAnimalCtor_bbea0(void*, uint32_t, uint32_t*, uint32_t,
                                          void*, int64_t, uint32_t);          // 0x1400bbea0

void RegisterPfRootsDropBatch() {
    RegisterExeAddr(0x1400c78c0, reinterpret_cast<uintptr_t>(&Pf_SpawnRawMaterialOnCell_78c0));
    RegisterExeAddr(0x1400c7570, reinterpret_cast<uintptr_t>(&Pf_SpillAllStorage_7570));
    RegisterExeAddr(0x1400c7620, reinterpret_cast<uintptr_t>(&Pf_DropOffExec_7620));
    RegisterExeAddr(0x1400bbea0, reinterpret_cast<uintptr_t>(&Pf_RemoveAnimalCtor_bbea0));
}
#endif
