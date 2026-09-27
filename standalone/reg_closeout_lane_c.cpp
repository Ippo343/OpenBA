// standalone/reg_closeout_lane_c.cpp
//
// Exe close-out wave, lane C (citizens + notifications). New registrations only —
// address_registry.cpp is being edited by the parallel lanes, so this wave keeps
// its own file and self-registers from a static initializer (the registry is a
// function-local static in resolve_standalone.cpp, so this is init-order safe;
// every standalone/*.cpp is linked directly into the exe, so nothing is dropped).
//
//   0x140096960  GameUpdateLoop's achievement-award EVENT HANDLER
//                (code/Game/Game/eventlog_achievement.cpp).
//                Installed on the AchievementManager by GameUpdateLoop::Load
//                step 30 (retail call site 0x14009752e). PLAN.md §1.2 item 6.
//
// PLAN.md §1.2 item 2 (CitizenAI Work / AdultTransition / ElderTransition) needed
// NO registration: all three were already ported in
// code/Game/Component/citizenai_handlers3.cpp and registered by
// standalone/reg_citizenai_handlers3.cpp. See docs/waves/exe_closeout/lane_c.md.

#include "resolve.h"
#include <cstdint>

extern "C" void EventLog_OnAchievementAwarded(int achievementId);  // 0x140096960

extern "C" void RegisterCloseoutLaneC() {
}

namespace {
struct CloseoutLaneCAutoReg {
    CloseoutLaneCAutoReg() { RegisterCloseoutLaneC(); }
};
CloseoutLaneCAutoReg g_closeoutLaneCAutoReg;
}  // namespace
