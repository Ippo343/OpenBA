// ============================================================================
// runtime/src/achievement/rt_factories_achievement.inc -- WAVE 28 LANE FACTREST
//
// MECHANICALLY TRANSCRIBED from `objdump -d` over the retail
// C:\GOG Games\Banished\Runtime-x64.dll on disk -- not typed by hand, and not
// read out of Ghidra's decompiled C. Every step carries the retail instruction
// address it came from, so any single row can be checked against the
// disassembly without reading the interpreter.
//
// Included by rt_factories_achievement.cpp (Object:: by rt_factories_core.cpp).
// Contract and op vocabulary: runtime/src/object/rt_factories.h
// ============================================================================

// --------------------------------------------------------------------------
// Achievement::Achievement
//   ?CreateNewObject@Achievement@1@SAPEAVBase@Object@@XZ  0x180001130
//   ?CreateInPlaceObject@Achievement@1@SAPEAVBase@Object@@PEAX@Z  0x1800011f0
// --------------------------------------------------------------------------
static const RtCtorStep kStepsAchievement_Achievement_New[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x180001193ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x18000119dull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x1800011abull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x1800011aeull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x1800011b2ull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Achievement@0@6B@", 0x1800011bdull },
    { RtFactOp::Imm, 8, 0x20, 0x0ull, nullptr, 0x1800011c0ull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x1800011c4ull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x1800011c8ull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x1800011ccull },
    { RtFactOp::Imm, 8, 0x40, 0x0ull, nullptr, 0x1800011d0ull },
    { RtFactOp::Imm, 8, 0x48, 0x0ull, nullptr, 0x1800011d4ull },
    { RtFactOp::Imm, 8, 0x50, 0x0ull, nullptr, 0x1800011d8ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static const RtCtorStep kStepsAchievement_Achievement_IP[] = {
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Base@Object@@6B@", 0x18000120dull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7External@Object@@6B@", 0x180001217ull },
    { RtFactOp::Imm, 8, 0x08, 0x0ull, nullptr, 0x180001225ull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x180001228ull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x18000122cull },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7Achievement@0@6B@", 0x180001237ull },
    { RtFactOp::Imm, 8, 0x20, 0x0ull, nullptr, 0x18000123aull },
    { RtFactOp::Imm, 8, 0x28, 0x0ull, nullptr, 0x18000123eull },
    { RtFactOp::Imm, 8, 0x30, 0x0ull, nullptr, 0x180001242ull },
    { RtFactOp::Imm, 8, 0x38, 0x0ull, nullptr, 0x180001246ull },
    { RtFactOp::Imm, 8, 0x40, 0x0ull, nullptr, 0x18000124aull },
    { RtFactOp::Imm, 8, 0x48, 0x0ull, nullptr, 0x18000124eull },
    { RtFactOp::Imm, 8, 0x50, 0x0ull, nullptr, 0x180001252ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountAchievement_Achievement[3];
static const RtFactoryClass kClsAchievement_Achievement = {
    "Achievement::Achievement", 0x58, 8,
    kStepsAchievement_Achievement_New, kStepsAchievement_Achievement_IP,
    0x180001130ull, 0x1800011f0ull, kCountAchievement_Achievement,
};
extern "C" void* RtFactNew_Achievement_Achievement()          { return RtFactoryNew(kClsAchievement_Achievement); }
extern "C" void* RtFactIP_Achievement_Achievement(void* p)    { return RtFactoryInPlace(kClsAchievement_Achievement, p); }

// --------------------------------------------------------------------------
// Achievement::AchievementInstance
//   ?CreateNewObject@AchievementInstance@Achievement@@SAPEAVBase@Object@@XZ  0x1800018b0
//   ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z  0x1800070c0
// --------------------------------------------------------------------------
static const RtCtorStep kStepsAchievement_AchievementInstance_New[] = {
    { RtFactOp::Call, 8, 0, 0, "??0Internal@Object@@QEAA@XZ", 0x18000190full },
    { RtFactOp::Vptr, 8, 0x00, 0, "??_7AchievementInstance@Achievement@@6B@", 0x18000191cull },
    { RtFactOp::Imm, 8, 0x10, 0x0ull, nullptr, 0x18000192aull },
    { RtFactOp::Imm, 8, 0x18, 0x0ull, nullptr, 0x18000192dull },
    { RtFactOp::Imm, 8, 0x20, 0x0ull, nullptr, 0x180001931ull },
    { RtFactOp::End, 0, 0, 0, nullptr, 0 },
};
static uint64_t kCountAchievement_AchievementInstance[3];
static const RtFactoryClass kClsAchievement_AchievementInstance = {
    "Achievement::AchievementInstance", 0x28, 8,
    kStepsAchievement_AchievementInstance_New, nullptr,
    0x1800018b0ull, 0x1800070c0ull, kCountAchievement_AchievementInstance,
};
extern "C" void* RtFactNew_Achievement_AchievementInstance()          { return RtFactoryNew(kClsAchievement_AchievementInstance); }
extern "C" void* RtFactIP_Achievement_AchievementInstance(void* p)    { return RtFactoryInPlace(kClsAchievement_AchievementInstance, p); }

// (Achievement::Goal / GoalInstance: ENDGAME B.3.9 ENG -- the classes' own
// factory pairs, runtime/src/achievement/goal.cpp.)


static const RtFactoryReg kRegs[] = {
    { "?CreateNewObject@Achievement@1@SAPEAVBase@Object@@XZ", 0x180001130ull, (void*)&RtFactNew_Achievement_Achievement,
      "?CreateInPlaceObject@Achievement@1@SAPEAVBase@Object@@PEAX@Z", 0x1800011f0ull, (void*)&RtFactIP_Achievement_Achievement, &kClsAchievement_Achievement },
    { "?CreateNewObject@AchievementInstance@Achievement@@SAPEAVBase@Object@@XZ", 0x1800018b0ull, (void*)&RtFactNew_Achievement_AchievementInstance,
      "?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z", 0x1800070c0ull, (void*)&RtFactIP_Achievement_AchievementInstance, &kClsAchievement_AchievementInstance },
};
