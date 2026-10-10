#pragma once
// ============================================================================
// src/game/Component/AutoPickupComponent.h — ENDGAME B.3.8 block VIS
//
// `AutoPickupComponent` as a real C++ class with a compiler-generated vtable
// (B38_RULES.md D1-D4). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "AutoPickupComponent", parent "Component", ti 0x1401c3fb0
//   create 0x1400033c0: MemAlloc(0x40, 8) + ctor 0x140044ae0
//   ctor   0x140044ae0: Component ctor, stamp 0x140158df0, +0x28 = 1 (byte),
//                       +0x30 = +0x38 = 0, flags |= 0x80 (no change)
//   dtor   0x140044a80: restamp, MemFree(+0x30), +0x30 = +0x38 = 0 (~Array of
//                       `field30`, 0x140044aa0..0x140044ab0), restamp, ~Internal
//   table  0x140158df0, 16 slots:
//     [0]  0x140003590 scalar deleting dtor  [1]/[2] TypeName/Type
//     [3]  0x1400449d0 Persist               [4] 0x140044990 Startup
//     [13] 0x140044970 CommandMask PORT GAP  [14] 0x140044930 Command PORT GAP
//     every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"
#include "collection/Collection.h"   // Collection::Array (field30)

class AutoPickupComponent final : public Component {
    DECLARE_EXE_TYPE(AutoPickupComponent)

public:
    AutoPickupComponent();                                        // 0x140044ae0

    // 0x140044a80: the body is empty; ~field30 (MemFree, then +0x30 = +0x38 = 0) and
    // ~Component follow, the order retail's dtor runs them.
    ~AutoPickupComponent() override = default;                    // [0]  0x140003590
    void     Persist(IO::Stream& stream) override;                      // [3]  0x1400449d0
    void     Startup() override;                                  // [4]  0x140044990
    uint32_t CommandMask(int command) override;                   // [13] 0x140044970 PORT GAP
    void     Command(int command) override;                       // [14] 0x140044930 PORT GAP

    // 0x140044810: append `value` to field30 while enabled (action_base_rawmat.cpp).
    void     Append(uint32_t value);

    // PUBLIC while the ported bodies read them (B38 decision 1).
    bool     isEnabled;    // +0x28 (retail ctor 1; the tree's zero is kept, PORT GAP)
    uint8_t  _pad29[7];    // +0x29
    // +0x30 Array<int32> (size +0x38, capacity +0x3c; Persist 0x1400449d0 serializes it).
    // The trait-derived row of int32_t is exe 0x1400021d0's: stride 4, align 4, the
    // grown tail left as-found, no element destroy.
    Collection::Array<int32_t> field30;
};

static_assert(sizeof(AutoPickupComponent) == 0x40, "AutoPickupComponent is 0x40 (create 0x1400033c0)");
static_assert(offsetof(AutoPickupComponent, _pad29) == 0x29, "AutoPickupComponent::_pad29");
static_assert(alignof(AutoPickupComponent) == 8, "AutoPickupComponent alignment");
static_assert(offsetof(AutoPickupComponent, isEnabled) == 0x28, "");
static_assert(offsetof(AutoPickupComponent, field30) == 0x30, "");
static_assert(offsetof(AutoPickupComponent, field30) + offsetof(Collection::Array<int32_t>, _size) == 0x38, "");
static_assert(offsetof(AutoPickupComponent, field30) + offsetof(Collection::Array<int32_t>, _capacity) == 0x3c, "");
