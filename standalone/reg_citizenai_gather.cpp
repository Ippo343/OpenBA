// standalone/reg_citizenai_gather.cpp
//
// Registers the CitizenAI resource-gathering state handlers + private helper ported in
// code/Game/Component/citizenai_gather.cpp into the standalone exe address registry so that
// ResolveAddr(Module::Exe, addr) returns our reimplementation:
//
//   0x14010bc90  CitizenAI_State_GatherResources   (state id 6)
//   0x14010b3e0  CitizenAI_State_GatherResources2  (state id 7)
//   0x14012ab00  Storage_GetResourceCount2         (private helper for Gather2)
//
// Called from address_registry.cpp (the lead wires the call site).

#ifdef STANDALONE_EXE

#include "resolve.h"
#include <cstdint>

extern "C" uint64_t CitizenAI_State_GatherResources(int64_t param_1);
extern "C" uint64_t CitizenAI_State_GatherResources2(int64_t param_1);
extern "C" int      Storage_GetResourceCount2(int64_t param_1);

void RegisterCitizenAIGather() {
    RegisterExeAddr(0x14010bc90, reinterpret_cast<uintptr_t>(&CitizenAI_State_GatherResources));
    RegisterExeAddr(0x14010b3e0, reinterpret_cast<uintptr_t>(&CitizenAI_State_GatherResources2));
    RegisterExeAddr(0x14012ab00, reinterpret_cast<uintptr_t>(&Storage_GetResourceCount2));
}

#endif // STANDALONE_EXE
