#pragma once
// ============================================================================
// code/Game/Component/NaturalResourceComponent.h — ENDGAME B.3.8 block CIT
//
// `NaturalResourceComponent` (trees, rocks, ore: harvestable resource state) as a
// real C++ class with a compiler-generated vtable (B38_RULES.md D1-D4). The
// engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "NaturalResourceComponent", parent "Component", ti 0x1401c7b30
//   create 0x140036550: MemAlloc(0x30, 8), then the ctor INLINED: Component ctor,
//                       stamp 0x140163e10, +0x28 = 1.0f, +0x2c (word) = 0x100
//                       (+0x2c mature = 0, +0x2d harvestable = 1). The ctor
//                       adds NO flag bits (flags stay Component's 0x80).
//                       0x140125dc0 is the same body out of line.
//   dtor   0x140125d80: stamp 0x140163e10, stamp 0x140162920, ~Internal;
//                       scalar 0x1400366b0 (83 B) + MemFree on &1
//   table  0x140163e10, 16 slots:
//     [0] 0x1400366b0 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140125cf0 Persist                [8] 0x140125c40 Tick (163 B)
//     every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class NaturalResourceComponent final : public Component {
    DECLARE_EXE_TYPE(NaturalResourceComponent)

public:
    NaturalResourceComponent();                                   // 0x140125dc0

    // 0x140125d80, no own work
    ~NaturalResourceComponent() override = default;               // [0] 0x1400366b0
    void Persist(void* stream) override;                          // [3] 0x140125cf0
    void Tick(void* time) override;                               // [8] 0x140125c40 PORT GAP

    // PUBLIC while the ported bodies read them (B38 decision 1).
    float    resourceScale; // +0x28 (ctor 1.0f)
    uint16_t resourceFlags; // +0x2C (ctor 0x100: +0x2c mature 0, +0x2d harvestable 1)
    uint8_t  _pad2E[2];     // +0x2E
};

static_assert(sizeof(NaturalResourceComponent) == 0x30, "NaturalResourceComponent is 0x30 (create 0x140036550)");
static_assert(alignof(NaturalResourceComponent) == 8, "NaturalResourceComponent alignment");
static_assert(offsetof(NaturalResourceComponent, resourceScale) == 0x28, "");
static_assert(offsetof(NaturalResourceComponent, resourceFlags) == 0x2C, "");
