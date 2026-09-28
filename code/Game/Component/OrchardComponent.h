#pragma once
// ============================================================================
// code/Game/Component/OrchardComponent.h — ENDGAME B.3.8 block MI-B
//
// `OrchardComponent` (an orchard: its tree slots and the plant/tend/harvest/cut
// cycle) as a real C++ class with a compiler-generated vtable and its state
// controller as an EMBEDDED MEMBER (B38_RULES.md D1-D6). The engine/types POD is
// a forwarder.
//
// RETAIL (B38_SLOTS.md §2 / §6, bytes read 2026-09-26):
//   reflected "OrchardComponent", parent "ControllerComponent", ti 0x1401c46b0
//   create 0x140009870: MemAlloc(0x68, 8) + ctor 0x140051f40
//   ctor 0x140051f40: Component ctor; member at +0x28 (0x14015f070, 0x14015a130),
//     back-pointer +0x40; +0x48 = 0 (byte); `MOV dword [RBX+0x4c],-1`; +0x50 =
//     +0x58 = +0x60 = 0; `_flags |= 0x80`.
//   dtor 0x140051ed0: MemFree(+0x50), +0x50 = +0x58 = 0 (the slot array), restamps,
//     ~Internal. [0] 0x140009a70 = dtor + MemFree on &1.
//   table 0x14015a180, 18 slots: [3] 0x140051de0 Persist, [4] 0x140051dd0 Startup
//     (`MOV qword [RCX+0x60],0`), [13] 0x140051c90 CommandMask, [14] 0x140051ba0
//     Command, [15] 0x140051990 OnMessage, [16] 0x1400026c0 `MOV EAX,2`, [17]
//     0x140004080 `LEA RAX,[RCX+0x28]`; every other slot = ControllerComponent's.
//   The tree's slots 18-21 were OrchardDescription's table 0x14015a210 read past
//     this table's end (B38 D3.5): gone.
//   member table 0x14015a130: the table-driven controller over the class's state
//     table (ControllerLive::Dispatcher<OrchardCtrlTag>).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"

struct OrchardCtrlTag {};

class OrchardComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(OrchardComponent)

public:
    using StateController = ::ControllerLive::TableController<OrchardCtrlTag>;

    OrchardComponent();                                                  // 0x140051f40

    // 0x140051ed0
    ~OrchardComponent() override;                                       // [0]  0x140009a70
    void          Persist(void* stream) override;                       // [3]  0x140051de0
    void          Startup() override;                                   // [4]  0x140051dd0
    uint32_t      CommandMask(int command) override;                    // [13] 0x140051c90
    void          Command(int command) override;                        // [14] 0x140051ba0
    void          OnMessage(int message, void* data,
                            uint32_t arg0, uint32_t arg1) override;     // [15] 0x140051990
    int         GetDefaultState() const override { return 2; }          // [16] 0x1400026c0
    Controller* GetController() override { return &_controller; }       // [17] 0x140004080

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    StateController _controller;   // +0x28 the embedded controller (D6)
    uint8_t  orchardFlag;          // +0x48 work enabled (byte)
    uint8_t  _pad49[3];            // +0x49
    int32_t  orchardId;            // +0x4c the last-harvest year (-1 = never; retail ctor)
    void*    field50;              // +0x50 the tree-slot array
    void*    field58;              // +0x58 its count / capacity
    void*    field60;              // +0x60 (Startup clears it)
};

static_assert(sizeof(OrchardComponent) == 0x68, "OrchardComponent is 0x68 (create 0x140009870)");
static_assert(alignof(OrchardComponent) == 8, "OrchardComponent alignment");
static_assert(offsetof(OrchardComponent, _controller) == 0x28,
              "OrchardComponent's controller member at +0x28 (slot 17 0x140004080)");
static_assert(offsetof(OrchardComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x40,
              "controller back-pointer at +0x40 (ctor 0x140051f9c)");
static_assert(offsetof(OrchardComponent, orchardFlag) == 0x48, "OrchardComponent +0x48");
static_assert(offsetof(OrchardComponent, orchardId) == 0x4c, "OrchardComponent +0x4c (ctor 0x140051fa3)");
static_assert(offsetof(OrchardComponent, field50) == 0x50, "OrchardComponent +0x50");
static_assert(offsetof(OrchardComponent, field58) == 0x58, "OrchardComponent +0x58");
static_assert(offsetof(OrchardComponent, field60) == 0x60, "OrchardComponent +0x60");
