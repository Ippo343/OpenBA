#pragma once
// engine/types/TutorialImages.h — ENDGAME B.3.8 block GOAL: the PODs are gone.
// TutorialImage and its leaves TutorialImageHotKey, TutorialImageInputKey and
// TutorialImageToolbarIcon are real classes now (code/Game/Tutorial/, compiler
// vtables). This forwarder keeps the old include spelling working until the
// closing pass deletes it (B6-4.4).
#include <cstdint>
#include "Tutorial/TutorialImage.h"
#include "Tutorial/TutorialImageHotKey.h"
#include "Tutorial/TutorialImageInputKey.h"
#include "Tutorial/TutorialImageToolbarIcon.h"

// Retail addresses (documentation; the tables are the compiler's now).
namespace TutorialImageAddr {
    constexpr uintptr_t Factory = 0x140027980;
    constexpr uintptr_t Vtable  = 0x140160a40;
    constexpr uintptr_t Cleanup = 0x1400279e0;
}
namespace TutorialImageHotKeyAddr {
    constexpr uintptr_t Ctor    = 0x1400e0680;
    constexpr uintptr_t Factory = 0x140027d00;
    constexpr uintptr_t Vtable  = 0x140160ab0;
}
namespace TutorialImageInputKeyAddr {
    constexpr uintptr_t Factory = 0x140027ea0;
    constexpr uintptr_t Vtable  = 0x140160b20;
    constexpr uintptr_t Cleanup = 0x140027f00;
}
namespace TutorialImageToolbarIconAddr {
    constexpr uintptr_t Ctor    = 0x1400e0c30;
    constexpr uintptr_t Factory = 0x140028020;
    constexpr uintptr_t Vtable  = 0x140160b90;
}
