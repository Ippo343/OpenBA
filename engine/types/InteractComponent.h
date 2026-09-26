#pragma once
// InteractComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: InteractComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x1401638b0
//   +0x28 = 0, +0x30 = 0
//   flags |= 0x1

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace InteractComponentAddr {
    constexpr uintptr_t Vtable = 0x1401638b0;
}

// ENDGAME B.3.8 VIS: the POD is gone — `InteractComponent` is a real class now
// (code/Game/Component/InteractComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/InteractComponent.h"
