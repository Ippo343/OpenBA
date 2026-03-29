#pragma once
// EditTool — Building editing/moving tool, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: EditTool_Ctor (0x1400fd340), EditTool_Factory (0x14002daa0)
// Inherits: Object::Internal (0x10) -> Tool (0x28) -> EditTool (0x38)
// NOTE: EditTool extends Tool directly, NOT DragTool.
//
// Vtable at 0x140161ca0 (13 slots):
//   +0x00: Dtor (0x14002db00)
//   +0x08: GetTypeInfo (0x14002da90)
//   +0x10: GetTypeId (0x14002da80)
//   +0x18: JMP thunk (0x14003a95a)
//   +0x20: Slot4 (3 bytes RET)
//   +0x28: Slot5 (JMP rel32)
//   +0x30: OnKey (0x1400fd030) — entity selection via picking
//   +0x38: Startup (0x1400fcda0) — initialization/positioning
//   +0x40: Slot8 (3 bytes RET)
//   +0x48: Slot9 (returns false)
//   +0x50: Serialize (0x1400fd290) — compare entity ref
//   +0x58: Slot11
//   +0x60: Slot12 (returns 1)

#include <cstdint>
#include <cstddef>

namespace EditToolAddr {
    constexpr uintptr_t Ctor    = 0x1400fd340;
    constexpr uintptr_t Factory = 0x14002daa0;
    constexpr uintptr_t Vtable  = 0x140161ca0;
}

struct EditTool {
    // --- Tool base (0x28 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161ca0
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    uint8_t  _pad10[8];          // +0x10: zeroed (dialog-related in Tool base)
    void*    _field18;           // +0x18: zeroed
    void*    _field20;           // +0x20: zeroed
    // --- EditTool fields ---
    uint64_t entityRef;          // +0x28: InternalBasePtr (dialog controller IBP, zeroed in ctor)
    uint64_t lastEntityRef;      // +0x30: last picked entity InternalBasePtr (used by Serialize/Slot10)
};

static_assert(sizeof(EditTool) == 0x38, "EditTool size mismatch");
static_assert(offsetof(EditTool, vtable) == 0x00, "");
static_assert(offsetof(EditTool, entityRef) == 0x28, "");
static_assert(offsetof(EditTool, lastEntityRef) == 0x30, "");
