#pragma once
// CitizenComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x290.
//
// Decompiled from: CitizenComponent_Ctor (0x140114770)
//
// Extends Component (0x28). Has a real Update at vtable[9].
// Largest AI component -- full citizen state (health, happiness, inventory, mesh, etc).
// Contains a Graphics::MeshInstance sub-object at +0xB8 (0x120 bytes).
// Flags: base | 0xCB
//
// Ctor init pattern:
//   *(u64)(+0x28) = 0
//   *(u32)(+0x30) = 0xFFFFFFFF
//   Zero block: +0x34..+0x84 (various u64/u32 writes)
//   *(u32)(+0x88) = 0
//   Zero: +0x90..+0xB4
//   *(u32)(+0xA8) = 0xFFFFFFFF
//   Graphics::MeshInstance::MeshInstance at +0xB8 (0x120 bytes)
//   Post-mesh fields: +0x1D8..+0x288

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace CitizenComponentAddr {
    constexpr uintptr_t Ctor       = 0x140114770;
    constexpr uintptr_t Vtable     = 0x140162580;
    constexpr uintptr_t ScalarDtor = 0x140030270; // vtable[0]
    constexpr uintptr_t Startup    = 0x1400300f0; // vtable[2]
    constexpr uintptr_t Shutdown   = 0x140114160; // vtable[3]
    constexpr uintptr_t Serialize  = 0x140114050; // vtable[4]
    constexpr uintptr_t Draw       = 0x140114000; // vtable[6] (CitizenComponent::OnAdd)
    // vtable[7] (+0x38) is the removed-from-world broadcast slot that
    // MarkForRemoval (0x1400c2ea0) fires — NOT OnCreate. Ported in
    // code/Game/Component/citizen_onremove.cpp.
    constexpr uintptr_t OnRemove   = 0x140113ca0; // vtable[7]
    constexpr uintptr_t OnCreate   = 0x140114900;
    constexpr uintptr_t Update     = 0x140112e70; // vtable[9]
    constexpr uintptr_t OnUpdate2  = 0x1401140f0; // vtable[11]
}

// ENDGAME B.3.8 CIT: the POD is gone — `CitizenComponent` is a real class now
// (code/Game/Component/CitizenComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/CitizenComponent.h"

