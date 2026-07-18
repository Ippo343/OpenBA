// standalone/reg_citizenai_housing_idle.cpp
//
// CitizenAI FindHousing + Idle handler closure — registers the two state handlers and their
// entire shared deep AVL/BST helper subtree, ported in
// code/Game/Component/citizenai_housing_idle.cpp.
//
// CLOSURE (verified self-closing): every exe sub-callee reached by this subtree is either
//   registered in THIS batch (the helper bodies) or already registered by an earlier wave and
//   called via ResolveAddr:
//     0x140115be0 CitizenComponent_CanFindHousing  (reg_component_gap_A.cpp)
//     0x14000e320 WorkBase_TreeNodeFree_e320        (reg_work_base.cpp)
//     0x14009f810 BuildStructure_impl_f810          (reg_pf_roots_route.cpp)
//     0x140132010 WorkImpl_Sqrtf_132010             (reg_work_impl2.cpp)
//     0x14000e450 Pf_AvlRotateB_e450                (reg_pf_avl.cpp)  [called by Helper_e360]
//     0x14000e580 Pf_AvlRotateB_e580                (reg_pf_avl.cpp)  [called by Helper_e360]
//   The only otherwise-unregistered callee discovered in the closure was
//   0x14000e360 (TradeDescription_impl_e360, AVL rebalance walker) — it is PORTED in this unit
//   and registered below.
//
// The integrator wires RegisterCitizenAIHousingIdle() into RegisterCodeFunctions() in
// address_registry.cpp.

#include "resolve.h"
#include <cstdint>

// Handlers (extern "C").
extern "C" uint64_t CitizenAI_State_FindHousing(int64_t);   // 0x14010b340
extern "C" uint64_t CitizenAI_State_Idle(int64_t);          // 0x14010b2d0

// The helper subtree lives in an anonymous namespace in the .cpp, so it is NOT registered by
// symbol from here. Registration of the helper addresses uses thin extern "C" trampolines is
// unnecessary: the helpers are only ever reached internally (the handlers call them directly via
// normal C++ calls, NOT through ResolveAddr). However the exe sub-callees that ARE reached via
// ResolveAddr from anywhere else must be registered so any cross-TU call resolves to our port.
//
// To register the in-file helper addresses (so any *other* TU that ResolveAddr's them gets our
// faithful port rather than a null), we expose extern "C" thunks from the .cpp. They are declared
// here and defined at the bottom of citizenai_housing_idle.cpp.
extern "C" uint64_t CitizenAI_Reg_helper_7550(int64_t, void**);
extern "C" void     CitizenAI_Reg_helper_7830(int64_t, void**);
extern "C" uint64_t CitizenAI_Reg_helper_79b0(int64_t, void**, void**, int64_t, char, char);
extern "C" uint8_t  CitizenAI_Reg_helper_7b60(int64_t, void**, int64_t*);
extern "C" void     CitizenAI_Reg_helper_7c90(void*, void*, void*, int64_t*, int64_t*);
extern "C" void     CitizenAI_Reg_helper_8000(void*, void*, int64_t*, int64_t*);
extern "C" void*    CitizenAI_Reg_helper_3f010(void*, uint64_t, uint64_t);
extern "C" void     CitizenAI_Reg_helper_e360(int64_t*, int64_t*);

void RegisterCitizenAIHousingIdle() {
    RegisterExeAddr(0x14010b340, reinterpret_cast<uintptr_t>(&CitizenAI_State_FindHousing));
    RegisterExeAddr(0x14010b2d0, reinterpret_cast<uintptr_t>(&CitizenAI_State_Idle));
    RegisterExeAddr(0x1400f7550, reinterpret_cast<uintptr_t>(&CitizenAI_Reg_helper_7550));
    RegisterExeAddr(0x1400f7830, reinterpret_cast<uintptr_t>(&CitizenAI_Reg_helper_7830));
    RegisterExeAddr(0x1400f79b0, reinterpret_cast<uintptr_t>(&CitizenAI_Reg_helper_79b0));
    RegisterExeAddr(0x1400f7b60, reinterpret_cast<uintptr_t>(&CitizenAI_Reg_helper_7b60));
    RegisterExeAddr(0x1400f7c90, reinterpret_cast<uintptr_t>(&CitizenAI_Reg_helper_7c90));
    RegisterExeAddr(0x1400f8000, reinterpret_cast<uintptr_t>(&CitizenAI_Reg_helper_8000));
    RegisterExeAddr(0x14003f010, reinterpret_cast<uintptr_t>(&CitizenAI_Reg_helper_3f010));
    RegisterExeAddr(0x14000e360, reinterpret_cast<uintptr_t>(&CitizenAI_Reg_helper_e360));
}
