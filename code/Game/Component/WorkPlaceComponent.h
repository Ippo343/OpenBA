#pragma once
// ============================================================================
// code/Game/Component/WorkPlaceComponent.h — ENDGAME B.3.8 block MI-B
//
// `WorkPlaceComponent` (a building's job roster and its profession-registry key)
// as a real C++ class with a compiler-generated vtable and its state controller
// as an EMBEDDED MEMBER (B38_RULES.md D1-D6). The engine/types POD is a forwarder.
//
// RETAIL (B38_SLOTS.md §2 / §6, bytes read 2026-09-26):
//   reflected "WorkPlaceComponent", parent "ControllerComponent", ti 0x1401c80e0
//   create 0x14003a030: MemAlloc(0x68, 8) + ctor 0x14012f2e0
//   ctor 0x14012f2e0: Component ctor; member at +0x28 (0x14015f070, 0x140164ce0),
//     back-pointer +0x40; +0x48 = 0; `MOV byte [RBX+0x50],1`; +0x58 = +0x60 = 0;
//     `_flags |= 2`.
//   dtor 0x14012f270: MemFree(+0x58), +0x58 = +0x60 = 0 (the roster), restamps,
//     ~Internal. [0] 0x14003a220 = dtor + MemFree on &1.
//   table 0x140164d30, 18 slots: [3] 0x14012f120 Persist, [7] 0x14012f090 OnRemove,
//     [16] 0x140003730 `XOR EAX,EAX` (default state 0), [17] 0x140004080
//     `LEA RAX,[RCX+0x28]`; every other slot = ControllerComponent's / Component's.
//   member table 0x140164ce0: the table-driven controller over the class's state
//     table (ControllerLive::Dispatcher<WPCCtrlTag>).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"

struct WPCCtrlTag {};

class WorkPlaceComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(WorkPlaceComponent)

public:
    using StateController = ::ControllerLive::TableController<WPCCtrlTag>;

    WorkPlaceComponent();                                                // 0x14012f2e0

    // 0x14012f270
    ~WorkPlaceComponent() override;                                     // [0]  0x14003a220
    void          Persist(void* stream) override;                       // [3]  0x14012f120
    void          OnRemove() override;                                  // [7]  0x14012f090
    int         GetDefaultState() const override { return 0; }          // [16] 0x140003730
    Controller* GetController() override { return &_controller; }       // [17] 0x140004080

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    StateController _controller;   // +0x28 the embedded controller (D6)
    void*    workerData;           // +0x48 the profession-registry key (serialized IBP)
    uint8_t  isAccepting;          // +0x50 1 from the ctor
    uint8_t  _pad51[7];            // +0x51
    void*    field58;              // +0x58 the worker roster (Array data)
    void*    field60;              // +0x60 its count / capacity
};

static_assert(sizeof(WorkPlaceComponent) == 0x68, "WorkPlaceComponent is 0x68 (create 0x14003a030)");
static_assert(alignof(WorkPlaceComponent) == 8, "WorkPlaceComponent alignment");
static_assert(offsetof(WorkPlaceComponent, _controller) == 0x28,
              "WorkPlaceComponent's controller member at +0x28 (slot 17 0x140004080)");
static_assert(offsetof(WorkPlaceComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x40,
              "controller back-pointer at +0x40 (ctor 0x14012f33c)");
static_assert(offsetof(WorkPlaceComponent, workerData) == 0x48, "WorkPlaceComponent +0x48");
static_assert(offsetof(WorkPlaceComponent, isAccepting) == 0x50, "WorkPlaceComponent +0x50 (ctor 0x14012f344)");
static_assert(offsetof(WorkPlaceComponent, field58) == 0x58, "WorkPlaceComponent +0x58");
static_assert(offsetof(WorkPlaceComponent, field60) == 0x60, "WorkPlaceComponent +0x60");
