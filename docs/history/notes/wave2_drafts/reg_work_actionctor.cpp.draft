// standalone/reg_work_actionctor.cpp
// Wave 2 (Work) — action/animation constructor sub-batch registration.
//
// Registers the exe addresses of the 4 Work action/animation constructors
// (resolved from cleararea / foresterwork / herbalistwork / pasturework /
// tendarea / plantorchard / fishingwork call sites) to their reimplementations
// in code/Game/Work/work_base_actionctor.cpp. The integrator wires
// RegisterWorkActionCtorBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Safety: every address below was verified as a real function ENTRY in Ghidra
// (get_function_by_address Entry == address). These are instruction-faithful
// ports; the exe sub-callees they invoke are mirrored via guarded ResolveAddr
// inside the reimpl (NOT registered here).
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// work_base_actionctor.cpp
extern void WorkAct_PerformWorkCtor_c1eb0(void* citizenIBP, uint32_t workId, void* targetIBP,
                                          float* worldPos, int zero, uint8_t zeroByte);            // 0x1400c1eb0
extern void WorkAct_PlayAnimationCtor_c2490(void* citizenIBP, uint32_t priority, uint32_t citizenIndex); // 0x1400c2490
extern void WorkAct_GrazingWorkCtor_bb0a0(void* citizenIBP, uint32_t priority, void* workEntityIBP,
                                          uint32_t unused4, float* targetPos, float* srcPos,
                                          uint32_t typeId, uint32_t flag8, uint32_t cellIdx9);      // 0x1400bb0a0
extern void WorkAct_PlantWorkCtor_be6e0(void* citizenIBP, uint32_t priority, void* workEntityInner,
                                        uint32_t unused4, float* position, uint32_t createParam6,
                                        uint32_t completeParam7, int64_t* condPtr8, uint32_t* msgIdPtr9,
                                        uint64_t unused10, uint32_t smParam11);                     // 0x1400be6e0

void RegisterWorkActionCtorBatch() {
    RegisterExeAddr(0x1400c1eb0, reinterpret_cast<uintptr_t>(&WorkAct_PerformWorkCtor_c1eb0));
    RegisterExeAddr(0x1400c2490, reinterpret_cast<uintptr_t>(&WorkAct_PlayAnimationCtor_c2490));
    RegisterExeAddr(0x1400bb0a0, reinterpret_cast<uintptr_t>(&WorkAct_GrazingWorkCtor_bb0a0));
    RegisterExeAddr(0x1400be6e0, reinterpret_cast<uintptr_t>(&WorkAct_PlantWorkCtor_be6e0));
}
#endif
