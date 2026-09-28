#pragma once
// ============================================================================
// code/Game/Player/ToolSetting.h — ENDGAME B.3.8 TOOL (B38_RULES.md D1-D4)
//
// `ToolSetting`, the exe's base of the per-tool persistent settings object
// (the one SetEditMode builds lazily from a tool's slot 11 and hands to the
// tool at +0x18). A REAL C++ class with a compiler vtable.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §1, bytes read 2026-09-26):
//   reflected "ToolSetting", parent "Object::Internal", ti 0x1401c6ef0,
//   create = createInPlace = 0x140003730 (abstract: returns null)
//   table 0x140161ee8, 4 slots, NO pure slot:
//     [0] 0x14002e960 scalar deleting dtor: stamp 0x140161ee8, `~Internal`,
//         `if (flags & 1) MemFree` — ~ToolSetting has no body
//     [1] 0x14002e930 TypeName   [2] 0x14002e920 Type
//     [3] Object::Internal::Persist (import) — INHERITED
//   No fields: the only subclass, RotationSetting, adds its dword at +0x10.
//   The ctor is inlined into RotationSetting's (0x140026d20): `??0Internal`,
//   stamp 0x140161ee8.
//
// The abstract record is DEFINE_EXE_TYPE with the abstract factory (ENDGAME
// B.3.9 RF); slots 1/2 answer it. Only a half-built or half-destroyed object
// ever dispatches there.
// ============================================================================

#include <cstdint>
#include "object/Internal.h"
#include "Object/exe_type.h"   // DECLARE_EXE_TYPE (ENDGAME B.3.9 RF)

class ToolSetting : public Object::Internal {
    DECLARE_EXE_TYPE(ToolSetting)

public:
    // no body in retail
    ~ToolSetting() override = default;                     // [0] 0x14002e960
    // [1] TypeName 0x14002e930 and [2] Type 0x14002e920: DECLARE_EXE_TYPE (retail `MOV RAX,[ti+8]` / `MOV EAX,[ti]`).
    // [3] Persist — Object::Internal::Persist, inherited (retail IMPORT).

protected:
    ToolSetting() : Object::Internal() {}
};

static_assert(sizeof(ToolSetting) == 0x10, "ToolSetting adds no bytes to Object::Internal");
static_assert(alignof(ToolSetting) == 8, "ToolSetting is 8-aligned");
