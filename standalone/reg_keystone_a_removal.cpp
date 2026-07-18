// standalone/reg_keystone_a_removal.cpp
// Keystone A (RemovalTool::CalcPath cluster) registration. Maps the exe addresses
// to the faithful 1:1 ports in code/Game/Player/keystone_a_removalcalcpath.cpp.
// The integrator wires RegisterKeystoneARemovalBatch() into
// RegisterCodeFunctions() in standalone/address_registry.cpp.
//
// CLOSURE-SAFE: every exe sub-callee reached from these 7 ports is now covered.
// The 7 ports here are the ones whose closure was NOT previously registered;
// they call each other bottom-up plus the already-registered set:
//   ComponentEntity_CopyCtor 0x1400c3720, FUN_1400c48a0, FUN_1400c2ea0,
//   FUN_140003110, FUN_140003ea0, FUN_140005780, FUN_14011e710, FUN_14012ead0,
//   CitizenAI_impl_8e30 0x1400f8e30, CitizenAI_impl_a6e0 0x14001a6e0,
//   CitizenAI_State_Leisure_sub_a8c0 0x14001a8c0, FUN_14009f2f0 0x14009f2f0,
//   FUN_1400a8c90 0x1400a8c90, FUN_1400a6fe0 0x1400a6fe0,
//   StateMachine_Tick 0x1400a9660, TradeWork_impl_f370 0x1400af370,
//   PlayerPath_Sub_4670/4770/48a0 (0x140104670/770/8a0), plus the engine fn
//   pointers (fn_InternalBasePtr_Ctor/CheckPtr, fn_IsA, fn_IsDerivedFrom) and the
//   dispatch slots (static MemAlloc 0x140152b60, Time::Time 0x140152b00).
//
// NOT autostart-exercised (player RemovalTool path) — Ghidra-verified only.
#include "resolve.h"
#include <cstdint>

extern "C" void*    RemovalCalc_helper_d960(int64_t, void**);                 // 0x14002d960
extern "C" void*    RemovalCalc_helper_8d50(int64_t, void**, int);           // 0x1400f8d50
extern "C" void     RemovalCalc_helper_e4d0(int64_t);                         // 0x14011e4d0
extern "C" void     RemovalCalc_StateMachine_dfa0(int64_t);                   // 0x14011dfa0
extern "C" uint64_t RemovalCalc_helper_c700(uint32_t*, int);                  // 0x14009c700
extern "C" void     RemovalCalc_CitizenAI_9ad0(int64_t, int);                // 0x1400f9ad0
extern "C" uint64_t RemovalCalc_Slot13(int64_t, int, int);                   // 0x1401035e0

void RegisterKeystoneARemovalBatch() {
    // bottom-up: leaf helpers first
    RegisterExeAddr(0x14002d960, reinterpret_cast<uintptr_t>(&RemovalCalc_helper_d960));
    RegisterExeAddr(0x1400f8d50, reinterpret_cast<uintptr_t>(&RemovalCalc_helper_8d50));
    RegisterExeAddr(0x14011e4d0, reinterpret_cast<uintptr_t>(&RemovalCalc_helper_e4d0));
    RegisterExeAddr(0x14011dfa0, reinterpret_cast<uintptr_t>(&RemovalCalc_StateMachine_dfa0));
    RegisterExeAddr(0x14009c700, reinterpret_cast<uintptr_t>(&RemovalCalc_helper_c700));
    RegisterExeAddr(0x1400f9ad0, reinterpret_cast<uintptr_t>(&RemovalCalc_CitizenAI_9ad0));
    // the CalcPath itself (full closure now covered)
    RegisterExeAddr(0x1401035e0, reinterpret_cast<uintptr_t>(&RemovalCalc_Slot13));
}
