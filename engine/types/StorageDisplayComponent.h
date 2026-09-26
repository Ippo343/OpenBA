#pragma once
// StorageDisplayComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: StorageDisplayComponent_Ctor (0x140056a00)
// Parent: Component (direct, NOT ControllerComponent)
//
// Constructor sets:
//   vtable = 0x14015a780
//   +0x28 = 0 (qword)
//   +0x30 = 0 (qword)
//   flags |= 0xCB (Serialize | Draw | Create | Update | bit6 | bit7)
//
// The heavy flag mask 0xCB suggests this component participates in
// drawing, updating, serialization, and entity lifecycle events.

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace StorageDisplayComponentAddr {
    constexpr uintptr_t Ctor    = 0x140056a00;
    constexpr uintptr_t Vtable  = 0x14015a780;
}

// ENDGAME B.3.8 VIS: the POD is gone — `StorageDisplayComponent` is a real class now
// (code/Game/Component/StorageDisplayComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/StorageDisplayComponent.h"
