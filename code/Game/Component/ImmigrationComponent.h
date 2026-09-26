#pragma once
// ============================================================================
// code/Game/Component/ImmigrationComponent.h — ENDGAME B.3.8 block MI-A
//
// `ImmigrationComponent` as a real C++ class with a compiler-generated vtable and its
// state controller as an EMBEDDED MEMBER (B38_RULES.md D1-D6; there is no
// multiple inheritance). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §2 and §6, bytes read 2026-09-26):
//   reflected "ImmigrationComponent", parent "ControllerComponent", ti 0x1401c4480
//   create 0x140007a20: MemAlloc(0x68, 8) + ctor 0x14004e640
//   createInPlace = the imported `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and is kept (B38 B8-13).
//   ctor 0x14004e640: Component ctor; member at +0x28; +0x48 = 0, +0x50 = 0
//     (dword), +0x58 = +0x60 = 0. No flag OR (0x80).
//   dtor 0x14004e5d0: MemFree(+0x58), +0x58 = +0x60 = 0, member restamp,
//     ~Internal. [0] 0x140007b70 = dtor + MemFree on &1.
//   table 0x140159b30, 18 slots:
//     [0] 0x140007b70 dtor  [1]/[2] TypeName/Type  [3] 0x14004e4b0 Persist
//     [16] 0x1400026c0 `MOV EAX,2`  [17] 0x140004080 `LEA RAX,[RCX+0x28]`
//     every other slot = ControllerComponent's / Component's.
//   member table 0x140159ae0 (9 slots): the table-driven state controller over the
//     class's state table (ControllerLive::Dispatcher<ImmigrationCtrlTag>), stamped at
//     this+0x28 after the interface 0x14015f070; back-pointer at this+0x40.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"

struct ImmigrationCtrlTag {};

class ImmigrationComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(ImmigrationComponent)

public:
    using StateController = ::ControllerLive::TableController<ImmigrationCtrlTag>;

    ImmigrationComponent();                               // 0x14004e640

    Object::Base* DeletingDtor(unsigned flags) override;          // [0]  0x140007b70
    void          Persist(void* stream) override;                 // [3]  0x14004e4b0
    int         GetDefaultState() const override { return 2; }  // [16] 0x1400026c0
    Controller* GetController() override { return &_controller; }  // [17] 0x140004080

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    StateController _controller;   // +0x28 the embedded controller (D6)
    void*           field48;       // +0x48
    uint32_t        field50;       // +0x50
    uint8_t         _pad54[4];     // +0x54
    void*           field58;       // +0x58 array (freed by the dtor)
    void*           field60;       // +0x60 array count / capacity

protected:
    ~ImmigrationComponent();                              // 0x14004e5d0
};

static_assert(sizeof(ImmigrationComponent) == 0x68, "ImmigrationComponent is 0x68 (create 0x140007a20)");
static_assert(alignof(ImmigrationComponent) == 8, "ImmigrationComponent alignment");
static_assert(offsetof(ImmigrationComponent, _controller) == 0x28,
              "ImmigrationComponent's controller member at +0x28 (slot 17 0x140004080: LEA RAX,[RCX+0x28])");
static_assert(offsetof(ImmigrationComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x40,
              "controller back-pointer at +0x40 (ctor 0x14004e640)");
static_assert(offsetof(ImmigrationComponent, field48) == 0x48, "ImmigrationComponent +0x48");
static_assert(offsetof(ImmigrationComponent, field50) == 0x50, "ImmigrationComponent +0x50");
static_assert(offsetof(ImmigrationComponent, field58) == 0x58, "ImmigrationComponent +0x58");
static_assert(offsetof(ImmigrationComponent, field60) == 0x60, "ImmigrationComponent +0x60");
