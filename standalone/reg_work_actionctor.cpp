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
// Pickup haul chain (2026-07-16). c0ad0 is the action-stack builder Pickup::Execute
// (0x1400f2f00) calls to make a citizen walk to a pile and haul it; c9b70 is the
// PickupRawMaterial action copy-ctor it pushes. Every other c0ad0 sub-callee was
// already registered by the Phase 2 action child-ctor waves.
extern void  WorkAct_PerformWorkCtor9_c0ad0(void* citizenIBP, uint32_t workId, void* targetIBP,
                                            float* worldPos, int cellIdx, uint8_t cancelFirst);     // 0x1400c0ad0
extern void* WorkAct_PickupRawMaterialCopyCtor_c9b70(void* self, void* p2, void* p3);               // 0x1400c9b70

// Pickup gate (code/Game/Work/pickup_gate.cpp) — d3c0 gates Pickup::Execute before
// c0ad0 is ever reached, so both must be registered for the pickup to complete.
extern uint64_t PickupGate_TownWants_8d3c0(int64_t townMgr, int64_t resEntity, int amount);          // 0x14008d3c0
extern uint64_t PickupGate_Accepts_12d990(int64_t slComp, int64_t resEntity);                        // 0x14012d990
extern int      PickupGate_AcceptMask_12d560(int64_t self);                                          // 0x14012d560

void RegisterWorkActionCtorBatch() {
    RegisterExeAddr(0x1400c1eb0, reinterpret_cast<uintptr_t>(&WorkAct_PerformWorkCtor_c1eb0));
    RegisterExeAddr(0x1400c2490, reinterpret_cast<uintptr_t>(&WorkAct_PlayAnimationCtor_c2490));
    RegisterExeAddr(0x1400bb0a0, reinterpret_cast<uintptr_t>(&WorkAct_GrazingWorkCtor_bb0a0));
    RegisterExeAddr(0x1400be6e0, reinterpret_cast<uintptr_t>(&WorkAct_PlantWorkCtor_be6e0));

    // --- pickup haul chain ---
    RegisterExeAddr(0x1400c0ad0, reinterpret_cast<uintptr_t>(&WorkAct_PerformWorkCtor9_c0ad0));
    RegisterExeAddr(0x1400c9b70, reinterpret_cast<uintptr_t>(&WorkAct_PickupRawMaterialCopyCtor_c9b70));
    RegisterExeAddr(0x14008d3c0, reinterpret_cast<uintptr_t>(&PickupGate_TownWants_8d3c0));
    RegisterExeAddr(0x14012d990, reinterpret_cast<uintptr_t>(&PickupGate_Accepts_12d990));
    RegisterExeAddr(0x14012d560, reinterpret_cast<uintptr_t>(&PickupGate_AcceptMask_12d560));
}
