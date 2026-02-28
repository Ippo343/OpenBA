#pragma once
// PlayAnimation — Animation playback action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: PlayAnimation_Factory (0x1400011f0) — inline ctor
// Inherits: Object::Internal -> Action (0x20) -> PlayAnimation

#include <cstdint>
#include <cstddef>

namespace PlayAnimationAddr {
    constexpr uintptr_t Factory      = 0x1400011f0;
    constexpr uintptr_t Vtable       = 0x140158950;
    constexpr uintptr_t RegisterType = 0x14014a5c0; // shares with ShowTool
}

struct PlayAnimation {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x140158950
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- PlayAnimation fields ---
    void*    _field20;           // +0x20: zeroed (animation ref?)
    uint8_t  _pad28[8];          // +0x28: gap
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
};

static_assert(sizeof(PlayAnimation) == 0x40, "PlayAnimation size mismatch");
static_assert(offsetof(PlayAnimation, vtable) == 0x00, "");
