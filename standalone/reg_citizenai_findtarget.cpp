// standalone/reg_citizenai_findtarget.cpp
// Phase 3 (Keystone D) — CitizenAI find-best-target linchpin registration.
//
// Registers the two faithful 1:1 ports in code/Game/Component/citizenai_findtarget.cpp
// to their Ghidra exe addresses. The lead wires RegisterCitizenAIFindTarget() into
// RegisterCodeFunctions() in standalone/address_registry.cpp AFTER Ghidra-verification.
//
// Functions registered:
//   0x14008dd30  CitizenAI_FindBestTarget_dd30  (the main find-best-target helper)
//   0x1400178c0  CitizenAI_sub_78c0             (find MarketLocationComponent on an entity)
//
// NO rdata float/type-id globals are registered here: the type-id globals
// (DWORD_1401c4870 / DWORD_1401c7cb8 / DAT_1401c7ee8 / DAT_1401c7f20 / DAT_1401c4560)
// are resolved self-contained in the port via Object::GetTypeIdByName (the "Brick"
// pattern), and the port reads NO rdata float constants.
//
// Closure (all already registered — verified by grep of standalone/*.cpp):
//   0x140087fb0 Pf_EntityAnchorCell_7fb0      (reg_pf_road.cpp)
//   0x14009f810 Pf_IsDstReachable_f810        (reg_pf_roots_route.cpp)
//   0x14004edb0 Pf_PickReachableCell_edb0     (reg_pf_road.cpp)
//   0x14008ef40 CitizenComp_Entity_impl_ef40  (reg_component_gap_A.cpp)
//   0x14012d5d0 Pf_FindBestAdjacentCell_d5d0  (reg_pf_roots_route.cpp)
//   0x140132010 WorkImpl_Sqrtf_132010         (reg_work_impl2.cpp)
//   0x140006270 WorkBase_FindComponent_6270   (reg_work_base.cpp)
#include "resolve.h"
#include <cstdint>

extern int64_t* CitizenAI_FindBestTarget_dd30(int64_t*, int64_t*, uint32_t, uint32_t,
                                              int64_t*, float*, uint32_t);  // 0x14008dd30
extern void*    CitizenAI_sub_78c0(int64_t, void*);                          // 0x1400178c0

void RegisterCitizenAIFindTarget() {
    RegisterExeAddr(0x14008dd30, reinterpret_cast<uintptr_t>(&CitizenAI_FindBestTarget_dd30));
    RegisterExeAddr(0x1400178c0, reinterpret_cast<uintptr_t>(&CitizenAI_sub_78c0));
}
