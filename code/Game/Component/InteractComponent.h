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
    void Persist(void* stream) override;                          // [3] 0x140123340
    void OnAdded() override;                                      // [6] 0x140123110

    // PUBLIC while the ported bodies read them (B38 decision 1).
    void*    field28;      // +0x28 use-location IBP array data (freed by the retail dtor)
    void*    field30;      // +0x30 its count / capacity
};

static_assert(sizeof(InteractComponent) == 0x38, "InteractComponent is 0x38 (create 0x1400351a0)");
static_assert(alignof(InteractComponent) == 8, "InteractComponent alignment");
static_assert(offsetof(InteractComponent, field28) == 0x28, "");
static_assert(offsetof(InteractComponent, field30) == 0x30, "");
