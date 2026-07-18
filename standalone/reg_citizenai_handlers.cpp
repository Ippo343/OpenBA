// standalone/reg_citizenai_handlers.cpp
//
// Registers the CitizenAIComponent state HANDLERS (Brick 3b) to their clean C++
// reimplementations (bodies in code/Game/Component/citizenai_handlers.cpp), plus
// the action-builder + the exe .rdata float constants those handlers read.
//
// Once a handler address is registered, CitizenAI_ValidateAndEnterState's
// ResolveAddr dispatch (citizenai_candidatelist.cpp) calls it instead of skipping,
// so the corresponding state becomes ENTERABLE.
//
// Wave 1 (this file): Wander (0x14010a2b0) + its builder PlayAnimation_Action_Ctor5
// (0x1400c26c0). All other sub-callees (c5400/c54e0/cb1d0/12d0/c9170/9ccf0/9f3e0)
// are registered elsewhere; DAT_140172114 (0.5) is registered in reg_work_tail_fire.
//
// The integrator wires RegisterCitizenAIHandlers() into address_registry.cpp.
#include "resolve.h"
#include <cstdint>

// --- reimpl bodies (defined in citizenai_handlers.cpp) ---
extern void CitizenAI_PlayAnimationActionCtor5(void* selfIBP, float* dest, float animTime);
extern bool CitizenAI_State_Wander(int64_t param_1);

// --- exe .rdata float constants (values read directly from Ghidra) ---
static const float k_rdata_172180 =  3.0f;   // anim-time base
static const float k_rdata_1721a0 =  7.0f;   // anim-time scale
static const float k_rdata_172280 = -1.0f;   // MoveTo speed sentinel (-> randomized default)

void RegisterCitizenAIHandlers() {
    RegisterExeAddr(0x1400c26c0, reinterpret_cast<uintptr_t>(&CitizenAI_PlayAnimationActionCtor5));
    RegisterExeAddr(0x14010a2b0, reinterpret_cast<uintptr_t>(&CitizenAI_State_Wander));
    RegisterExeAddr(0x140172180, reinterpret_cast<uintptr_t>(&k_rdata_172180));
    RegisterExeAddr(0x1401721a0, reinterpret_cast<uintptr_t>(&k_rdata_1721a0));
    RegisterExeAddr(0x140172280, reinterpret_cast<uintptr_t>(&k_rdata_172280));
}
