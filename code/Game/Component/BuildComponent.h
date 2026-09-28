#pragma once
// ============================================================================
// code/Game/Component/BuildComponent.h — ENDGAME B.3.8 block MI-A
//
// `BuildComponent` as a real C++ class with a compiler-generated vtable and its
// state controller as an EMBEDDED MEMBER (B38_RULES.md D1-D6; there is no
// multiple inheritance). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §2 and §6, bytes read 2026-09-26):
//   reflected "BuildComponent", parent "ControllerComponent", ti 0x1401c7008
//   create 0x14002f2b0: MemAlloc(0x78, 8) + ctor 0x140108fc0
//   createInPlace = the imported `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and is kept (B38 B8-13).
//   ctor 0x140108fc0: Component ctor; member at +0x28; +0x48 = 0, +0x4c = 1,
//     +0x50 = 0 (byte), +0x58..+0x70 = 0; `_flags |= 0x83`.
//   dtor 0x140108f30: MemFree(+0x68), +0x68 = +0x70 = 0; MemFree(+0x58),
//     +0x58 = +0x60 = 0; member restamp; ~Internal. [0] 0x14002f4d0 = dtor + MemFree.
//   table 0x140162260, 18 slots:
//     [0] 0x14002f4d0 dtor  [1]/[2] TypeName/Type  [3] 0x140108d80 Persist
//     [4] 0x140108d60 `RET 0`  [6] 0x140108d40 OnAdded  [7] 0x140108cb0 OnRemove
//     [11] 0x140108d70 `RET 0`  [15] 0x140108710 OnMessage (labour credit)
//     [16] 0x14002f2f0 `MOV EAX,1`  [17] 0x140004080 `LEA RAX,[RCX+0x28]`
//     every other slot = ControllerComponent's / Component's.
//   member table 0x140162210 (9 slots): the table-driven state controller over the
//     class's state table (ControllerLive::Dispatcher<BuildCtrlTag>), stamped at
//     this+0x28 after the interface 0x14015f070; back-pointer at this+0x40.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"

struct BuildCtrlTag {};

class BuildComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(BuildComponent)

public:
    using StateController = ::ControllerLive::TableController<BuildCtrlTag>;

    BuildComponent();                                     // 0x140108fc0

    // 0x140108f30
    ~BuildComponent() override;                                   // [0]  0x14002f4d0
    void          Persist(void* stream) override;                 // [3]  0x140108d80
    void          Startup() override {}                           // [4]  0x140108d60 `RET 0` (own)
    void          OnAdded() override;                             // [6]  0x140108d40
    void          OnRemove() override;                            // [7]  0x140108cb0
    void          Slot11() override {}                            // [11] 0x140108d70 `RET 0` (own)
    void          OnMessage(int message, void* data,
                            uint32_t arg0, uint32_t arg1) override;  // [15] 0x140108710
    int         GetDefaultState() const override { return 1; }  // [16] 0x14002f2f0
    Controller* GetController() override { return &_controller; }  // [17] 0x140004080

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    StateController _controller;   // +0x28 the embedded controller (D6)
    uint32_t        buildState;    // +0x48 labour-progress accumulator
    uint32_t        buildProgress; // +0x4c cost multiplier (retail ctor: 1)
    uint8_t         buildFlag;     // +0x50 (serialized)
    uint8_t         _pad51[7];     // +0x51
    void*           field58;       // +0x58 mesh list (InternalBasePtr[])
    void*           field60;       // +0x60 mesh list count / capacity
    void*           field68;       // +0x68 picking list (InternalBasePtr[])
    void*           field70;       // +0x70 picking list count / capacity
};

static_assert(sizeof(BuildComponent) == 0x78, "BuildComponent is 0x78 (create 0x14002f2b0)");
static_assert(alignof(BuildComponent) == 8, "BuildComponent alignment");
static_assert(offsetof(BuildComponent, _controller) == 0x28,
              "BuildComponent's controller member at +0x28 (slot 17 0x140004080: LEA RAX,[RCX+0x28])");
static_assert(offsetof(BuildComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x40,
              "controller back-pointer at +0x40 (ctor 0x140108fc0)");
static_assert(offsetof(BuildComponent, buildState) == 0x48, "BuildComponent +0x48");
static_assert(offsetof(BuildComponent, buildProgress) == 0x4c, "BuildComponent +0x4c");
static_assert(offsetof(BuildComponent, buildFlag) == 0x50, "BuildComponent +0x50");
static_assert(offsetof(BuildComponent, field58) == 0x58, "BuildComponent +0x58");
static_assert(offsetof(BuildComponent, field68) == 0x68, "BuildComponent +0x68");
static_assert(offsetof(BuildComponent, field70) == 0x70, "BuildComponent +0x70");
