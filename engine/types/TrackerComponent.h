#pragma once
// TrackerComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x50.
//
// Created via: TrackerComponent_Factory (0x14000d7a0)
// Parent: Component (direct, no named Ctor)
//
// Factory does: MemAlloc(0x50) + Component_Ctor + set vtable + init fields
//   vtable = 0x14015ab80
//   +0x28 = 0, +0x30 = 0, +0x38 = 0, +0x40 = 0
//   +0x48 = -1 (int32)
//   NO flag modification

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace TrackerComponentAddr {
    constexpr uintptr_t Factory = 0x14000d7a0;
    constexpr uintptr_t Vtable  = 0x14015ab80;
}

// ENDGAME B.3.8 VIS: the POD is gone — `TrackerComponent` is a real class now
// (code/Game/Component/TrackerComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/TrackerComponent.h"
