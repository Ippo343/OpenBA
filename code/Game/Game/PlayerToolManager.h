#pragma once
// ============================================================================
// code/Game/Game/PlayerToolManager.h — ENDGAME B.4 W0-H2
//
// The 0xB0-byte player tool manager at GameUpdateLoop+0x120 (m_toolManager;
// the tree called the member m_entityUI, but an EntityUI is a 0x48-byte
// UI::DialogController). Load builds it with MemAlloc(0xb0, 4) + ctor
// 0x1401025c0; Update calls 0x1401026a0 on it every frame.
//
// CLASS NAME NOT SETTLED (inventory D's name; the profile exe was not open).
// No vtable: a plain struct built in raw memory.
//
// FIELDS (retail ctor 0x1401025c0; 0x1401026a0):
//   +0x28 currentTool  InternalPtr<Tool>: 0x1401026a0 installs one when it is
//                      null (0x140102b60(this, 0)), tests it with IsA(EditTool)
//                      and calls its slots 7 (+0x38) and 12 (+0x60) each frame
//   +0x80 / +0x88      a pending-tool flag and the tool 0x1401026a0 hands to
//                      0x140102b60 when the flag is set
//   +0x70              a Collection::Array sized to 5 by the ctor
//                      (0x14002e000): data +0x70, count +0x78, capacity +0x7C
//   The InternalBasePtrs (+0x00, +0x28) are declared as plain pointers (B.4
//   H5: an InternalBasePtr member's ctor would run). Every other field is a
//   typed guess from the ctor's store; the tree reads +0x30 as the paused
//   flag and +0x34 as the game speed (ctor 1) — not yet confirmed in retail.
// ============================================================================

#include <cstdint>
#include <cstddef>

class PlayerToolManager {
public:
    void*    field_0x00;         // +0x00 InternalBasePtr (ctor null)
    uint64_t field_0x08;         // +0x08 not written by the ctor
    uint64_t field_0x10;         // +0x10 ctor 0
    int32_t  field_0x18;         // +0x18 ctor 0
    uint8_t  _pad1C[4];          // +0x1C
    uint64_t field_0x20;         // +0x20 ctor 0
    void*    currentTool;        // +0x28 InternalPtr<Tool> (ctor null)
    bool     field_0x30;         // +0x30 ctor 0 (the tree: paused)
    uint8_t  _pad31[3];          // +0x31
    int32_t  field_0x34;         // +0x34 ctor 1 (the tree: game speed)
    bool     field_0x38;         // +0x38 ctor 0
    uint8_t  _pad39[3];          // +0x39
    int32_t  field_0x3C;         // +0x3C ctor 0 (one qword store with +0x40)
    int32_t  field_0x40;         // +0x40 ctor 0
    float    field_0x44;         // +0x44 ctor 0.75
    void*    field_0x48;         // +0x48 ctor 0
    void*    field_0x50;         // +0x50 ctor 0
    int32_t  field_0x58;         // +0x58 ctor 0
    uint8_t  _pad5C[4];          // +0x5C
    void*    field_0x60;         // +0x60 ctor 0
    int32_t  field_0x68;         // +0x68 ctor 0
    uint8_t  _pad6C[4];          // +0x6C
    void*    field_0x70;         // +0x70 Array data (sized to 5 by the ctor)
    int32_t  field_0x78;         // +0x78 Array count
    int32_t  field_0x7C;         // +0x7C Array capacity
    bool     field_0x80;         // +0x80 pending-tool flag (ctor 0)
    uint8_t  _pad81[7];          // +0x81
    void*    field_0x88;         // +0x88 pending tool (ctor 0)
    void*    field_0x90;         // +0x90 ctor 0
    int32_t  field_0x98;         // +0x98 ctor -1 (one qword store with +0x9C)
    int32_t  field_0x9C;         // +0x9C ctor -1
    int32_t  field_0xA0;         // +0xA0 ctor -1
    bool     field_0xA4;         // +0xA4 ctor 0
    uint8_t  _padA5[3];          // +0xA5
    int32_t  field_0xA8;         // +0xA8 not written by the ctor
    int32_t  field_0xAC;         // +0xAC not written by the ctor
};

static_assert(sizeof(PlayerToolManager) == 0xB0, "PlayerToolManager is 0xB0 (Load MemAlloc(0xb0, 4))");
static_assert(offsetof(PlayerToolManager, field_0x00) == 0x00, "+0x00");
static_assert(offsetof(PlayerToolManager, field_0x08) == 0x08, "+0x08");
static_assert(offsetof(PlayerToolManager, field_0x10) == 0x10, "+0x10");
static_assert(offsetof(PlayerToolManager, field_0x18) == 0x18, "+0x18");
static_assert(offsetof(PlayerToolManager, _pad1C) == 0x1C, "+0x1C");
static_assert(offsetof(PlayerToolManager, field_0x20) == 0x20, "+0x20");
static_assert(offsetof(PlayerToolManager, currentTool) == 0x28, "PlayerToolManager::currentTool @ +0x28 (0x1401026a0)");
static_assert(offsetof(PlayerToolManager, field_0x30) == 0x30, "+0x30");
static_assert(offsetof(PlayerToolManager, _pad31) == 0x31, "+0x31");
static_assert(offsetof(PlayerToolManager, field_0x34) == 0x34, "+0x34");
static_assert(offsetof(PlayerToolManager, field_0x38) == 0x38, "+0x38");
static_assert(offsetof(PlayerToolManager, _pad39) == 0x39, "+0x39");
static_assert(offsetof(PlayerToolManager, field_0x3C) == 0x3C, "+0x3C");
static_assert(offsetof(PlayerToolManager, field_0x40) == 0x40, "+0x40");
static_assert(offsetof(PlayerToolManager, field_0x44) == 0x44, "+0x44");
static_assert(offsetof(PlayerToolManager, field_0x48) == 0x48, "+0x48");
static_assert(offsetof(PlayerToolManager, field_0x50) == 0x50, "+0x50");
static_assert(offsetof(PlayerToolManager, field_0x58) == 0x58, "+0x58");
static_assert(offsetof(PlayerToolManager, _pad5C) == 0x5C, "+0x5C");
static_assert(offsetof(PlayerToolManager, field_0x60) == 0x60, "+0x60");
static_assert(offsetof(PlayerToolManager, field_0x68) == 0x68, "+0x68");
static_assert(offsetof(PlayerToolManager, _pad6C) == 0x6C, "+0x6C");
static_assert(offsetof(PlayerToolManager, field_0x70) == 0x70, "+0x70");
static_assert(offsetof(PlayerToolManager, field_0x78) == 0x78, "+0x78");
static_assert(offsetof(PlayerToolManager, field_0x7C) == 0x7C, "+0x7C");
static_assert(offsetof(PlayerToolManager, field_0x80) == 0x80, "+0x80");
static_assert(offsetof(PlayerToolManager, _pad81) == 0x81, "+0x81");
static_assert(offsetof(PlayerToolManager, field_0x88) == 0x88, "+0x88");
static_assert(offsetof(PlayerToolManager, field_0x90) == 0x90, "+0x90");
static_assert(offsetof(PlayerToolManager, field_0x98) == 0x98, "+0x98");
static_assert(offsetof(PlayerToolManager, field_0x9C) == 0x9C, "+0x9C");
static_assert(offsetof(PlayerToolManager, field_0xA0) == 0xA0, "+0xA0");
static_assert(offsetof(PlayerToolManager, field_0xA4) == 0xA4, "+0xA4");
static_assert(offsetof(PlayerToolManager, _padA5) == 0xA5, "+0xA5");
static_assert(offsetof(PlayerToolManager, field_0xA8) == 0xA8, "+0xA8");
static_assert(offsetof(PlayerToolManager, field_0xAC) == 0xAC, "+0xAC");
