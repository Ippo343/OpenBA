// standalone/reg_citizenai_handlers3.cpp
// Phase 3 (Keystone D) — Brick 3b handler wave 3 registration.
//
// Registers the three handler ports in code/Game/Component/citizenai_handlers3.cpp.
// ValidateAndEnterState (citizenai_candidatelist.cpp) dispatches them by ResolveAddr,
// so registering these addresses makes the states ENTERABLE.
//
//   0x14010d790  CitizenAI_State_Work            (state id 5)
//   0x14010cd80  CitizenAI_State_AdultTransition  (state id 0/14)
//   0x14010d2c0  CitizenAI_State_ElderTransition  (state id 1/15)
//
// Closure: the deep find-helpers (12ac30/e370/ec70/db20/70b0) landed in the prior brick
// (reg_citizenai_findhelpers.cpp); every other sub-callee was already registered (see the
// citizenai_handlers3.cpp header for the full list). Type-ids (InteractDescription /
// CitizenDescription) resolved self-contained via Object::GetTypeIdByName.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern "C" uint64_t CitizenAI_State_Work(int64_t);             // 0x14010d790
extern "C" uint64_t CitizenAI_State_AdultTransition(int64_t);  // 0x14010cd80
extern "C" uint64_t CitizenAI_State_ElderTransition(int64_t);  // 0x14010d2c0

void RegisterCitizenAIHandlers3() {
    RegisterExeAddr(0x14010d790, reinterpret_cast<uintptr_t>(&CitizenAI_State_Work));
    RegisterExeAddr(0x14010cd80, reinterpret_cast<uintptr_t>(&CitizenAI_State_AdultTransition));
    RegisterExeAddr(0x14010d2c0, reinterpret_cast<uintptr_t>(&CitizenAI_State_ElderTransition));
}
#endif
