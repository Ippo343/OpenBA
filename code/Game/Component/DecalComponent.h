#pragma once
// ============================================================================
// code/Game/Component/DecalComponent.h — ENDGAME B.3.8 block VIS
//
// `DecalComponent` as a real C++ class with a compiler-generated vtable
// (B38_RULES.md D1-D4). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "DecalComponent", parent "Component", ti 0x1401c7580
//   create 0x1400337c0: MemAlloc(0x38, 8) + ctor 0x14011b8f0
//   ctor   0x14011b8f0: Component ctor, stamp 0x140162fd0, +0x28 = 0,
//                       +0x2c = -1, +0x30 = 1.0f, flags |= 0xc3
//   dtor   0x14011b8b0 (slot 0 0x1400338d0, 83 B: the plain restamp + ~Internal shape)
//   table  0x140162fd0, 16 slots:
//     [0]  0x1400338d0 scalar deleting dtor  [1]/[2] TypeName/Type
//     [3]  0x14011b820 Persist               [4] 0x14011b7d0 Startup (Ghidra "Serialize")
//     [6]  0x14011b560 OnAdded (Ghidra "Draw", AddDecal)
//     [7]  0x14011b510 OnRemove (Ghidra "OnCreate", release the decal slot)
//     [11] 0x14011b810 Slot11 (7 B) PORT GAP [12] 0x14011b760 OnEvent PORT GAP
//     every other slot = Component's
//
// TREE DIVERGENCES kept (behaviour of the tree's table, not retail's):
//   [4] runs the standalone draw-flag bucketing (DecalComponent_SetDrawFlag)
//       instead of retail's 0x14011b7d0 (ported as DecalComponent_Serialize,
//       never installed);
//   [8] retail inherits Component's `RET`; the tree drives the build-once
//       retry from it (the owner is bucketed by [4]). Retiring both is a
//       titled behaviour change of its own.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class DecalComponent final : public Component {
    DECLARE_EXE_TYPE(DecalComponent)

public:
    DecalComponent();                                             // 0x14011b8f0

    // 0x14011b8b0
    ~DecalComponent() override = default;                         // [0]  0x1400338d0
    void Persist(void* stream) override;                          // [3]  0x14011b820
    void Startup() override;                                      // [4]  0x14011b7d0 TREE DIVERGENCE
    void OnAdded() override;                                      // [6]  0x14011b560
    void OnRemove() override;                                     // [7]  0x14011b510
    void Tick(void* time) override;                               // [8]  = Component's in retail; TREE DIVERGENCE
    void Slot11() override;                                       // [11] 0x14011b810 PORT GAP
    void OnEvent(int eventId) override;                           // [12] 0x14011b760 PORT GAP

    // PUBLIC while the ported bodies read them (B38 decision 1).
    uint32_t decalState;   // +0x28 (ctor 0)
    int32_t  decalId;      // +0x2C terrain decal slot (ctor -1 = none)
    float    decalScale;   // +0x30 (ctor 1.0f)
    uint8_t  _pad34[4];    // +0x34
};

static_assert(sizeof(DecalComponent) == 0x38, "DecalComponent is 0x38 (create 0x1400337c0)");
static_assert(alignof(DecalComponent) == 8, "DecalComponent alignment");
static_assert(offsetof(DecalComponent, decalState) == 0x28, "");
static_assert(offsetof(DecalComponent, decalId) == 0x2C, "");
static_assert(offsetof(DecalComponent, decalScale) == 0x30, "");
