#pragma once
// ============================================================================
// code/Game/Component/LivestockComponent.h — ENDGAME B.3.8 block CIT
//
// `LivestockComponent` (a pasture animal's production timer and pasture
// back-ref) as a real C++ class with a compiler-generated vtable (B38_RULES.md
// D1-D4). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "LivestockComponent", parent "Component", ti 0x1401c7970
//   create 0x140035480: MemAlloc(0x38, 8) + ctor 0x140123ee0
//   ctor   0x140123ee0: Component ctor, stamp 0x1401639b0, +0x28 = +0x30 = 0,
//                       flags |= 0x0b
//   dtor   0x140035680 (scalar, 83 B): restamp, ~Internal, MemFree on &1
//   table  0x1401639b0, 16 slots:
//     [0] 0x140035680 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140123e10 Persist                [6] 0x140123c70 OnAdded
//     [7] 0x140123c00 OnRemove               [9] 0x140123b50 Update
//     every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class LivestockComponent final : public Component {
    DECLARE_EXE_TYPE(LivestockComponent)

public:
    LivestockComponent();                                         // 0x140123ee0

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140035680
    void Persist(void* stream) override;                          // [3] 0x140123e10
    void OnAdded() override;                                      // [6] 0x140123c70
    void OnRemove() override;                                     // [7] 0x140123c00
    void Update(void* time) override;                             // [9] 0x140123b50

    // PUBLIC while the ported bodies read them (B38 decision 1).
    uint64_t data28;       // +0x28 assigned PastureComponent back-ref
    uint64_t data30;       // +0x30 System::Time production timer

protected:
    ~LivestockComponent() = default;                              // no own work
};

static_assert(sizeof(LivestockComponent) == 0x38, "LivestockComponent is 0x38 (create 0x140035480)");
static_assert(alignof(LivestockComponent) == 8, "LivestockComponent alignment");
static_assert(offsetof(LivestockComponent, data28) == 0x28, "");
static_assert(offsetof(LivestockComponent, data30) == 0x30, "");
