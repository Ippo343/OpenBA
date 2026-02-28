#pragma once
// Tutorial Goal types — binary-compatible struct layouts.
// Application-x64.exe, build 170910.
//
// TutorialGoal base (vtable 0x1401607a0) is abstract.
// All tutorial goals inherit from Object::External -> TutorialGoal -> [concrete].

#include <cstdint>
#include <cstddef>

namespace TutorialGoalAddr {
    constexpr uintptr_t Vtable       = 0x1401607a0;
    constexpr uintptr_t RegisterType = 0x14014d790;
}

// TutorialGoalButton — wait for button press. Size = 0x30.
// Decompiled from: TutorialGoalButton_Factory (0x140027290, inline ctor)
namespace TutorialGoalButtonAddr {
    constexpr uintptr_t Factory      = 0x140027290;
    constexpr uintptr_t Vtable       = 0x140160800;
    constexpr uintptr_t RegisterType = 0x14014d7d0;
    constexpr uintptr_t Cleanup      = 0x140027300;
}

struct TutorialGoalButton {
    void*    vtable;             // +0x00: -> 0x140160800
    uint8_t  _extBase[0x18];     // +0x08: External base
    void*    _field20;           // +0x20: zeroed (button name?)
    uint8_t  _field28;           // +0x28: zeroed (bool, pressed?)
    uint8_t  _pad29[7];          // +0x29: padding
};
static_assert(sizeof(TutorialGoalButton) == 0x30, "TutorialGoalButton size mismatch");

// TutorialGoalGameSpeed — wait for game speed change. Size = 0x28.
// Decompiled from: TutorialGoalGameSpeed_Factory (0x140027630, inline ctor)
namespace TutorialGoalGameSpeedAddr {
    constexpr uintptr_t Factory      = 0x140027630;
    constexpr uintptr_t Vtable       = 0x1401608e0;
    constexpr uintptr_t Cleanup      = 0x140027690;
}

struct TutorialGoalGameSpeed {
    void*    vtable;             // +0x00: -> 0x1401608e0
    uint8_t  _extBase[0x18];     // +0x08: External base
    int32_t  targetSpeed;        // +0x20: 1 (target game speed)
    uint8_t  _pad24[4];          // +0x24: padding
};
static_assert(sizeof(TutorialGoalGameSpeed) == 0x28, "TutorialGoalGameSpeed size mismatch");

// TutorialGoalCount — wait for item/building count. Size = 0x50.
// Decompiled from: TutorialGoalCount_Ctor (0x1400dfa60)
namespace TutorialGoalCountAddr {
    constexpr uintptr_t Ctor         = 0x1400dfa60;
    constexpr uintptr_t Factory      = 0x1400277b0;
    constexpr uintptr_t Vtable       = 0x140160950;
}

struct TutorialGoalCount {
    void*    vtable;             // +0x00: -> 0x140160950
    uint8_t  _extBase[0x18];     // +0x08: External base
    void*    _field20;           // +0x20: zeroed (type reference?)
    uint8_t  _field28;           // +0x28: zeroed (bool?)
    uint8_t  _pad29[7];          // +0x29: padding
    void*    _field30;           // +0x30: zeroed
    int32_t  _field38;           // +0x38: -1 sentinel (target count?)
    uint8_t  _pad3C[4];          // +0x3C: padding
    void*    _field40;           // +0x40: zeroed
    void*    _field48;           // +0x48: zeroed
};
static_assert(sizeof(TutorialGoalCount) == 0x50, "TutorialGoalCount size mismatch");
