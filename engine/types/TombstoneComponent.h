#pragma once
// TombstoneComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x30.
//
// Decompiled from: TombstoneComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x14015a980
//   +0x28 = 0 (qword)
//   flags |= 0x88

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace TombstoneComponentAddr {
    constexpr uintptr_t Vtable = 0x14015a980;
}

// ENDGAME B.3.8 CIT: the POD is gone — `TombstoneComponent` is a real class now
// (code/Game/Component/TombstoneComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/TombstoneComponent.h"

