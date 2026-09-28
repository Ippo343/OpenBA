#pragma once
// ============================================================================
// code/Game/Component/NomadComponent.h — ENDGAME B.3.8 block MI-A
//
// `NomadComponent` as a real C++ class with a compiler-generated vtable and its
// state controller as an EMBEDDED MEMBER (B38_RULES.md D1-D6; there is no
// multiple inheritance). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §2 and §6, bytes read 2026-09-26):
//   reflected "NomadComponent", parent "ControllerComponent", ti 0x1401c4640
//   create 0x140009020: MemAlloc(0x78, 8) + ctor 0x14004fdb0
//   createInPlace = the imported `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and is kept (B38 B8-13).
//   ctor 0x14004fdb0: Component ctor; +0x28..+0x48 = 0, +0x50 = 0 (dword);
//     member at +0x58; `_flags |= 2`.
//   dtor 0x14004fd50: restamp only, ~Internal (no own work).
//     [0] 0x140009230 = the dtor inlined + MemFree on &1.
//   table 0x14015a020, 18 slots:
//     [0] 0x140009230 dtor  [1]/[2] TypeName/Type  [3] 0x14004fc40 Persist
//     [7] 0x14004fd20 OnRemove (42 B)
//     [16] 0x140003730 `XOR EAX,EAX`  [17] 0x140009060 `LEA RAX,[RCX+0x58]`
//     every other slot = ControllerComponent's / Component's.
//   member table 0x140159fd0 (9 slots): the table-driven state controller over the
//     class's state table (ControllerLive::Dispatcher<NomadCtrlTag>), stamped at
//     this+0x58 after the interface 0x14015f070; back-pointer at this+0x70.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"

struct NomadCtrlTag {};

class NomadComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(NomadComponent)

public:
    using StateController = ::ControllerLive::TableController<NomadCtrlTag>;

    NomadComponent();                                     // 0x14004fdb0

    // 0x14004fd50
    ~NomadComponent() override;                                   // [0]  0x140009230
    void          Persist(void* stream) override;                 // [3]  0x14004fc40
    void          OnRemove() override;                            // [7]  0x14004fd20
    int         GetDefaultState() const override { return 0; }  // [16] 0x140003730
    Controller* GetController() override { return &_controller; }  // [17] 0x140009060

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    void*           field28;       // +0x28 entity (InternalBasePtr)
    void*           field30;       // +0x30 Vector3 ...
    void*           field38;       // +0x38 ... / Vector3 at +0x3c ...
    void*           field40;       // +0x40 ...
    void*           field48;       // +0x48 System::Time
    uint32_t        field50;       // +0x50
    uint8_t         _pad54[4];     // +0x54
    StateController _controller;   // +0x58 the embedded controller (D6)
};

static_assert(sizeof(NomadComponent) == 0x78, "NomadComponent is 0x78 (create 0x140009020)");
static_assert(alignof(NomadComponent) == 8, "NomadComponent alignment");
static_assert(offsetof(NomadComponent, _controller) == 0x58,
              "NomadComponent's controller member at +0x58 (slot 17 0x140009060: LEA RAX,[RCX+0x58])");
static_assert(offsetof(NomadComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x70,
              "controller back-pointer at +0x70 (ctor 0x14004fdb0)");
static_assert(offsetof(NomadComponent, field28) == 0x28, "NomadComponent +0x28");
static_assert(offsetof(NomadComponent, field48) == 0x48, "NomadComponent +0x48");
static_assert(offsetof(NomadComponent, field50) == 0x50, "NomadComponent +0x50");
