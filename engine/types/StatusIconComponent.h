#pragma once
// StatusIconComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x108.
//
// Decompiled from: StatusIconComponent_Ctor
// Parent: Component (direct)
//
// Contains Graphics::Geometry sub-object at +0x28 (initialized by Geometry ctor),
// then overwritten vtable at +0x28 to 0x14015f0c0.
//
// Constructor sets:
//   vtable = 0x140164620
//   +0x28: Geometry sub-object + vtable override to 0x14015f0c0
//   +0xE8 = 0 (qword), +0xF0 = 0 (dword), +0xF4 = 0 (byte)
//   +0xF8 = 0 (qword), +0x100 = -1 (int32)
//   flags |= 0xC2

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace StatusIconComponentAddr {
    constexpr uintptr_t Vtable = 0x140164620;
}

// ENDGAME B.3.8 VIS: the POD is gone — `StatusIconComponent` is a real class now
// (code/Game/Component/StatusIconComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/StatusIconComponent.h"
