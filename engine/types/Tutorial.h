#pragma once
// engine/types/Tutorial.h — ENDGAME B.3.8 block GOAL: the POD is gone.
// `Tutorial` is a real class now (code/Game/Tutorial/Tutorial.h, compiler
// vtable on Object::External). This forwarder keeps the old include spelling
// working until the closing pass deletes it (B6-4.4).
#include <cstdint>
#include "Tutorial/Tutorial.h"

namespace TutorialAddr {
    constexpr uintptr_t Factory      = 0x140026e50;
    constexpr uintptr_t Vtable       = 0x140160740;   // retail table (documentation)
    constexpr uintptr_t RegisterType = 0x14014d750;
    constexpr uintptr_t Cleanup      = 0x140026eb0;
}
