#pragma once
// HighlightComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x128.
//
// Decompiled from: HighlightComponent_Ctor
// Parent: Component (direct)
//
// Contains large embedded sub-objects:
//   +0x28: Graphics::EdgeMeshInstance (initialized by EdgeMeshInstance ctor)
//   +0xF8: qword = 0
//   +0x100: Math::Transform (initialized by Transform ctor)
//   +0x11C: byte = 0
//   +0x120: int32 = -1

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace HighlightComponentAddr {
    constexpr uintptr_t Vtable = 0x140159990;
}

struct HighlightComponent {
    void*    vtable;               // +0x00
    void*    next;                 // +0x08
    void*    owner;                // +0x10
    void*    _unk18;               // +0x18
    uint32_t flags;                // +0x20
    uint8_t  _pad24[4];            // +0x24

    // Graphics::EdgeMeshInstance sub-object (0xD0 bytes)
    uint8_t  edgeMeshInstance[0xD0]; // +0x28..+0xF7

    void*    fieldF8;              // +0xF8: zeroed

    // Math::Transform sub-object (0x1C bytes)
    uint8_t  transform[0x1C];      // +0x100..+0x11B

    uint8_t  highlightFlag;        // +0x11C: zeroed (byte)
    uint8_t  _pad11D[3];           // +0x11D: padding
    int32_t  highlightId;          // +0x120: initialized to -1
    uint8_t  _pad124[4];           // +0x124: padding to 0x128
};

static_assert(sizeof(HighlightComponent) == 0x128, "HighlightComponent size mismatch");
static_assert(offsetof(HighlightComponent, vtable) == 0x00, "");
static_assert(offsetof(HighlightComponent, flags) == 0x20, "");
static_assert(offsetof(HighlightComponent, edgeMeshInstance) == 0x28, "");
static_assert(offsetof(HighlightComponent, fieldF8) == 0xF8, "");
static_assert(offsetof(HighlightComponent, transform) == 0x100, "");
static_assert(offsetof(HighlightComponent, highlightFlag) == 0x11C, "");
static_assert(offsetof(HighlightComponent, highlightId) == 0x120, "");
