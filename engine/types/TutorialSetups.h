#pragma once
// Tutorial Setup types — binary-compatible struct layouts.
// Application-x64.exe, build 170910.
//
// TutorialSetup base (vtable 0x140160c30) configures game state for tutorial steps.

#include <cstdint>
#include <cstddef>

namespace TutorialSetupAddr {
    constexpr uintptr_t Vtable = 0x140160c30;
}

// TutorialSetupCamera — camera position/angle setup. Size = 0x30.
// Decompiled from: TutorialSetupCamera_Factory (0x140028400, inline ctor)
namespace TutorialSetupCameraAddr {
    constexpr uintptr_t Factory = 0x140028400;
    constexpr uintptr_t Vtable  = 0x140160ca0;
    constexpr uintptr_t Cleanup = 0x140028470;
}

struct TutorialSetupCamera {
    void*    vtable;             // +0x00: -> 0x140160ca0
    uint8_t  _extBase[0x18];     // +0x08: External base
    int64_t  _field20;           // +0x20: -1 (all 0xFF bytes) (camera target X,Y packed?)
    int64_t  _field28;           // +0x28: -1
};
static_assert(sizeof(TutorialSetupCamera) == 0x30, "TutorialSetupCamera size mismatch");

// TutorialSetupGameSpeed — set game speed. Size = 0x28.
// Decompiled from: TutorialSetupGameSpeed_Factory (0x1400289c0, inline ctor)
namespace TutorialSetupGameSpeedAddr {
    constexpr uintptr_t Factory = 0x1400289c0;
    constexpr uintptr_t Vtable  = 0x140160da0;
    constexpr uintptr_t Cleanup = 0x140028a20;
}

struct TutorialSetupGameSpeed {
    void*    vtable;             // +0x00: -> 0x140160da0
    uint8_t  _extBase[0x18];     // +0x08: External base
    int32_t  speed;              // +0x20: 1 (game speed to set)
    uint8_t  _field24;           // +0x24: zeroed (bool, pause after?)
    uint8_t  _pad25[3];          // +0x25: padding
};
static_assert(sizeof(TutorialSetupGameSpeed) == 0x28, "TutorialSetupGameSpeed size mismatch");

// TutorialSetupRestrictPlayer — restrict player actions. Size = 0x68.
// Decompiled from: TutorialSetupRestrictPlayer_Ctor (0x1400e3220)
namespace TutorialSetupRestrictPlayerAddr {
    constexpr uintptr_t Ctor    = 0x1400e3220;
    constexpr uintptr_t Factory = 0x140028b40;
    constexpr uintptr_t Vtable  = 0x140160e20;
}

struct TutorialSetupRestrictPlayer {
    void*    vtable;             // +0x00: -> 0x140160e20
    uint8_t  _extBase[0x18];     // +0x08: External base
    int64_t  _field20;           // +0x20: -1 (restriction mask A?)
    int64_t  _field28;           // +0x28: -1 (restriction mask B?)
    uint32_t _field30;           // +0x30: zeroed
    uint8_t  _pad34[4];          // +0x34: padding
    void*    _field38;           // +0x38: zeroed
    int32_t  _field40;           // +0x40: -1 sentinel
    uint8_t  _pad44[4];          // +0x44: padding
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    int32_t  _field58;           // +0x58: -1 sentinel
    uint8_t  _pad5C[4];          // +0x5C: padding
    void*    _field60;           // +0x60: zeroed
};
static_assert(sizeof(TutorialSetupRestrictPlayer) == 0x68, "TutorialSetupRestrictPlayer size mismatch");

// TutorialSetupDialog — dialog for tutorial setup. Size = 0x78.
// Decompiled from: TutorialSetupDialog_Ctor (0x1400e2cc0)
namespace TutorialSetupDialogAddr {
    constexpr uintptr_t Ctor    = 0x1400e2cc0;
    constexpr uintptr_t Vtable  = 0x140160d20;
}

struct TutorialSetupDialog {
    void*    vtable;             // +0x00: -> 0x140160d20
    uint8_t  _extBase[0x18];     // +0x08: External base
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    void*    _field40;           // +0x40: zeroed
    uint32_t _field48;           // +0x48: zeroed
    uint8_t  _pad4C[4];          // +0x4C: padding
    void*    _field50;           // +0x50: zeroed
    void*    _field58;           // +0x58: zeroed
    int32_t  _field60;           // +0x60: -1 sentinel
    uint8_t  _pad64[4];          // +0x64: padding
    void*    _field68;           // +0x68: zeroed
    uint32_t _field70;           // +0x70: zeroed
    uint8_t  _pad74[4];          // +0x74: padding
};
static_assert(sizeof(TutorialSetupDialog) == 0x78, "TutorialSetupDialog size mismatch");
