#pragma once
// UIComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: UIComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x1401649f0
//   +0x28 = 0, +0x30 = 0
//   flags |= 0x80

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace UIComponentAddr {
    constexpr uintptr_t Vtable = 0x1401649f0;
}

// ENDGAME B.3.8 VIS: the POD is gone — `UIComponent` is a real class now
// (code/Game/Component/UIComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/UIComponent.h"
