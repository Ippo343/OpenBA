#pragma once
// ============================================================================
// code/Game/Component/AttendanceComponent.h — ENDGAME B.3.8 block MI-A
//
// `AttendanceComponent` as a real C++ class with a compiler-generated vtable and its
// state controller as an EMBEDDED MEMBER (B38_RULES.md D1-D6; there is no
// multiple inheritance). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §2 and §6, bytes read 2026-09-26):
//   reflected "AttendanceComponent", parent "ControllerComponent", ti 0x1401c3f40
//   create 0x140002670: MemAlloc(0x58, 8) + ctor 0x140044580
//   createInPlace = the imported `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and is kept (B38 B8-13).
//   ctor 0x140044580: Component ctor; +0x28 = +0x30 = 0; member at +0x38;
//     `_flags |= 0xa` (0x8a with Component's 0x80).
//   dtor 0x140044510: MemFree(+0x28) and +0x28 = +0x30 = 0 (the waiting
//     queue), member restamp, ~Internal. [0] 0x140002860 = dtor + MemFree on &1.
//   table 0x140158ce0, 18 slots:
//     [0] 0x140002860 dtor  [1]/[2] TypeName/Type  [3] 0x140044410 Persist
//     [7] 0x1400443d0 OnRemove (leave the town attendance list)
//     [9] 0x140044370 Update  [13] 0x140043f10 CommandMask  [14] 0x140043e80 Command
//     [16] 0x1400026c0 `MOV EAX,2`  [17] 0x1400026b0 `LEA RAX,[RCX+0x38]`
//     The tree's slots 18-21 held AttendanceDescription's table (past retail's
//     end, B38 D3.5): gone.
//     every other slot = ControllerComponent's / Component's.
//   member table 0x140158c90 (9 slots): the table-driven state controller over the
//     class's state table (ControllerLive::Dispatcher<AttendanceCtrlTag>), stamped at
//     this+0x38 after the interface 0x14015f070; back-pointer at this+0x50.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"

struct AttendanceCtrlTag {};

class AttendanceComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(AttendanceComponent)

public:
    using StateController = ::ControllerLive::TableController<AttendanceCtrlTag>;

    AttendanceComponent();                                // 0x140044580

    Object::Base* DeletingDtor(unsigned flags) override;          // [0]  0x140002860
    void          Persist(void* stream) override;                 // [3]  0x140044410
    void          OnRemove() override;                            // [7]  0x1400443d0
    void          Update(void* time) override;                    // [9]  0x140044370
    uint32_t      CommandMask(int command) override;              // [13] 0x140043f10
    void          Command(int command) override;                  // [14] 0x140043e80
    int         GetDefaultState() const override { return 2; }  // [16] 0x1400026c0
    Controller* GetController() override { return &_controller; }  // [17] 0x1400026b0

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    void*           field28;       // +0x28 waiting-queue array (InternalBasePtr[])
    void*           field30;       // +0x30 queue count (int) / capacity (int)
    StateController _controller;   // +0x38 the embedded controller (D6)

protected:
    ~AttendanceComponent();                               // 0x140044510
};

static_assert(sizeof(AttendanceComponent) == 0x58, "AttendanceComponent is 0x58 (create 0x140002670)");
static_assert(alignof(AttendanceComponent) == 8, "AttendanceComponent alignment");
static_assert(offsetof(AttendanceComponent, _controller) == 0x38,
              "AttendanceComponent's controller member at +0x38 (slot 17 0x1400026b0: LEA RAX,[RCX+0x38])");
static_assert(offsetof(AttendanceComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x50,
              "controller back-pointer at +0x50 (ctor 0x140044580)");
static_assert(offsetof(AttendanceComponent, field28) == 0x28, "AttendanceComponent +0x28 queue");
static_assert(offsetof(AttendanceComponent, field30) == 0x30, "AttendanceComponent +0x30 queue count");
