#pragma once
// ============================================================================
// code/Game/Component/InteractComponent.h — ENDGAME B.3.8 block VIS
//
// `InteractComponent` as a real C++ class with a compiler-generated vtable
// (B38_RULES.md D1-D4). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "InteractComponent", parent "Component", ti 0x1401c7900
//   create 0x1400351a0: MemAlloc(0x38, 8) + ctor 0x140123480
//   ctor   0x140123480: Component ctor, stamp 0x1401638b0, +0x28 = +0x30 = 0,
//                       flags |= 1 (0x81)
//   dtor   0x140123420: MemFree(+0x28), +0x28 = +0x30 = 0, restamp, ~Internal
//   table  0x1401638b0, 16 slots:
//     [0] 0x1400352f0 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140123340 Persist                [6] 0x140123110 OnAdded (Ghidra "Draw")
//     every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class InteractComponent final : public Component {
    DECLARE_EXE_TYPE(InteractComponent)

public:
    InteractComponent();                                          // 0x140123480

    // PORT GAP (retail 0x140123420) — B38 D2: retail frees the +0x28 array; the
    // tree never did (Inert_ScalarDtor). Kept as the tree's.
    ~InteractComponent() override = default;                      // [0] 0x1400352f0
    void Persist(IO::Stream& stream) override;                          // [3] 0x140123340
    void OnAdded() override;                                      // [6] 0x140123110

    // PUBLIC while the ported bodies read them (B38 decision 1). Types from Persist
    // 0x140123340 (int count +0x30 with the +0x28 IBP array, each validated against
    // InteractComponent 0x1401c7900) and OnAdded 0x140123110.
    InteractComponent** useLocations;       // +0x28 the parts' InteractComponents (Array data, freed by the retail dtor)
    int32_t             useLocationCount;   // +0x30
    int32_t             useLocationCapacity;// +0x34
};

static_assert(sizeof(InteractComponent) == 0x38, "InteractComponent is 0x38 (create 0x1400351a0)");
static_assert(alignof(InteractComponent) == 8, "InteractComponent alignment");
static_assert(offsetof(InteractComponent, useLocations) == 0x28, "");
static_assert(offsetof(InteractComponent, useLocationCount) == 0x30, "");
static_assert(offsetof(InteractComponent, useLocationCapacity) == 0x34, "");
