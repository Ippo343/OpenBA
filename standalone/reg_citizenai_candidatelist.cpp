// standalone/reg_citizenai_candidatelist.cpp
//
// Registers the 6 CitizenAIComponent candidate-state / need-check exe helper
// addresses to their clean C++ reimplementations (bodies live in
// code/Game/Component/citizenai_candidatelist.cpp).
//
// The integrator wires RegisterCitizenAICandidateList() into
// address_registry.cpp (extern decl + a call).
//
// NOTE: the sub-callees these functions invoke (0x140008090, 0x14012a9d0,
// 0x14012cf10, 0x140115be0) are registered ELSEWHERE (reg_component_people.cpp)
// and are intentionally NOT re-registered here. 0x14012ab00
// (Storage_GetResourceCount2) is registered by reg_citizenai_gather.cpp, and all
// 14 CitizenAI_State_* handlers are registered across the citizenai waves
// (reg_citizenai_handlers/handlers2/handlers3/gather/growup/leisure/housing_idle)
// — ValidateAndEnterState dispatches into real reimpls. (An earlier revision of
// this note predated those waves; audited faithful vs Ghidra 2026-07-19.)
#include "resolve.h"
#include <cstdint>

// --- reimpl bodies (non-static, defined in citizenai_candidatelist.cpp) ---
extern void CitizenAI_SortByPriority(void* base, int count);
extern int  CitizenComp_HasSufficientResources(int64_t self);
extern int  CitizenComp_HasSufficientResources2(int64_t self);
extern int  CitizenComp_GetHappiness(int64_t self);
extern void CitizenAI_BuildCandidateStateList(void* self, int64_t* outList, char param3);
extern int  CitizenAI_ValidateAndEnterState(void* self, int state);

void RegisterCitizenAICandidateList() {
    RegisterExeAddr(0x14002fe10, reinterpret_cast<uintptr_t>(&CitizenAI_SortByPriority));
    RegisterExeAddr(0x1401156e0, reinterpret_cast<uintptr_t>(&CitizenComp_HasSufficientResources));
    RegisterExeAddr(0x1401157f0, reinterpret_cast<uintptr_t>(&CitizenComp_HasSufficientResources2));
    RegisterExeAddr(0x140115de0, reinterpret_cast<uintptr_t>(&CitizenComp_GetHappiness));
    RegisterExeAddr(0x14010dca0, reinterpret_cast<uintptr_t>(&CitizenAI_BuildCandidateStateList));
    RegisterExeAddr(0x14010f1f0, reinterpret_cast<uintptr_t>(&CitizenAI_ValidateAndEnterState));
}
