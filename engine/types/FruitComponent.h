#pragma once
// FruitComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: FruitComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140159450
//   +0x28 = 0 (qword), +0x30 = 0 (dword), +0x34 = 0 (byte)
//   flags |= 0x88

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace FruitComponentAddr {
    constexpr uintptr_t Vtable = 0x140159450;
}

// ENDGAME B.3.8 CIT: the POD is gone — `FruitComponent` is a real class now
// (code/Game/Component/FruitComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/FruitComponent.h"

