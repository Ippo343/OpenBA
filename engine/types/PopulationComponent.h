#pragma once
// PopulationComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x70.
//
// Decompiled from: PopulationComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x14015a360
//   +0x28 = 0 (qword), +0x30 = 0 (dword), +0x38 = 0, +0x40 = 0
//   +0x48 = 0 (dword), +0x50 = 0, +0x58 = 0, +0x60 = 0, +0x68 = 0
//   flags |= 0xCB (Draw+Update+Serialize+Create+bit6+bit7)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace PopulationComponentAddr {
    constexpr uintptr_t Vtable = 0x14015a360;
}

// ENDGAME B.3.8 CIT: the POD is gone — `PopulationComponent` is a real class now
// (code/Game/Component/PopulationComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/PopulationComponent.h"

