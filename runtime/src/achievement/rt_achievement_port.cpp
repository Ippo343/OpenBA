// ============================================================================
// runtime/src/achievement/rt_achievement_port.cpp — RtAchievementPortInit()
// Wave 5 lane group A/B: the Achievement namespace
// ============================================================================
// One entry point for the Achievement namespace. Same shape as
// RtIoPortInit() (Wave 4): each lane owns exactly ONE source file and exposes
// exactly ONE Register function, so no two lanes ever edit the same file.
//
// Switches, outermost first:
//   BANISHED_RT_ACHIEVEMENT=0   leave the whole namespace on the DLL
//   BANISHED_RT_GOAL=0          ... just Goal / GoalInstance / GoalList (lane A)
//   BANISHED_RT_ACHMGR=0        ... just Achievement / AchievementInstance /
//                                   AchievementManager                  (lane B)
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtAchGoalRegister();       // goal.cpp                (lane A)
extern int RtAchManagerRegister();    // achievement_manager.cpp (lane B)
// Wave 9 lane P — goal_serialize.cpp. Gated by BANISHED_RT_GOAL with goal.cpp,
// not by a switch of its own: class atomicity (PLAN §4.1 decision 5) means the
// two stream members must flip together with the ctors/dtors that build the
// objects they serialize.
extern int RtAchGoalSerializeRegister();  // goal_serialize.cpp      (Wave 9 P)
// Wave 23 lane ACHIEVE. Both gated by BANISHED_RT_ACHMGR with
// achievement_manager.cpp: achievement_state.cpp ports five more members of the
// SAME three classes (the manager dtor, RemoveAll, ReadAchievements,
// SetAchievementMet and Achievement::CreateInstance) and
// achievement_serialize.cpp the two stream members Wave 5 held back. Class
// atomicity (PLAN �4.1 decision 5) puts them on one switch.
extern int RtAchStateRegister();          // achievement_state.cpp     (Wave 23)
extern int RtAchSerializeRegister();      // achievement_serialize.cpp (Wave 23)

void RtAchievementPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_ACHIEVEMENT")) {
        printf("[RtPort] BANISHED_RT_ACHIEVEMENT=0 -- Achievement namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_GOAL")) {
        n += RtAchGoalRegister();
        n += RtAchGoalSerializeRegister();
    }
    if (RtWaveEnabled("BANISHED_RT_ACHMGR")) {
        n += RtAchManagerRegister();
        n += RtAchStateRegister();
        n += RtAchSerializeRegister();
    }
    printf("[RtPort] Achievement: %d exports registered\n", n);
}
