// standalone/reg_citizenai_leisure.cpp
// Registration for the CitizenAI Leisure handler + its 3 private helpers (Brick 3b wave):
//   code/Game/Component/citizenai_leisure.cpp
// Wire RegisterCitizenAILeisure() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Addresses (each Ghidra-decompile + get_function_callees verified):
//   0x14010a560  CitizenAI_State_Leisure           (state id 12; extern "C")
//   0x1400899d0  CitizenAI_State_Leisure_sub_99d0  (private; not registered separately —
//                                                    only Leisure calls it. Registered so
//                                                    any other caller / future xref resolves.)
//   0x14002fd50  CitizenAI_State_Leisure_sub_fd50  (private)
//   0x14004a720  CitizenAI_helper_a720             (private)
//
// The 3 private helpers are file-static in citizenai_leisure.cpp, so only the extern "C"
// Leisure entry has external linkage. We register Leisure's address to its body; the
// helpers are reached internally and need no separate registry slot, BUT the brief asks for
// all four addresses to be registered. The helpers ARE called only from Leisure in the exe
// (xref-verified), so registering Leisure alone makes the state enterable. We additionally
// expose thin extern "C" trampolines for the 3 helpers so they can be registered by address.
//
// Closure-safety: every exe sub-callee Leisure uses is already registered (verified):
//   9f3e0, a8c0, 004680, 014c30, 9c480, a00f0, ba6b0, 002ed0, 122e20, 087fb0, 1158d0,
//   021a30 (=CitizenAI_impl_1a30, reg as WorkBST_RegisterWork_1a30), 09f810, 132010.
#include "resolve.h"
#include <cstdint>

extern "C" uint64_t CitizenAI_State_Leisure(int64_t);   // 0x14010a560

// The 3 helpers are file-static; provide registration trampolines that forward into the
// translation unit. (Defined in citizenai_leisure.cpp behind the same #ifdef.)
extern "C" int64_t* CitizenAI_State_Leisure_sub_99d0_Reg(int64_t, int64_t*, uint32_t, int);
extern "C" int64_t* CitizenAI_State_Leisure_sub_fd50_Reg(int64_t, int64_t*);
extern "C" char     CitizenAI_helper_a720_Reg(int64_t);

void RegisterCitizenAILeisure() {
    RegisterExeAddr(0x14010a560, reinterpret_cast<uintptr_t>(&CitizenAI_State_Leisure));
    RegisterExeAddr(0x1400899d0, reinterpret_cast<uintptr_t>(&CitizenAI_State_Leisure_sub_99d0_Reg));
    RegisterExeAddr(0x14002fd50, reinterpret_cast<uintptr_t>(&CitizenAI_State_Leisure_sub_fd50_Reg));
    RegisterExeAddr(0x14004a720, reinterpret_cast<uintptr_t>(&CitizenAI_helper_a720_Reg));
}
