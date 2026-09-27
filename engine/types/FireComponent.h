#pragma once
// FireComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: FireComponent_Ctor (0x140049b80)
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140159380
//   +0x28 = 0, +0x30 = 0, +0x38 = 0, +0x40 = 0   (four 8-byte stores)
//   flags |= 0x8   (and, when that bit newly appears and +0x10 != 0,
//                   calls FUN_1400c3470 — the entity update-bucket re-aggregate)
//
// ⚠ +0x28 and +0x2c are TWO SEPARATE int32s, not one pointer. Ground truth from
// three independent bodies:
//   FireComponent_Shutdown  0x140049a90 — `Serialize(ser, (int*)(this+0x28))` and
//                                          `Serialize(ser, (int*)(this+0x2c))`
//   FireComponent_Slot15    0x140049090 — `*(int*)(this+0x28) += 1`
//   FireComponent_Spread    0x140049130 — `*(int*)(this+0x28) - *(int*)(this+0x2c)`
// The ctor zeroes both with a single 8-byte store, which is what made an earlier
// `void* field28` typing look plausible; it is not.
//
// VTABLE (0x140159380, 16 slots — it ENDS at 0x1401593ff, where FireDescription's
// vtable begins at 0x140159400). Identified from the vtable BYTES, not from
// Ghidra's slot names, which are wrong throughout this component family:
//   0  0x140005720  ScalarDtor
//   1  0x1400055d0
//   2  0x1400055c0  GetTypeId
//   3  0x140049a90  Serialize/Shutdown            (FireComponent_Shutdown)
//   4  0x140118a50  RET       (empty base stub)
//   5  0x140118a60  RET
//   6  0x140118820  RET
//   7  0x140118830  RET
//   8  0x140118a70  RET
//   9  0x140048fb0  Update — the per-frame tick   (FireComponent_Update)
//   10 0x140118a90  RET
//   11 0x140118aa0  RET
//   12 0x140118ab0  XOR EAX,EAX ; RET  (return 0)
//   13 0x140118ac0  RET
//   14 0x140118ad0  RET
//   15 0x140049090  water-applied message hook    (FireComponent_Slot15)
// So FireComponent overrides only 0, 1, 2, 3, 9 and 15.

#include <cstdint>
#include <cstddef>
#include "Component.h"

// ENDGAME B.3.8 CIT: the POD is gone — `FireComponent` is a real class now
// (code/Game/Component/FireComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/FireComponent.h"

// ============================================================
// FireDescription field map (the +0x18 description block, size 0x50).
// Offsets ground-truthed from FireDescription_sub_9d60 (ctor defaults) and the
// four FireComponent bodies that read them. Defaults in brackets — the real
// values come from the building's .rsc, so do not reason about pacing from them.
// ============================================================
namespace FireDescriptionField {
    constexpr size_t BurnBase        = 0x20;  // float [1.0]  duration = Time(Scale*300 + Base)
    constexpr size_t BurnScale       = 0x24;  // float [0.0]
    constexpr size_t ParticleEmitter = 0x28;  // int   [1]    ParticleComponent emitter index
    constexpr size_t AudioEmitter    = 0x2c;  // int   [0]    AmbientEmitterComponent index
    constexpr size_t SpreadInterval  = 0x30;  // float [0.25]
    constexpr size_t SpreadChance    = 0x34;  // float [0.5]
    constexpr size_t DangerRadius    = 0x38;  // int   [8]    cells marked as fire-danger
    constexpr size_t BucketsToOut    = 0x3c;  // int   [100]  buckets that fully extinguish
    constexpr size_t MaxFirefighters = 0x40;  // int   [50]   FireWork capacity (+0x48)
    constexpr size_t IgniteChance0   = 0x44;  // float [1.0]  \_ the {rate, weight} pair
    constexpr size_t IgniteChance1   = 0x48;  // float [1.0]  /  FUN_1400a9290 rolls against
}
