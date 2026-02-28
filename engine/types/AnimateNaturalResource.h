#pragma once
// AnimateNaturalResource — Action to animate a natural resource, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x30.
//
// Decompiled from: AnimateNaturalResource_Factory (0x140021dc0, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> AnimateNaturalResource

#include <cstdint>
#include <cstddef>

namespace AnimateNaturalResourceAddr {
    constexpr uintptr_t Factory = 0x140021dc0;
    constexpr uintptr_t Vtable  = 0x14015fb10;
}

struct AnimateNaturalResource {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fb10
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- AnimateNaturalResource fields ---
    void*    _field20;           // +0x20: entity ref (natural resource)
    int32_t  animIndex;          // +0x28: animation index
    uint8_t  waitForAnim;        // +0x2C: bool — wait for animation to complete
    uint8_t  _pad2D[3];          // +0x2D: padding
};

static_assert(sizeof(AnimateNaturalResource) == 0x30, "AnimateNaturalResource size mismatch");
static_assert(offsetof(AnimateNaturalResource, vtable) == 0x00, "");
static_assert(offsetof(AnimateNaturalResource, animIndex) == 0x28, "");
static_assert(offsetof(AnimateNaturalResource, waitForAnim) == 0x2C, "");
