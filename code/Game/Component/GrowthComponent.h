#pragma once
// ============================================================================
// code/Game/Component/GrowthComponent.h — ENDGAME B.3.8 block CIT
//
// `GrowthComponent` (natural-resource growth and its visual scale) as a real C++
// class with a compiler-generated vtable (B38_RULES.md D1-D4). The engine/types
// POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "GrowthComponent", parent "Component", ti 0x1401c77b0
//   create 0x140034b10: MemAlloc(0x50, 8) + ctor 0x1401208f0
//   ctor   0x1401208f0: Component ctor, stamp 0x140163590, +0x28..+0x40 = 0,
//                       +0x48 = +0x4c = 1.0f, flags |= 0xcb
//   dtor   0x140034c50 (scalar, 83 B): restamp, ~Internal, MemFree on &1
//   table  0x140163590, 16 slots:
//     [0] 0x140034c50 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x1401207c0 Persist                [4] 0x1401206f0 Startup
//     [6] 0x140120690 OnAdded (its own `RET`)
//     [7] 0x140120560 OnRemove (283 B)       [9] 0x140120050 Update
//     [10] 0x14011fea0 PostUpdate (visual scale)
//     [11] 0x1401206a0 Slot11 (64 B)         every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class GrowthComponent final : public Component {
    DECLARE_EXE_TYPE(GrowthComponent)

public:
    GrowthComponent();                                            // 0x1401208f0

    Object::Base* DeletingDtor(unsigned flags) override;          // [0]  0x140034c50
    void Persist(void* stream) override;                          // [3]  0x1401207c0
    void Startup() override;                                      // [4]  0x1401206f0
    void OnAdded() override;                                      // [6]  0x140120690 (RET)
    void OnRemove() override;                                     // [7]  0x140120560 PORT GAP
    void Update(void* time) override;                             // [9]  0x140120050
    void PostUpdate() override;                                   // [10] 0x14011fea0
    void Slot11() override;                                       // [11] 0x1401206a0 PORT GAP

    // PUBLIC while the ported bodies read them (B38 decision 1).
    void*    field28;      // +0x28 resource sub-component
    void*    field30;      // +0x30 model sibling
    void*    field38;      // +0x38 config
    void*    field40;      // +0x40
    float    growthRate;   // +0x48 (retail ctor 1.0f)
    float    growthScale;  // +0x4C (retail ctor 1.0f)

protected:
    ~GrowthComponent() = default;                                 // no own work
};

static_assert(sizeof(GrowthComponent) == 0x50, "GrowthComponent is 0x50 (create 0x140034b10)");
static_assert(alignof(GrowthComponent) == 8, "GrowthComponent alignment");
static_assert(offsetof(GrowthComponent, field28)     == 0x28, "");
static_assert(offsetof(GrowthComponent, field30)     == 0x30, "");
static_assert(offsetof(GrowthComponent, field38)     == 0x38, "");
static_assert(offsetof(GrowthComponent, field40)     == 0x40, "");
static_assert(offsetof(GrowthComponent, growthRate)  == 0x48, "");
static_assert(offsetof(GrowthComponent, growthScale) == 0x4C, "");
