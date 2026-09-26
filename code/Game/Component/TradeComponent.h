#pragma once
// ============================================================================
// code/Game/Component/TradeComponent.h — ENDGAME B.3.8 block MI-B
//
// `TradeComponent` (the trading post: merchant table, buy orders, dock state) as
// a real C++ class with a compiler-generated vtable and its state controller as
// an EMBEDDED MEMBER at +0x88, after the class's own 0x60 bytes (B38_RULES.md
// D1-D6; there is no multiple inheritance). The engine/types POD is a forwarder.
//
// RETAIL (B38_SLOTS.md §2 / §6, bytes read 2026-09-26):
//   reflected "TradeComponent", parent "ControllerComponent", ti 0x1401c4b80
//   create 0x14000db60: MemAlloc(0xa8, 8) + ctor 0x14005c430
//   ctor 0x14005c430: Component ctor; +0x28 = 0; +0x2c = -1; +0x30..+0x58 = 0;
//     +0x60 = 0 (qword), +0x68 = 0 (dword); `MOV byte [RBX+0x71],0`; +0x78 = +0x80
//     = 0; member at +0x88 (0x14015f070, 0x14015ac80), back-pointer +0xa0;
//     `_flags |= 3`.
//   dtor 0x14005c340: member restamp; frees the +0x60 BST (0x14000e320 on both
//     children + MemFree), +0x50, +0x40 and the +0x30 name array (0x14000ef10 +
//     MemFree), zeroing each; restamps; ~Internal. [0] 0x14000dc80 = dtor + MemFree.
//   table 0x14015acd0, 18 slots: [3] 0x14005bd50 Persist, [6] 0x14005c1d0 OnAdded
//     (Ghidra "Draw"), [7] 0x14005c190 OnRemove (Ghidra "OnCreate"), [13]
//     0x14005c140 CommandMask, [14] 0x14005c000 Command, [16] 0x1400026c0
//     `MOV EAX,2`, [17] 0x14000dba0 `LEA RAX,[RCX+0x88]`; every other slot =
//     ControllerComponent's / Component's.
//   member table 0x14015ac80: the table-driven controller over the class's state
//     table (ControllerLive::Dispatcher<TradeCtrlTag>).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"

struct TradeCtrlTag {};

class TradeComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(TradeComponent)

public:
    using StateController = ::ControllerLive::TableController<TradeCtrlTag>;

    TradeComponent();                                                    // 0x14005c430

    Object::Base* DeletingDtor(unsigned flags) override;                // [0]  0x14000dc80
    void          Persist(void* stream) override;                       // [3]  0x14005bd50
    void          OnAdded() override;                                   // [6]  0x14005c1d0
    void          OnRemove() override;                                  // [7]  0x14005c190
    uint32_t      CommandMask(int command) override;                    // [13] 0x14005c140
    void          Command(int command) override;                        // [14] 0x14005c000
    int         GetDefaultState() const override { return 2; }          // [16] 0x1400026c0
    Controller* GetController() override { return &_controller; }       // [17] 0x14000dba0

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    uint32_t tradeState;        // +0x28: 0 from the ctor
    int32_t  tradeId;           // +0x2c: -1 = no merchant selected
    void*    field30;           // +0x30: merchant-name array
    void*    field38;           // +0x38
    void*    field40;           // +0x40
    void*    field48;           // +0x48
    void*    field50;           // +0x50
    void*    field58;           // +0x58
    void*    field60;           // +0x60: buy-order BST root
    uint32_t field68;           // +0x68: its count
    uint8_t  _pad6C[5];         // +0x6c
    uint8_t  tradeFlag;         // +0x71
    uint8_t  _pad72[6];         // +0x72
    void*    field78;           // +0x78
    void*    field80;           // +0x80
    StateController _controller;   // +0x88 the embedded controller (D6)

protected:
    ~TradeComponent();                                                   // 0x14005c340
};

static_assert(sizeof(TradeComponent) == 0xa8, "TradeComponent is 0xa8 (create 0x14000db60)");
static_assert(alignof(TradeComponent) == 8, "TradeComponent alignment");
static_assert(offsetof(TradeComponent, tradeState) == 0x28, "TradeComponent +0x28");
static_assert(offsetof(TradeComponent, tradeId) == 0x2c, "TradeComponent +0x2c (ctor 0x14005c46a)");
static_assert(offsetof(TradeComponent, field30) == 0x30, "TradeComponent +0x30");
static_assert(offsetof(TradeComponent, field58) == 0x58, "TradeComponent +0x58");
static_assert(offsetof(TradeComponent, field60) == 0x60, "TradeComponent +0x60");
static_assert(offsetof(TradeComponent, field68) == 0x68, "TradeComponent +0x68");
static_assert(offsetof(TradeComponent, tradeFlag) == 0x71, "TradeComponent +0x71 (ctor 0x14005c498)");
static_assert(offsetof(TradeComponent, field78) == 0x78, "TradeComponent +0x78");
static_assert(offsetof(TradeComponent, field80) == 0x80, "TradeComponent +0x80");
static_assert(offsetof(TradeComponent, _controller) == 0x88,
              "TradeComponent's controller member at +0x88 (slot 17 0x14000dba0: LEA RAX,[RCX+0x88])");
static_assert(offsetof(TradeComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0xa0,
              "controller back-pointer at +0xa0 (ctor 0x14005c4ce)");
