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
#include <map>

// Backing storage for the .rdata dispatch slots below. RegisterExeAddr maps the
// Ghidra address of the SLOT to the address of a word holding the function
// pointer, which is what code/Game/Goal/timegoalinstance.cpp reads.
static std::map<uintptr_t, uintptr_t>& GoalSlotStorage() {
    static std::map<uintptr_t, uintptr_t> s;
    return s;
}
static void RegisterGoalSlot(uintptr_t ghidra_addr, uintptr_t fn_ptr) {
    auto& slots = GoalSlotStorage();
    slots[ghidra_addr] = fn_ptr;
    RegisterExeAddr(ghidra_addr, reinterpret_cast<uintptr_t>(&slots[ghidra_addr]));
}

void RegisterGoalBatch() {
    RegisterExeAddr(0x1400265b0, reinterpret_cast<uintptr_t>(&StatGoal_ResolveExternalPtr));
    RegisterExeAddr(0x1400da4e0, reinterpret_cast<uintptr_t>(&StatGoal_StatValueDispatch));
    RegisterExeAddr(0x14001b340, reinterpret_cast<uintptr_t>(&Goal_RefArray_Resize));

    // --- GoalInstance dispatch slots (all three were UNREGISTERED) -----------
    //
    // 0x140152868 Achievement::GoalInstance::Persist is the one that bites, and
    // it is a SERIALIZER. TimeGoalInstance::Slot3 (our port of 0x1400dad30) is
    // byte-faithful to retail except that it calls the base through
    // `if (s_fn_GoalInstancePersist)`. With the slot unregistered that pointer
    // was null, so the base call was silently skipped -- while retail always
    // makes it. GoalInstance::Persist (Runtime 0x18009ef70) reads an ExternalPtr
    // (the parent Goal, +0x18) plus a uint goal-state (+0x10), so every goal
    // instance consumed FEWER bytes on load than retail wrote, desynchronising
    // the stream from that object onward.
    //
    // That is why loading the packaged TUTORIAL save died in
    // InternalFactory::PersistObject: PersistObject constructs an object for an
    // unseen id and calls obj->vt[0x18](stream) so it deserializes itself INLINE,
    // and the object right after the first TimeGoalInstance was therefore read
    // off misaligned bytes, yielding a garbage objId that indexed the id map out
    // of bounds. Our own saves never caught it because our SAVE leg skipped the
    // same bytes -- self-consistent, but not retail-compatible. A retail-authored
    // save is the only test that could see it.
    RegisterGoalSlot(0x140152868, reinterpret_cast<uintptr_t>(
        ResolveExport("?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z")));

    // The other two are read by TimeGoalInstance::Slot5 (Update) behind the same
    // kind of null guard, so with them unregistered a time goal never advanced
    // and never consulted the other goals. Not stream-affecting, but the tutorial
    // is goal-driven, so wire them in the same pass.
    RegisterGoalSlot(0x140152878, reinterpret_cast<uintptr_t>(ResolveExport(
        "?AreGoalsMetExcept@GoalList@Achievement@@QEBA_NV?$InternalPtr@VGoalInstance@Achievement@@@Object@@@Z")));
    RegisterGoalSlot(0x140152880, reinterpret_cast<uintptr_t>(ResolveExport(
        "?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z")));
}

