#pragma once
// ============================================================================
// code/Game/Component/ResidenceComponent.h — ENDGAME B.3.8 block MI-A
//
// `ResidenceComponent` as a real C++ class with a compiler-generated vtable and its
// state controller as an EMBEDDED MEMBER (B38_RULES.md D1-D6; there is no
// multiple inheritance). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §2 and §6, bytes read 2026-09-26):
//   reflected "ResidenceComponent", parent "ControllerComponent", ti 0x1401c7cf0
//   create 0x140037440: MemAlloc(0x60, 8) + ctor 0x14012a2a0
//   createInPlace = the imported `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and is kept (B38 B8-13).
//   ctor 0x14012a2a0: Component ctor; member at +0x28; +0x48 = +0x50 = +0x58
//     = 0; `_flags |= 0x82`.
//   dtor 0x14012a230 (inlined in [0] 0x140037610): 0x1400175d0 on +0x50 (free
//     each resident slot's inner array, then the slot array; zero +0x50/+0x58),
//     member restamp, ~Internal.
//   table 0x1401642c0, 18 slots:
//     [0] 0x140037610 dtor  [1]/[2] TypeName/Type  [3] 0x14012a170 Persist
//     [4] 0x14012a120 Startup (size the resident array)  [7] 0x14012a0e0 OnRemove
//     [16] 0x1400026c0 `MOV EAX,2`  [17] 0x140004080 `LEA RAX,[RCX+0x28]`
//     every other slot = ControllerComponent's / Component's.
//   member table 0x140164270 (9 slots): the table-driven state controller over the
//     class's state table (ControllerLive::Dispatcher<ResidenceCtrlTag>), stamped at
//     this+0x28 after the interface 0x14015f070; back-pointer at this+0x40.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"

struct ResidenceCtrlTag {};

class ResidenceComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(ResidenceComponent)

public:
    using StateController = ::ControllerLive::TableController<ResidenceCtrlTag>;

    ResidenceComponent();                                 // 0x14012a2a0

    Object::Base* DeletingDtor(unsigned flags) override;          // [0]  0x140037610
    void          Persist(void* stream) override;                 // [3]  0x14012a170
    void          Startup() override;                             // [4]  0x14012a120
    void          OnRemove() override;                            // [7]  0x14012a0e0
    int         GetDefaultState() const override { return 2; }  // [16] 0x1400026c0
    Controller* GetController() override { return &_controller; }  // [17] 0x140004080

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    StateController _controller;   // +0x28 the embedded controller (D6)
    int64_t         heatRemaining; // +0x48 System::Time countdown (heat tick 0x140129970)
    void*           residentSlots; // +0x50 resident slot array (0x10 stride)
    int32_t         residentSlotCount; // +0x58 slot count (desc capacity +0x20)
    uint8_t         _pad5c[4];     // +0x5c

protected:
    ~ResidenceComponent();                                // 0x14012a230
};

static_assert(sizeof(ResidenceComponent) == 0x60, "ResidenceComponent is 0x60 (create 0x140037440)");
static_assert(alignof(ResidenceComponent) == 8, "ResidenceComponent alignment");
static_assert(offsetof(ResidenceComponent, _controller) == 0x28,
              "ResidenceComponent's controller member at +0x28 (slot 17 0x140004080: LEA RAX,[RCX+0x28])");
static_assert(offsetof(ResidenceComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x40,
              "controller back-pointer at +0x40 (ctor 0x14012a2a0)");
static_assert(offsetof(ResidenceComponent, heatRemaining) == 0x48, "ResidenceComponent +0x48");
static_assert(offsetof(ResidenceComponent, residentSlots) == 0x50, "ResidenceComponent +0x50");
static_assert(offsetof(ResidenceComponent, residentSlotCount) == 0x58, "ResidenceComponent +0x58");
