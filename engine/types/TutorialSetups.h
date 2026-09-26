#pragma once
// engine/types/TutorialSetups.h — ENDGAME B.3.8 block GOAL: the PODs are gone.
// `TutorialSetup` (abstract) and its leaves TutorialSetupCamera,
// TutorialSetupGameSpeed and TutorialSetupRestrictPlayer are real classes now
// (code/Game/Tutorial/, compiler vtables). This forwarder keeps the old include
// spelling working until the closing pass deletes it (B6-4.4).
//
// B38_RULES D9 #14: this header used to carry a SECOND global
// `struct TutorialSetupDialog` (the other in engine/types/ui/Dialogs.h) — a
// live ODR violation hidden by disjoint include sets. It is gone from here;
// TutorialSetupDialog has one definition.
#include <cstdint>
#include "Tutorial/TutorialSetup.h"
#include "Tutorial/TutorialSetupCamera.h"
#include "Tutorial/TutorialSetupGameSpeed.h"
#include "Tutorial/TutorialSetupRestrictPlayer.h"

// Retail addresses (documentation; the tables are the compiler's now).
namespace TutorialSetupAddr {
    constexpr uintptr_t Vtable = 0x140160c30;
}
namespace TutorialSetupCameraAddr {
    constexpr uintptr_t Factory = 0x140028400;
    constexpr uintptr_t Vtable  = 0x140160ca0;
    constexpr uintptr_t Cleanup = 0x140028470;
}
namespace TutorialSetupGameSpeedAddr {
    constexpr uintptr_t Factory = 0x1400289c0;
    constexpr uintptr_t Vtable  = 0x140160da0;
    constexpr uintptr_t Cleanup = 0x140028a20;
}
namespace TutorialSetupRestrictPlayerAddr {
    constexpr uintptr_t Ctor    = 0x1400e3220;
    constexpr uintptr_t Factory = 0x140028b40;
    constexpr uintptr_t Vtable  = 0x140160e20;
}
// TutorialSetupDialogAddr lives in engine/types/ui/Dialogs.h (one definition).
