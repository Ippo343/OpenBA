#pragma once
// ============================================================================
// code/Game/Component/BoatComponent.h — ENDGAME B.3.8 block MI-A
//
// `BoatComponent` as a real C++ class with a compiler-generated vtable and its
// state controller as an EMBEDDED MEMBER (B38_RULES.md D1-D6; there is no
// multiple inheritance). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §2 and §6, bytes read 2026-09-26):
//   reflected "BoatComponent", parent "ControllerComponent", ti 0x1401c4020
//   create 0x1400036e0: MemAlloc(0x98, 8) + ctor 0x140046800
//   createInPlace = the imported `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and is kept (B38 B8-13).
//   ctor 0x140046800: Component ctor; +0x28..+0x40 = 0, +0x48 = 0, +0x50 = 0,
//     +0x58 = 0, +0x60 = +0x68 = 0 (route array), +0x70 = -1; member at +0x78.
//     No flag OR (0x80).
//   dtor 0x140046770: MemFree(+0x60) and +0x60 = +0x68 = 0 (the route
//     waypoint array), member restamp, ~Internal. [0] 0x140003970 = dtor + MemFree.
//   table 0x140158f40, 18 slots:
//     [0] 0x140003970 dtor  [1]/[2] TypeName/Type  [3] 0x1400465d0 Persist
//     [16] 0x140003730 `XOR EAX,EAX`  [17] 0x140003720 `LEA RAX,[RCX+0x78]`
//     every other slot = ControllerComponent's / Component's.
//   member table 0x140158ef0 (9 slots): the table-driven state controller over the
//     class's state table (ControllerLive::Dispatcher<BoatCtrlTag>), stamped at
//     this+0x78 after the interface 0x14015f070; back-pointer at this+0x90.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"

struct BoatCtrlTag {};

class BoatComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(BoatComponent)

public:
    using StateController = ::ControllerLive::TableController<BoatCtrlTag>;

    BoatComponent();                                      // 0x140046800

    // 0x140046770
    ~BoatComponent() override;                                    // [0]  0x140003970
    void          Persist(void* stream) override;                 // [3]  0x1400465d0
    int         GetDefaultState() const override { return 0; }  // [16] 0x140003730
    Controller* GetController() override { return &_controller; }  // [17] 0x140003720

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    uint64_t        data28;        // +0x28 destination entity (InternalBasePtr)
    uint64_t        data30;        // +0x30 InternalBasePtr
    uint64_t        data38;        // +0x38 InternalBasePtr
    uint64_t        data40;        // +0x40 Vector3 position (x, y) ...
    uint32_t        data48;        // +0x48 ... (z)
    uint8_t         _pad4C[4];     // +0x4c float (serialized)
    uint64_t        data50;        // +0x50 Vector3 heading (x, y) ...
    uint32_t        data58;        // +0x58 ... (z)
    uint8_t         _pad5C[4];     // +0x5c
    uint64_t        data60;        // +0x60 route waypoint array (Vector3[])
    uint64_t        data68;        // +0x68 route count / capacity
    int32_t         data70;        // +0x70 route index (retail ctor: -1)
    uint8_t         _pad74[4];     // +0x74
    StateController _controller;   // +0x78 the embedded controller (D6)
};

static_assert(sizeof(BoatComponent) == 0x98, "BoatComponent is 0x98 (create 0x1400036e0)");
static_assert(alignof(BoatComponent) == 8, "BoatComponent alignment");
static_assert(offsetof(BoatComponent, _controller) == 0x78,
              "BoatComponent's controller member at +0x78 (slot 17 0x140003720: LEA RAX,[RCX+0x78])");
static_assert(offsetof(BoatComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x90,
              "controller back-pointer at +0x90 (ctor 0x140046800)");
static_assert(offsetof(BoatComponent, data28) == 0x28, "BoatComponent +0x28");
static_assert(offsetof(BoatComponent, data48) == 0x48, "BoatComponent +0x48");
static_assert(offsetof(BoatComponent, data50) == 0x50, "BoatComponent +0x50");
static_assert(offsetof(BoatComponent, data60) == 0x60, "BoatComponent +0x60 route array");
static_assert(offsetof(BoatComponent, data70) == 0x70, "BoatComponent +0x70 route index");
