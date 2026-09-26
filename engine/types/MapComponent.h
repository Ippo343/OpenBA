#pragma once
// MapComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: MapComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140163aa0
//   +0x28 = 0xFFFF (uint16), +0x2A = 0xFFFF (uint16)
//   +0x2C = 0xFFFF (uint16), +0x2E = 0xFFFF (uint16)
//   These look like 4 sentinel tile coordinates (max uint16 = invalid).
//   flags |= 0x3

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace MapComponentAddr {
    constexpr uintptr_t Vtable = 0x140163aa0;
}

// ENDGAME B.3.8 VIS: the POD is gone — `MapComponent` is a real class now
// (code/Game/Component/MapComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/MapComponent.h"
