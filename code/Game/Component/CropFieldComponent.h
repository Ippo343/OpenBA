#pragma once
// ============================================================================
// code/Game/Component/CropFieldComponent.h — ENDGAME B.3.8 block MI-B
//
// `CropFieldComponent` (a crop field: its slot grid and the plant/tend/harvest
// cycle) as a real C++ class with a compiler-generated vtable and its state
// controller as an EMBEDDED MEMBER (B38_RULES.md D1-D6). The engine/types POD is
// a forwarder.
//
// RETAIL (B38_SLOTS.md §2 / §6, bytes read 2026-09-26):
//   reflected "CropFieldComponent", parent "ControllerComponent", ti 0x1401c7510
//   create 0x140032ed0: MemAlloc(0x68, 8) + ctor 0x14011afd0
//   ctor 0x14011afd0: Component ctor; member at +0x28 (0x14015f070, 0x140162e70),
//     back-pointer +0x40; `MOV word [RBX+0x48],0x100` (+0x48 = 0, +0x49 = 1);
//     +0x50 = +0x58 = +0x60 = 0; `_flags |= 0x80`.
//   dtor 0x14011af60: MemFree(+0x58), +0x58 = +0x60 = 0 (the slot grid), restamps,
//     ~Internal. [0] 0x1400330a0 = dtor + MemFree on &1.
//   table 0x140162ec0, 18 slots: [3] 0x14011ae70 Persist, [4] 0x14011ae60 Startup
//     (`MOV qword [RCX+0x50],0`), [13] 0x14011ada0 CommandMask, [14] 0x14011acc0
//     Command, [15] 0x14011ab50 OnMessage, [16] 0x1400026c0 `MOV EAX,2`, [17]
//     0x140004080 `LEA RAX,[RCX+0x28]`; every other slot = ControllerComponent's.
//   The tree's slots 18-21 (ControllerLive_ScalarDeletingDtor and the description's
//     type info / id / version) were CropFieldDescription's table 0x140162f50 read
//     past this table's end (B38 D3.5): gone.
//   member table 0x140162e70: the table-driven controller over the class's state
//     table (ControllerLive::Dispatcher<CropFieldCtrlTag>).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"

struct CropFieldCtrlTag {};

class CropFieldComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(CropFieldComponent)

public:
    using StateController = ::ControllerLive::TableController<CropFieldCtrlTag>;

    CropFieldComponent();                                                // 0x14011afd0

    // 0x14011af60
    ~CropFieldComponent() override;                                     // [0]  0x1400330a0
    void          Persist(void* stream) override;                       // [3]  0x14011ae70
    void          Startup() override;                                   // [4]  0x14011ae60
    uint32_t      CommandMask(int command) override;                    // [13] 0x14011ada0
    void          Command(int command) override;                        // [14] 0x14011acc0
    void          OnMessage(int message, void* data,
                            uint32_t arg0, uint32_t arg1) override;     // [15] 0x14011ab50
    int         GetDefaultState() const override { return 2; }          // [16] 0x1400026c0
    Controller* GetController() override { return &_controller; }       // [17] 0x140004080

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    StateController _controller;   // +0x28 the embedded controller (D6)
    uint16_t cropFlags;            // +0x48 byte 0 = work enabled, byte 1 (+0x49) = 1
    uint8_t  _pad4A[6];            // +0x4a
    void*    field50;              // +0x50 the crop description list (Startup clears it)
    void*    field58;              // +0x58 the slot grid (stride 0x20)
    void*    field60;              // +0x60 its count (dword) / capacity
};

static_assert(sizeof(CropFieldComponent) == 0x68, "CropFieldComponent is 0x68 (create 0x140032ed0)");
static_assert(alignof(CropFieldComponent) == 8, "CropFieldComponent alignment");
static_assert(offsetof(CropFieldComponent, _controller) == 0x28,
              "CropFieldComponent's controller member at +0x28 (slot 17 0x140004080)");
static_assert(offsetof(CropFieldComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x40,
              "controller back-pointer at +0x40 (ctor 0x14011b02c)");
static_assert(offsetof(CropFieldComponent, cropFlags) == 0x48, "CropFieldComponent +0x48 (ctor 0x14011b030)");
static_assert(offsetof(CropFieldComponent, field50) == 0x50, "CropFieldComponent +0x50");
static_assert(offsetof(CropFieldComponent, field58) == 0x58, "CropFieldComponent +0x58");
static_assert(offsetof(CropFieldComponent, field60) == 0x60, "CropFieldComponent +0x60");
