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
// (0x140092260 spatial/octree query — formerly "deliberately not registered" — IS now
//  ported + registered in the pathfinding wave: action_pf_roots_misc.cpp via a same-size
//  local quadtree-walker buffer (alignas(16) uint8_t[0x2438]) calling the registered
//  18dd0/18880, with disasm-verified record offsets. Registered in RegisterPfRootsMiscBatch.)
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

// Closure-safe code sub-callees (action_base_movedeps.cpp) — registering these unblocks
// the two closure-safe members of the move-unsafe-6 set.
extern bool ActMvDep_TimeCheck_a9290(void*, float*, void*);             // 0x1400a9290
extern void ActMvDep_AnimHelper_50f0(uintptr_t, uint32_t, float, float, float); // 0x1401250f0

// MoveTo execution .rdata float constant: minimum arrival distance (squared), 0.05f.
// Read MoveTo_AbstractImpl_Impl @0x1401720d4 — `if (dist < *s_fMinDistSq) return 0;`.
static const float k_rdata_1720d4_MinDist = 0.05f;

void RegisterActionMoveBatch() {
    RegisterExeAddr(0x1401720d4, reinterpret_cast<uintptr_t>(&k_rdata_1720d4_MinDist));   // MoveTo arrival dist
    // CLOSURE-SAFE subset (lead Ghidra-verified faithful + full closure satisfied):
    RegisterExeAddr(0x1400012d0, reinterpret_cast<uintptr_t>(&ActMv_FindComponentFamily_12d0));   // engine funcs only
    RegisterExeAddr(0x14010a060, reinterpret_cast<uintptr_t>(&ActMv_SetFacingDirection_a060));    // Vec3 Normalize+Heading slots
    RegisterExeAddr(0x1400c8d20, reinterpret_cast<uintptr_t>(&ActMv_UpdateFacing_c8d20));         // Vec slots + 0x14009f3e0(reg)
    RegisterExeAddr(0x1400a6d50, reinterpret_cast<uintptr_t>(&ActMv_CopyPathFromFinder_a6d50));   // dep 0x1400021d0(reg)

    // Sub-callee closure (action_base_movedeps.cpp) — register BEFORE the helpers below
    // so they resolve at first call:
    //  - 0x1400a9290 TimeCheck: engine_funcs AsSeconds/RandFloat exports + DAT constants only.
    //  - 0x1401250f0 AnimHelper: IsDerivedFrom/CheckPtr (engine_funcs) + Time::Time/PlayAnimation
    //    (Runtime exports) + FUN_1400c3470 (registered in reg_action_hunt). All satisfied.
    RegisterExeAddr(0x1400a9290, reinterpret_cast<uintptr_t>(&ActMvDep_TimeCheck_a9290));
    RegisterExeAddr(0x1401250f0, reinterpret_cast<uintptr_t>(&ActMvDep_AnimHelper_50f0));

    // NOW closure-safe (their sub-callees registered above):
    RegisterExeAddr(0x14010a1f0, reinterpret_cast<uintptr_t>(&ActMv_SetAnimation_a1f0));          // -> 1250f0 + a9290
    RegisterExeAddr(0x140116650, reinterpret_cast<uintptr_t>(&ActMv_CitizenBreakCheck_116650));   // -> a9290 (8-byte pair fix)

    // ExternalPtrSerialize: faithful body exists; its closure is engine_funcs fn_* + dispatch
    // slots ONLY (no unregistered exe-code child — map confirms 22970 -> {}). Only hit on
    // action save/load (not the build/sim path), so the FindOrLoad concern is the terminating
    // single-target re-resolve, not the non-terminating preload. Safe to register.
    RegisterExeAddr(0x140022970, reinterpret_cast<uintptr_t>(&ActMv_ExternalPtrSerialize_22970));

    // The three existing pathfinder-caller bodies are NOW closure-safe (the pathfinding wave
    // registered their enqueuer/A* closure: a6a30/a68b0/a6b20/a6c10 enqueuers + a6430 + A*,
    // and 9c480 ZoneRefresh -> 9c170 flood-fill). e550 -> {a6a30,a68b0}; 9cfb0 -> {9c480,
    // a6b20,a6c10}; c8c10 -> {e550,9cfb0}. All registered, so wire the bodies.
    RegisterExeAddr(0x14009e550, reinterpret_cast<uintptr_t>(&ActMv_SetTerrainCosts_e550));
    RegisterExeAddr(0x14009cfb0, reinterpret_cast<uintptr_t>(&ActMv_PathfinderQuery_9cfb0));
    RegisterExeAddr(0x1400c8c10, reinterpret_cast<uintptr_t>(&ActMv_PathSetup_c8c10));

    // (Formerly-deferred e550/9cfb0/c8c10 are now wired above — the pathfinding wave landed
    //  the a6430 dispatcher + A* core + the enqueuer/flood-fill closure they depend on.)
}
