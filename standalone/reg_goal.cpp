// standalone/reg_goal.cpp
// Goal-module registration wave — closes the Goal code-gap (2 addrs).
//
//   0x1400265b0  StatGoal lazy ExternalPtr resolver. Closure = ExternalFactory
//                singleton + ExternalFactory::Find + Filename ctor/dtor +
//                IsDerivedFrom — all engine_funcs (ResolveExport). Identical
//                shape to engine_funcs.h ResolveExternalByName_Impl. The
//                expected-type global (0x140152870) is unregistered in
//                standalone → type-check skipped, Find result trusted (same
//                doctrine as the BuildComponent resolver). No exe sub-callees.
//   0x1400da4e0  StatGoal StatValueDispatch — 30-case switch over GameWorld
//                stat queries. Full closure (dispatcher + all ~14 sub-callees
//                incl. AVL/MemAlloc scratch helpers) ported in-file in
//                code/Game/Goal/statvaluedispatch.cpp; verified closure-CLEAN
//                (zero unregistered exe-internal calls). All type-id globals it
//                reads are out-of-gap-range data, null-guarded (conservative
//                no-match) — read-only, never corrupts.
//
//   0x14001b340  ExternalPtr-triple array resize (stride 0x18), the LOAD-leg
//                resize for BuildingGoal::Serialize (0x1400d76a0) and
//                MapGenerator::Serialize (0x1400a2980). Closure = MemAlloc /
//                MemFree / String copy-ctor / String dtor — all engine_funcs.
//                Same primitive as EventArray_Grow (eventlog.cpp) at a 0x18
//                stride. Seeds new entries with generation -1, which is what
//                keeps the lazy resolver from mistaking them for cached.
//
// SAFETY: all three addresses verified as true function ENTRYs in Ghidra.
#include "resolve.h"
#include "Goal/goal_exe_helpers.h"
#include "Goal/statvaluedispatch.h"
#include <cstdint>

void RegisterGoalBatch() {
    RegisterExeAddr(0x1400265b0, reinterpret_cast<uintptr_t>(&StatGoal_ResolveExternalPtr));
    RegisterExeAddr(0x1400da4e0, reinterpret_cast<uintptr_t>(&StatGoal_StatValueDispatch));
    RegisterExeAddr(0x14001b340, reinterpret_cast<uintptr_t>(&Goal_RefArray_Resize));
}

