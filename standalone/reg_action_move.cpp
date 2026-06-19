// standalone/reg_action_move.cpp
// Wave 3 (Action) — MoveTo / PerformWork / PlayAnimation sub-helper registration.
//
// Registers the exe addresses of shared StateMachine-action exe sub-helpers
// (resolved from code/Game/Action/{moveto,performwork,playanimation,huntanimal}.cpp
// call sites) to their faithful reimplementations in
// code/Game/Action/action_base_move.cpp. The integrator wires RegisterActionMoveBatch()
// into RegisterCodeFunctions() in standalone/address_registry.cpp.
//
// Safety: every address below was verified as a real function ENTRY in Ghidra
// (get_function_by_address Entry == address) and is a faithful 1:1 port.
//
// DELIBERATELY NOT registered:
//   0x140092260 — FUN_140092260 spatial/bridge octree query. Frame-exact stack layout
//                 (~9.5KB local node stack from FUN_140018dd0) is not safely portable;
//                 a half-faithful body would mis-select the nearest entity. Leaving it
//                 unregistered preserves current behavior (silently skipped) — no
//                 regression. See note in action_base_move.cpp.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// FindComponent (member-style: RCX=entity, RDX=retIBP, returns retIBP)
extern void* ActMv_FindComponentFamily_12d0(uintptr_t, void*);          // 0x1400012d0

// ExternalPtr serialize — void fn(void* extPtrSlot, void* serializer)
extern void  ActMv_ExternalPtrSerialize_22970(void*, void*);            // 0x140022970

// Facing / animation setters
extern void  ActMv_SetFacingDirection_a060(void*, void*);               // 0x14010a060
extern void  ActMv_UpdateFacing_c8d20(void*);                           // 0x1400c8d20
extern void  ActMv_SetAnimation_a1f0(void*, uint32_t, float, float);    // 0x14010a1f0

// Citizen break / task-due check — uint8_t fn(void* entity)
extern uint8_t ActMv_CitizenBreakCheck_116650(void*);                   // 0x140116650

// Pathfinding helpers
extern void  ActMv_SetTerrainCosts_e550(void*, void*, int, void*);      // 0x14009e550
extern int   ActMv_CopyPathFromFinder_a6d50(void*, void*, void*, int);  // 0x1400a6d50
extern void* ActMv_PathfinderQuery_9cfb0(void*, void*, uint64_t, uint32_t, uint32_t); // 0x14009cfb0
extern void* ActMv_PathSetup_c8c10(void*, void*, void*, void*);         // 0x1400c8c10

void RegisterActionMoveBatch() {
    // CLOSURE-SAFE subset (lead Ghidra-verified faithful + full closure satisfied):
    RegisterExeAddr(0x1400012d0, reinterpret_cast<uintptr_t>(&ActMv_FindComponentFamily_12d0));   // engine funcs only
    RegisterExeAddr(0x14010a060, reinterpret_cast<uintptr_t>(&ActMv_SetFacingDirection_a060));    // Vec3 Normalize+Heading slots
    RegisterExeAddr(0x1400c8d20, reinterpret_cast<uintptr_t>(&ActMv_UpdateFacing_c8d20));         // Vec slots + 0x14009f3e0(reg)
    RegisterExeAddr(0x1400a6d50, reinterpret_cast<uintptr_t>(&ActMv_CopyPathFromFinder_a6d50));   // dep 0x1400021d0(reg)
    // DEFERRED (bodies present, NOT registered — unsatisfied closure):
    //  0x14010a1f0 SetAnimation     — calls unreg 0x1401250f0, 0x1400a9290
    //  0x140116650 CitizenBreakCheck — calls unreg 0x1400a9290 (TimeCheck)
    //  0x14009e550 SetTerrainCosts   — calls unreg 0x1400a68b0/a6a30
    //  0x14009cfb0 PathfinderQuery   — calls unreg 0x14009c480/a6b20/a6c10
    //  0x1400c8c10 PathSetup         — calls unsafe siblings e550+9cfb0
    //  0x140022970 ExternalPtrSerialize — heavy External/Filename/String runtime closure (drift risk)
    //  0x140092260 — not registered (see header note: ~9.5KB local node stack, not safely portable).
}
#endif
