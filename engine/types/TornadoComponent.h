#pragma once
// TornadoComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: TornadoComponent_Ctor (0x140058cf0)
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x14015aa80
//   +0x28 = 0, +0x30 = 0, +0x38 = 0 (dword), +0x40 = 0, +0x48 = 0 (qword: count+cap)
//   +0x50 = -1 (int32), +0x58 = 0, +0x60 = 0 (dword)
//   flags |= 0x84   (bit 2 = full-rate update tree -> vtable slot 8 ticks every
//                    frame; bit 7 = the second bucket the entity registers in)
//
// FIELD MEANINGS (2026-08-22 wave). Three names in the first version of this
// header were wrong; they were guessed from the ctor's zero-stores rather than
// from the readers. Corrected against the two functions that actually index this
// block:
//   * 0x1400a70c0 takes `self + 0x30` as its base and reads +0x10 (data),
//     +0x18 (count) and +0x20 (index) off it -> self+0x40 / +0x48 / +0x50.
//     So +0x50 is the PATH INDEX, not a "tornadoId".
//   * 0x140057cd0 (Update) integrates movement into +0x58/+0x5c/+0x60 every
//     frame and feeds it to Audio/Geometry as the world position.
//     So +0x58 is the live POSITION, not a "direction".
//   * 0x140058040 (damage tick) does `self+0x28 -= dt` and reloads it from the
//     description's Time at desc+0x28. So +0x28 is the DAMAGE COUNTDOWN.
//
// +0x40 is the head of an exe dynamic array of Vector3 handled by 0x14000cf40
// (grow+fill) and 0x14000d030 (resize): { void* data; int count; int capacity; }
// laid over +0x40 / +0x48 / +0x4C. The previous header modelled +0x48 as a full
// 8-byte pointer, which merged count and capacity into one field — exactly the
// narrowing that reference_narrowed_8byte_store_desyncs_count_and_cap warns
// about. They are split here.

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace TornadoComponentAddr {
    constexpr uintptr_t Vtable = 0x14015aa80;

    // The three overridden slots. Everything else in the 16-slot vtable is a
    // base-Component no-op (0x140118xxx). Slot indices are from the vtable BYTES
    // at 0x14015aa80, not from Ghidra's names — Ghidra labels slot 3 "_Shutdown",
    // slot 4 "_Serialize" and slot 8 "_OnRemove", and all three are off by one.
    constexpr uintptr_t Serialize = 0x140058b80;  // slot 3 (+0x18)
    constexpr uintptr_t Startup   = 0x1400585a0;  // slot 4 (+0x20)
    constexpr uintptr_t Update    = 0x140057cd0;  // slot 8 (+0x40), every frame
}

// ENDGAME B.3.8 CIT: the POD is gone — `TornadoComponent` is a real class now
// (code/Game/Component/TornadoComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/TornadoComponent.h"

// TornadoDescription (0x38 bytes) — the parameter block read by every function
// above. Offsets verified against the ctor (0x140058e80) and Serialize
// (0x140058d90); meanings verified against the readers.
struct TornadoDescriptionFields {
    // +0x20 float  speed           default 1.0  — metres/sec along the path
    // +0x24 int    minSeparation   default 10   — min endpoint distance (cells)
    // +0x28 Time   damageInterval  default 0    — reload for damageCountdown
    // +0x30 int    damageRadius    default 1    — radius query + square scan
    // +0x34 int    scatterRadius   default 2    — the extra random single strike
    static constexpr uintptr_t kSpeed          = 0x20;
    static constexpr uintptr_t kMinSeparation  = 0x24;
    static constexpr uintptr_t kDamageInterval = 0x28;
    static constexpr uintptr_t kDamageRadius   = 0x30;
    static constexpr uintptr_t kScatterRadius  = 0x34;
};
