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

void RtAchievementPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_ACHIEVEMENT")) {
        printf("[RtPort] BANISHED_RT_ACHIEVEMENT=0 -- Achievement namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_GOAL"))   n += RtAchGoalRegister();
    if (RtWaveEnabled("BANISHED_RT_ACHMGR")) n += RtAchManagerRegister();
    printf("[RtPort] Achievement: %d exports registered\n", n);
}
