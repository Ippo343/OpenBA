#pragma once
// LivestockComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: LivestockComponent_Ctor (0x140123ee0)
//
// Extends Component (0x28). Has a real Update at vtable[9].
// Two 8-byte fields zeroed by ctor.
// Flags: base | 0x0B

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace LivestockComponentAddr {
    constexpr uintptr_t Ctor       = 0x140123ee0;
    constexpr uintptr_t Vtable     = 0x1401639b0;
    constexpr uintptr_t ScalarDtor = 0x140035680; // vtable[0]
    constexpr uintptr_t Startup    = 0x140035460; // vtable[2]
    constexpr uintptr_t Shutdown   = 0x140123e10; // vtable[3]
    constexpr uintptr_t Draw       = 0x140123c70; // vtable[6]
    constexpr uintptr_t OnCreate   = 0x140123c00; // vtable[7]
    constexpr uintptr_t Update     = 0x140123b50; // vtable[9]
    // Serialize = base no-op (0x140118a50)
}

// ENDGAME B.3.8 CIT: the POD is gone — `LivestockComponent` is a real class now
// (code/Game/Component/LivestockComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/LivestockComponent.h"

