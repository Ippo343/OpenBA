#pragma once
// ============================================================================
// code/Game/Component/TrackerComponent.h — ENDGAME B.3.8 block VIS
//
// `TrackerComponent` as a real C++ class with a compiler-generated vtable
// (B38_RULES.md D1-D4). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "TrackerComponent", parent "Component", ti 0x1401c4b10
//   create 0x14000d7a0: MemAlloc(0x50, 8) + the INLINED ctor: Component ctor,
//                       stamp 0x14015ab80, +0x28/+0x30/+0x38/+0x40 = 0,
//                       +0x48 = -1 (no flag OR: flags stay Component's 0x80)
//   dtor   0x140059280: MemFree(+0x38), +0x38 = +0x40 = 0, MemFree(+0x28),
//                       +0x28 = +0x30 = 0, restamp, ~Internal
//   table  0x14015ab80, 16 slots:
//     [0] 0x14000d9a0 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140059130 Persist                every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class TrackerComponent final : public Component {
    DECLARE_EXE_TYPE(TrackerComponent)

public:
    TrackerComponent();                                           // inlined in 0x14000d7a0

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x14000d9a0
    void Persist(void* stream) override;                          // [3] 0x140059130

    // PUBLIC while the ported bodies read them (B38 decision 1).
    void*    field28;      // +0x28 array data (freed by the retail dtor)
    void*    field30;      // +0x30 its count / capacity
    void*    field38;      // +0x38 array data (freed by the retail dtor)
    void*    field40;      // +0x40 its count / capacity
    int32_t  trackerId;    // +0x48 game-world item-count generation stamp (ctor -1)
    uint8_t  _pad4C[4];    // +0x4C

protected:
    // PORT GAP (retail 0x140059280) — B38 D2: retail frees both arrays; the
    // tree never did (Inert_ScalarDtor). Kept as the tree's.
    ~TrackerComponent() = default;
};

static_assert(sizeof(TrackerComponent) == 0x50, "TrackerComponent is 0x50 (create 0x14000d7a0)");
static_assert(alignof(TrackerComponent) == 8, "TrackerComponent alignment");
static_assert(offsetof(TrackerComponent, field28) == 0x28, "");
static_assert(offsetof(TrackerComponent, field30) == 0x30, "");
static_assert(offsetof(TrackerComponent, field38) == 0x38, "");
static_assert(offsetof(TrackerComponent, field40) == 0x40, "");
static_assert(offsetof(TrackerComponent, trackerId) == 0x48, "");
