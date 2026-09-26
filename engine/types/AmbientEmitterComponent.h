#pragma once
// AmbientEmitterComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: AmbientEmitterComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140158a60
//   +0x28 = 0, +0x30 = 0 (dword), +0x38 = 0, +0x40 = 0
//   flags |= 0x43

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace AmbientEmitterComponentAddr {
    constexpr uintptr_t Vtable = 0x140158a60;
}

// ENDGAME B.3.8 CIT: the POD is gone — `AmbientEmitterComponent` is a real class now
// (code/Game/Component/AmbientEmitterComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/AmbientEmitterComponent.h"

