#pragma once
// ============================================================================
// code/Game/Component/DestroyComponent.h — ENDGAME B.3.8 block MI-A
//
// `DestroyComponent` as a real C++ class with a compiler-generated vtable and its
// state controller as an EMBEDDED MEMBER (B38_RULES.md D1-D6; there is no
// multiple inheritance). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §2 and §6, bytes read 2026-09-26):
//   reflected "DestroyComponent", parent "ControllerComponent", ti 0x1401c75f0
//   create 0x140033bc0: MemAlloc(0x68, 8) + ctor 0x14011dc80
//   createInPlace = the imported `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and is kept (B38 B8-13).
//   ctor 0x14011dc80: Component ctor; member at +0x28; +0x48 = 0 (qword),
//     +0x50 = 0 (word), +0x54 = 0 (qword), +0x60 = 0. No flag OR (0x80).
//   dtor 0x14011dc20: restamp only, ~Internal (no own work).
//     [0] 0x140033dd0 = the dtor inlined + MemFree on &1.
//   table 0x140163120, 18 slots:
//     [0] 0x140033dd0 dtor  [1]/[2] TypeName/Type  [3] 0x14011db50 Persist
//     [4] 0x14011db00 Startup (copy the description's default target)
//     [15] 0x14011daf0 OnMessage (demolition-progress credit)
//     [16] 0x140033c00 `MOV EAX,4`  [17] 0x140004080 `LEA RAX,[RCX+0x28]`
//     every other slot = ControllerComponent's / Component's.
//   member table 0x1401630d0 (9 slots): the table-driven state controller over the
//     class's state table (ControllerLive::Dispatcher<DestroyCtrlTag>), stamped at
//     this+0x28 after the interface 0x14015f070; back-pointer at this+0x40.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"

struct DestroyCtrlTag {};

class DestroyComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(DestroyComponent)

public:
    using StateController = ::ControllerLive::TableController<DestroyCtrlTag>;

    DestroyComponent();                                   // 0x14011dc80

    Object::Base* DeletingDtor(unsigned flags) override;          // [0]  0x140033dd0
    void          Persist(void* stream) override;                 // [3]  0x14011db50
    void          Startup() override;                             // [4]  0x14011db00
    void          OnMessage(int message, void* data,
                            uint32_t arg0, uint32_t arg1) override;  // [15] 0x14011daf0
    int         GetDefaultState() const override { return 4; }  // [16] 0x140033c00
    Controller* GetController() override { return &_controller; }  // [17] 0x140004080

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    StateController _controller;   // +0x28 the embedded controller (D6)
    void*           field48;       // +0x48
    uint16_t        destroyFlags;  // +0x50
    uint8_t         _pad52[2];     // +0x52
    uint32_t        field54;       // +0x54 demolition progress (OnMessage credit)
    void*           field58;       // +0x58 demolition target (low dword)
    void*           field60;       // +0x60 target description (Startup)

protected:
    ~DestroyComponent();                                  // 0x14011dc20
};

static_assert(sizeof(DestroyComponent) == 0x68, "DestroyComponent is 0x68 (create 0x140033bc0)");
static_assert(alignof(DestroyComponent) == 8, "DestroyComponent alignment");
static_assert(offsetof(DestroyComponent, _controller) == 0x28,
              "DestroyComponent's controller member at +0x28 (slot 17 0x140004080: LEA RAX,[RCX+0x28])");
static_assert(offsetof(DestroyComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x40,
              "controller back-pointer at +0x40 (ctor 0x14011dc80)");
static_assert(offsetof(DestroyComponent, field48) == 0x48, "DestroyComponent +0x48");
static_assert(offsetof(DestroyComponent, destroyFlags) == 0x50, "DestroyComponent +0x50");
static_assert(offsetof(DestroyComponent, field54) == 0x54, "DestroyComponent +0x54");
static_assert(offsetof(DestroyComponent, field58) == 0x58, "DestroyComponent +0x58");
static_assert(offsetof(DestroyComponent, field60) == 0x60, "DestroyComponent +0x60");
