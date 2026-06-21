// standalone/reg_citizenai_findhelpers.cpp
// Phase 3 (Keystone D) — Brick 3b deep find-helpers registration.
//
// Registers the five faithful 1:1 ports in code/Game/Component/citizenai_findhelpers.cpp.
// These unblock the State_Work / State_AdultTransition / State_ElderTransition handler trio
// (12ac30 blocks BOTH transitions). The lead wires RegisterCitizenAIFindHelpers() into
// RegisterCodeFunctions() AFTER Ghidra-verification.
//
// Functions registered:
//   0x14012ac30  ResidenceCheck_2ac30        (residence capacity / threshold check; bool)
//   0x14010db20  CitizenAI_helper_db20       (State_Work workplace-still-valid)
//   0x1400170b0  CitizenAI_helper_70b0       (occupancy-sum leaf)
//   0x14008ec70  Entity_impl_ec70            (ElderTransition find-target)
//   0x14008e370  CitizenAI_helper_e370       (AdultTransition find-best-target, 4 phases)
//
// NO rdata float/type-id globals registered here: the six type-id globals
// (c7d28/c7900/c7cb8/c7ee8/c7f20/c7cf0) are resolved self-contained in the port via
// Object::GetTypeIdByName (the "Brick" pattern); the ports read no rdata float constants.
//
// Closure (all already registered — verified by grep of standalone/*.cpp):
//   0x1401158d0(gap_A) 0x14009f810(pf_roots_route) 0x140008090(people) 0x140006270(work_base)
//   0x140122e20(pf_road) 0x1400a00f0(work_base) 0x140132010(work_impl2) 0x140087fb0(pf_road)
//   0x14008ef40(gap_A) 0x14004edb0(pf_road) 0x140008830(work_base) 0x14009c480(pf_roots_road)
//   0x14012d5d0(pf_roots_route) 0x1400178c0(citizenai_findtarget)
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern bool      ResidenceCheck_2ac30(int64_t);                                          // 0x14012ac30
extern uint64_t  CitizenAI_helper_db20(int64_t);                                         // 0x14010db20
extern uint64_t  CitizenAI_helper_70b0(int64_t);                                         // 0x1400170b0
extern int64_t*  Entity_impl_ec70(int64_t, int64_t*, uint32_t, float*);                  // 0x14008ec70
extern int64_t*  CitizenAI_helper_e370(int64_t*, int64_t*, uint32_t, int64_t*, float*,
                                       uint64_t, int64_t*);                              // 0x14008e370

void RegisterCitizenAIFindHelpers() {
    RegisterExeAddr(0x14012ac30, reinterpret_cast<uintptr_t>(&ResidenceCheck_2ac30));
    RegisterExeAddr(0x14010db20, reinterpret_cast<uintptr_t>(&CitizenAI_helper_db20));
    RegisterExeAddr(0x1400170b0, reinterpret_cast<uintptr_t>(&CitizenAI_helper_70b0));
    RegisterExeAddr(0x14008ec70, reinterpret_cast<uintptr_t>(&Entity_impl_ec70));
    RegisterExeAddr(0x14008e370, reinterpret_cast<uintptr_t>(&CitizenAI_helper_e370));
}
#endif
