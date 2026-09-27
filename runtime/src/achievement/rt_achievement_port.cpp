// ============================================================================
// runtime/src/achievement/rt_achievement_port.cpp — RtAchievementPortInit()
// Wave 5 lane group A/B: the Achievement namespace
// ============================================================================
// One entry point for the Achievement namespace. Same shape as
// RtIoPortInit() (Wave 4): each lane owns exactly ONE source file and exposes
// exactly ONE Register function, so no two lanes ever edit the same file.
//
// Switches, outermost first:
//   RT_ACHIEVEMENT=0   leave the whole namespace on the DLL
//   RT_GOAL=0          ... just Goal / GoalInstance / GoalList (lane A)
//   RT_ACHMGR=0        ... just Achievement / AchievementInstance /
//                                   AchievementManager                  (lane B)
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtAchGoalRegister();       // goal.cpp                (lane A)
extern int RtAchManagerRegister();    // achievement_manager.cpp (lane B)
// Wave 9 lane P — goal_serialize.cpp. Gated by RT_GOAL with goal.cpp,
// not by a switch of its own: class atomicity (PLAN §4.1 decision 5) means the
// two stream members must flip together with the ctors/dtors that build the
// objects they serialize.
// Wave 23 lane ACHIEVE. Both gated by RT_ACHMGR with
// achievement_manager.cpp: achievement_state.cpp ports five more members of the
// SAME three classes (the manager dtor, RemoveAll, ReadAchievements,
// SetAchievementMet and Achievement::CreateInstance) and
// achievement_serialize.cpp the two stream members Wave 5 held back. Class
// atomicity (PLAN �4.1 decision 5) puts them on one switch.
extern int RtAchStateRegister();          // achievement_state.cpp     (Wave 23)
extern int RtAchSerializeRegister();      // achievement_serialize.cpp (Wave 23)
// Wave 27 lane ACHIEVE — achievement_typeinfo_accessors.cpp. ONE file, TWO
// register functions, because its eleven slot bodies span all four Achievement
// vtable rows and those rows sit on DIFFERENT kill switches. Splitting the
// registration rather than the file is what keeps class atomicity (PLAN §4.1
// decision 5) while the eleven bodies — which are the same two-instruction
// shape — stay together where they can be read side by side.
extern int RtAchManagerTypeInfoAccessorsRegister();  // Achievement + …Instance
// Wave 27 lane ACHIEVE — achievement_vtable.cpp (the four non-accessor slots).
extern int RtAchVtableRegister();
// Wave 27 lane ACHIEVE — goal_list.cpp (GoalList::Initialize, a BODY gap row).
extern int RtAchGoalListRegister();
// NO ??0AchievementManager@Achievement@@ HERE, DELIBERATELY. Wave 27 lane
// ACHIEVE wrote one and DELETED it: the ctor (retail 0x18009eb00) was ALREADY
// ported and registered through both doors in
// runtime/src/engine/engine_lifecycle.cpp:1101-1102, and a second
// RegisterRuntimeExport for the same mangled name makes one of the two dead
// code (the SHADOWED hazard). Caught by lane LOOPS, not by review.
//
// WHY THE PRE-WRITE GREP MISSED IT, because the next person will grep the same
// way: the search was scoped to runtime/src/achievement/ — reasoning about
// ownership by CLASS. That ctor lives under runtime/src/engine/ because it was
// found as an INLINED BLOCK inside Engine::Startup and ported by the lane that
// owned the CALLER. A directory-scoped grep cannot see a port filed under its
// caller's namespace. Grep the MANGLED NAME across the whole tree.

// BATCH C registration entry point (batchc_*.cpp).
extern "C" int RtAchBatchCGoalListDtorRegister();

void RtAchievementPortInit() {
    // BATCH C — Achievement::GoalList::~GoalList. Unconditional, see
    // DIRECTION.md §2.3.
    {
        // Hoisted out of the printf: the perf build compiles printf to ((void)0) (standalone/log_strip.h).
        const int n_achbatchcgoallistdtor = RtAchBatchCGoalListDtorRegister();
        printf("[RtPort] BATCH C: %d achievement leaf registered\n", n_achbatchcgoallistdtor);
    }
    int n = 0;
    n += RtAchGoalRegister();
    n += RtAchGoalListRegister();
    n += RtAchManagerRegister();
    n += RtAchStateRegister();
    n += RtAchSerializeRegister();
    n += RtAchManagerTypeInfoAccessorsRegister();
    n += RtAchVtableRegister();
    printf("[RtPort] Achievement: %d exports registered\n", n);
}
