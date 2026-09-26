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

// ENDGAME B.3.8 VIS: the POD is gone — `HighlightComponent` is a real class now
// (code/Game/Component/HighlightComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/HighlightComponent.h"
