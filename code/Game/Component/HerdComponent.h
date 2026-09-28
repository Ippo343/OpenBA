#pragma once
// ============================================================================
// code/Game/Component/HerdComponent.h — ENDGAME B.3.8 block CIT
//
// `HerdComponent` (a wild-animal herd: grazing, migration, size) as a real C++
// class with a compiler-generated vtable (B38_RULES.md D1-D4). The engine/types
// POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "HerdComponent", parent "Component", ti 0x1401c7820
//   create 0x140034cd0: MemAlloc(0x60, 8) + ctor 0x140122150
//   ctor   0x140122150: Component ctor, stamp 0x140163680, +0x28 = 0, +0x38 =
//                       +0x40 = 0, +0x48 (byte) = 0, +0x4c = -1, +0x50 = +0x58
//                       = 0, flags |= 0x0b. +0x30 is NOT written.
//   dtor   0x140034ea0 (scalar): restamp, MemFree(+0x38), +0x38 = +0x40 = 0
//                       (the member array), stamp 0x140162920, ~Internal,
//                       MemFree on &1
//   table  0x140163680, 16 slots:
//     [0] 0x140034ea0 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140121f90 Persist                [6] 0x140121f40 OnAdded
//     [7] 0x140121e20 OnRemove               [9] 0x140121bf0 Update
//     every other slot = Component's (slot 4 is the base `RET`)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class HerdComponent final : public Component {
    DECLARE_EXE_TYPE(HerdComponent)

public:
    HerdComponent();                                              // 0x140122150

    // PORT GAP (retail 0x140034ea0) — B38 D2: retail frees the member array at
    // +0x38; the tree never did (Inert_ScalarDtor).
    ~HerdComponent() override = default;                          // [0] 0x140034ea0
    void Persist(void* stream) override;                          // [3] 0x140121f90
    // [4] inherited: Component's `RET` 0x140118a50 (the ctor writes +0x4c = -1).
    void OnAdded() override;                                      // [6] 0x140121f40
    void OnRemove() override;                                     // [7] 0x140121e20 PORT GAP
    void Update(void* time) override;                             // [9] 0x140121bf0

    // PUBLIC while the ported bodies read them (B38 decision 1).
    uint64_t data28;       // +0x28 PopulationComponent back-ref
    uint64_t data30;       // +0x30 not written by the retail ctor
    uint64_t data38;       // +0x38 member array data (freed by the retail dtor)
    uint64_t data40;       // +0x40 member array count / capacity
    uint8_t  data48;       // +0x48 phase (0 grazing, 1 migrating)
    uint8_t  _pad49[3];    // +0x49
    int32_t  data4C;       // +0x4C tile (ctor -1)
    uint64_t data50;       // +0x50 System::Time timer
    uint64_t data58;       // +0x58
};

static_assert(sizeof(HerdComponent) == 0x60, "HerdComponent is 0x60 (create 0x140034cd0)");
static_assert(alignof(HerdComponent) == 8, "HerdComponent alignment");
static_assert(offsetof(HerdComponent, data28) == 0x28, "");
static_assert(offsetof(HerdComponent, data30) == 0x30, "");
static_assert(offsetof(HerdComponent, data38) == 0x38, "");
static_assert(offsetof(HerdComponent, data40) == 0x40, "");
static_assert(offsetof(HerdComponent, data48) == 0x48, "");
static_assert(offsetof(HerdComponent, data4C) == 0x4C, "");
static_assert(offsetof(HerdComponent, data50) == 0x50, "");
static_assert(offsetof(HerdComponent, data58) == 0x58, "");
