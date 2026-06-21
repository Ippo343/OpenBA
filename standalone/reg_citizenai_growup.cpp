// standalone/reg_citizenai_growup.cpp
//
// Registration for the GrowUp + shared LongTermPlan state handlers + the private f230
// find-helper:  code/Game/Component/citizenai_growup.cpp
//
// Wire RegisterCitizenAIGrowUp() into RegisterCodeFunctions() in
// standalone/address_registry.cpp (the lead does this).
//
// VERIFIED addresses (decompile + callee re-verify this session):
//   0x14010c890  CitizenAI_State_GrowUp        (state id 4)
//   0x140115e60  CitizenAI_State_LongTermPlan  (state id 13, SHARED)
//   0x14008f230  Entity_impl_f230              (private GrowUp helper — registered so the
//                                               handler's ResolveAddr(0x14008f230) resolves)
//
// Closure-safety: ALL exe sub-callees are already registered:
//   0x140006270 (work_base) 0x14012cf10 (people QueryCount) 0x14008dd30 (findtarget)
//   0x140087fb0 (pf_road 7fb0) 0x1400bc790 (actionctors Ctor11) 0x14008d590 (reg elsewhere)
//   0x1400c1070 (actionctors Ctor4) 0x14012cd80 (reg elsewhere) 0x1400c4c50 (executor)
//   --- f230 closure ---
//   0x14009f3e0 (mapquery f3e0) 0x140002ed0 (work_base Position) 0x140122e20 (pf_road 2e20)
//   0x140008830 (work_base 8830) 0x1400a00f0 (work_base a00f0)
//
// Type-ids resolved self-contained via Object::GetTypeIdByName (the DAT_1401c7xxx globals are
// UNFILLED 0xFFFFFFFF in standalone): "RawMaterialDescription" (DWORD_1401c7cb8),
// "StorageComponent" (DWORD_1401c7e78), "WorkPlaceComponent" (DWORD_1401c80e0).
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern "C" uint64_t CitizenAI_State_GrowUp(int64_t);                                 // 0x14010c890
extern "C" void     CitizenAI_State_LongTermPlan(int64_t);                            // 0x140115e60
extern "C" void*    CitizenAI_Entity_impl_f230(int64_t, void*, uint32_t, float*);     // 0x14008f230

void RegisterCitizenAIGrowUp() {
    RegisterExeAddr(0x14010c890, reinterpret_cast<uintptr_t>(&CitizenAI_State_GrowUp));
    RegisterExeAddr(0x140115e60, reinterpret_cast<uintptr_t>(&CitizenAI_State_LongTermPlan));
    RegisterExeAddr(0x14008f230, reinterpret_cast<uintptr_t>(&CitizenAI_Entity_impl_f230));
}
#endif // STANDALONE_EXE
