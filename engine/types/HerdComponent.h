#pragma once
// HerdComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: HerdComponent_Ctor (0x140122150)
//
// Extends Component (0x28). Has a real Update at vtable[9].
// Manages a herd of animals. Fields include an entity ref and a counter.
// Flags: base | 0x0B
//
// Ctor init pattern:
//   *(u64)(+0x28)=0, *(u64)(+0x38)=0, *(u64)(+0x40)=0,
//   byte(+0x48)=0, *(u32)(+0x4C)=0xFFFFFFFF, *(u64)(+0x50)=0, *(u64)(+0x58)=0
//
// Note: +0x30 is NOT initialized by ctor (gap between 0x28 and 0x38)

#include <cstdint>
#include <cstddef>
#include "Component.h"

// Slot names below were read back off the .rdata table at 0x140163680 (each
// entry confirmed by get_xrefs_to on the function). The earlier labelling here
// was off: 0x140121f90 is Serialize (slot 3), not Shutdown; 0x140121f40 is
// OnAdd (slot 6), not Draw; 0x140121e20 is OnRemove (slot 7), not OnCreate.
// 0x1401221e0 is NOT a HerdComponent slot at all — it belongs to the
// HerdDescription vtable that follows this one in .rdata (see herddescription.cpp).
namespace HerdComponentAddr {
    constexpr uintptr_t Ctor       = 0x140122150;
    constexpr uintptr_t Vtable     = 0x140163680;
    constexpr uintptr_t ScalarDtor = 0x140034ea0; // vtable[0]
    constexpr uintptr_t GetTypeId  = 0x140034cb0; // vtable[2]
    constexpr uintptr_t Serialize  = 0x140121f90; // vtable[3]
    constexpr uintptr_t Startup    = 0x140118a50; // vtable[4] (base no-op)
    constexpr uintptr_t OnAdd      = 0x140121f40; // vtable[6]
    constexpr uintptr_t OnRemove   = 0x140121e20; // vtable[7]
    constexpr uintptr_t Update     = 0x140121bf0; // vtable[9]
}

// ENDGAME B.3.8 CIT: the POD is gone — `HerdComponent` is a real class now
// (code/Game/Component/HerdComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/HerdComponent.h"

