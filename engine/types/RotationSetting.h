#pragma once
// RotationSetting — Rotation config for placed objects, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x18.
//
// Decompiled from: RotationSetting_Factory (0x140026d20, inline ctor)
// Inherits: Object::Internal (0x10) -> RotationSetting
// Tiny type — manages rotation state. Has two vtable writes (intermediate base?).

#include <cstdint>
#include <cstddef>

namespace RotationSettingAddr {
    constexpr uintptr_t Factory = 0x140026d20;
    constexpr uintptr_t Vtable  = 0x140160710;
}

struct RotationSetting {
    void*    vtable;             // +0x00: -> 0x140160710
    uint8_t  _base08[8];         // +0x08: Internal base
    uint32_t rotation;           // +0x10: 0 (rotation value, 0-3?)
    uint8_t  _pad14[4];          // +0x14: padding
};

static_assert(sizeof(RotationSetting) == 0x18, "RotationSetting size mismatch");
static_assert(offsetof(RotationSetting, vtable) == 0x00, "");
static_assert(offsetof(RotationSetting, rotation) == 0x10, "");
