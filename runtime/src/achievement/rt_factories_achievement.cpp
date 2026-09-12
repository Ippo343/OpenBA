// ============================================================================
// runtime/src/achievement/rt_factories_achievement.cpp -- WAVE 28 LANE FACTREST
//
// 4 classes. Goal is a retail null stub in both slots; AchievementInstance
// and GoalInstance share ?CreateInPlaceObject@Internal@Object@@, which is
// itself a null stub.
//
// The bodies are the shared interpreter in runtime/src/object/rt_factories_core.cpp;
// everything class-specific is the transcribed step table in the .inc beside
// this file. Registration is BOTH doors (name and address) per slot, default
// ON, from RtFactoriesPortInit() which runs before RtDataMintInit().
//
// ATTENDANCE 0 IS THE EXPECTED READING HERE while the DLL is present, and it is
// NOT a verification of any body. See rt_factories.h.
// ============================================================================

#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../object/rt_factories.h"

#include "rt_factories_achievement.inc"

int RtFactoriesAchievementRegister() {
    return RtFactoryRegisterAll(kRegs, sizeof(kRegs) / sizeof(kRegs[0]));
}
