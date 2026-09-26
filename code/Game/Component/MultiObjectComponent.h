#pragma once
// ============================================================================
// code/Game/Component/MultiObjectComponent.h — ENDGAME B.3.8 block MI-B
//
// `MultiObjectComponent` (a structure made of several entities: bridge segments,
// tunnel mouths, a cemetery gate, fence posts) as a real C++ class with a
// compiler-generated vtable and its state controller as an EMBEDDED MEMBER
// (B38_RULES.md D1-D6, D9 #8: the tree had none). The engine/types POD is a
// forwarder.
//
// RETAIL (B38_SLOTS.md §2 / §6, bytes read 2026-09-26):
//   reflected "MultiObjectComponent", parent "ControllerComponent", ti 0x1401c7ac0
//   create 0x140035ef0: MemAlloc(0x60, 8) + ctor 0x140125a70
//   ctor 0x140125a70: Component ctor; member at +0x28 (0x14015f070, 0x140163ca0),
//     back-pointer +0x40; +0x48 = +0x50 = 0; `MOV dword [RBX+0x58],1`;
//     `_flags |= 2`.
//   dtor 0x140125a00: MemFree(+0x48), +0x48 = +0x50 = 0 (the child list), restamps,
//     ~Internal. [0] 0x1400360c0 = dtor + MemFree on &1.
//   table 0x140163cf0, 18 slots: [3] 0x140125880 Persist, [7] 0x1401257c0 OnRemove
//     (mark every child entity for removal), [12] 0x140125490 OnEvent (relay the
//     event to every child's components), [16] 0x1400026c0 `MOV EAX,2`, [17]
//     0x140004080 `LEA RAX,[RCX+0x28]`; every other slot = ControllerComponent's.
//   member table 0x140163ca0 (FindState 0x1400362c0 over the table at 0x1401d8a70,
//     count 0x1401d99f0; HasStates 0x140036490 reads the "built" flag 0x1401d99f8):
//     the table-driven controller over the class's state table
//     (ControllerLive::Dispatcher<MultiObjectCtrlTag>), built by
//     MultiObjectDescription_Setup 0x14014f530.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"

struct MultiObjectCtrlTag {};

class MultiObjectComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(MultiObjectComponent)

public:
    using StateController = ::ControllerLive::TableController<MultiObjectCtrlTag>;

    MultiObjectComponent();                                              // 0x140125a70

    Object::Base* DeletingDtor(unsigned flags) override;                // [0]  0x1400360c0
    void          Persist(void* stream) override;                       // [3]  0x140125880
    // PORT GAP (retail 0x1401257c0) — B38 D9: the removal hook (mark every child for
    // removal) is ported as MultiObjectComponent_OnCreate but was never wired in the
    // tree; the inherited RET is what the tree's inert slot did.
    void          OnRemove() override {}                                // [7]  0x1401257c0
    void          OnEvent(int eventId) override;                        // [12] 0x140125490
    int         GetDefaultState() const override { return 2; }          // [16] 0x1400026c0
    Controller* GetController() override { return &_controller; }       // [17] 0x140004080

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    StateController _controller;   // +0x28 the embedded controller (D6)
    void*    field48;              // +0x48 the child-entity list (Array<InternalBasePtr> data)
    void*    field50;              // +0x50 its count (dword) / capacity
    uint32_t multiCount;           // +0x58 1 in retail's ctor
    uint8_t  _pad5C[4];            // +0x5c

protected:
    ~MultiObjectComponent();                                             // 0x140125a00
};

static_assert(sizeof(MultiObjectComponent) == 0x60, "MultiObjectComponent is 0x60 (create 0x140035ef0)");
static_assert(alignof(MultiObjectComponent) == 8, "MultiObjectComponent alignment");
static_assert(offsetof(MultiObjectComponent, _controller) == 0x28,
              "MultiObjectComponent's controller member at +0x28 (slot 17 0x140004080)");
static_assert(offsetof(MultiObjectComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x40,
              "controller back-pointer at +0x40 (ctor 0x140125acc)");
static_assert(offsetof(MultiObjectComponent, field48) == 0x48, "MultiObjectComponent +0x48");
static_assert(offsetof(MultiObjectComponent, field50) == 0x50, "MultiObjectComponent +0x50");
static_assert(offsetof(MultiObjectComponent, multiCount) == 0x58, "MultiObjectComponent +0x58 (ctor 0x140125ad8)");
