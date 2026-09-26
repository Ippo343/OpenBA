#pragma once
// engine/types/RotationSetting.h — ENDGAME B.3.8 TOOL: the POD is gone.
// `RotationSetting` is a real class now (code/Game/Player/RotationSetting.h,
// compiler vtable). This forwarder keeps the old include spelling working
// until the closing pass deletes it (B8-4).
#include "Player/RotationSetting.h"

namespace RotationSettingAddr {
    constexpr uintptr_t Factory = 0x140026d20;   // retail create (documentation)
    constexpr uintptr_t Vtable  = 0x140160710;   // retail table (documentation)
}
