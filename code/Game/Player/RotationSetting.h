#pragma once
// ============================================================================
// code/Game/Player/RotationSetting.h — ENDGAME B.3.8 TOOL (B38_RULES.md D1-D5)
//
// The rotation a placement/fenced tool keeps between activations (the
// ToolSetting PlacementTool/FencedTool name in their slot 11). A REAL C++
// class with a compiler vtable.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6, bytes read 2026-09-26):
//   reflected "RotationSetting", parent "ToolSetting", ti 0x1401c63c8
//   create 0x140026d20: MemAlloc(0x18, 8); `??0Internal`; stamp 0x140161ee8
//     (ToolSetting); stamp 0x140160710; `+0x10 = 0` (the ctor, inlined)
//   createInPlace 0x140026d80: the same on `storage`
//   dtor 0x1400dea40: stamp 0x140160710, stamp 0x140161ee8, tail-JMP `~Internal`
//   table 0x140160710, 4 slots:
//     [0] 0x140026dd0 scalar deleting dtor (the dtor + MemFree on &1)
//     [1] 0x140026d10 TypeName   [2] 0x140026d00 Type
//     [3] Object::Internal::Persist (import) — INHERITED
//   sizeof 0x18: +0x10 the rotation (0..3), +0x14 padding never written.
//
// The value at +0x10 is read raw by PlacementTool/FencedTool through the IBP
// their Startup resolves; it is PUBLIC for them.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Player/ToolSetting.h"
#include "Object/exe_type.h"

class RotationSetting final : public ToolSetting {
    DECLARE_EXE_TYPE(RotationSetting)

public:
    RotationSetting() : rotation(0), _pad14(0) {}           // inlined into 0x140026d20

    Object::Base* DeletingDtor(unsigned flags) override;   // [0] 0x140026dd0

    uint32_t rotation;   // +0x10 0..3 (the ctor writes 0)
    uint32_t _pad14;     // +0x14 never written by retail (zeroed here)

protected:
    ~RotationSetting() = default;                          // 0x1400dea40: no body
};

static_assert(sizeof(RotationSetting) == 0x18, "RotationSetting is 0x18 (create 0x140026d20 MemAlloc(0x18))");
static_assert(alignof(RotationSetting) == 8, "RotationSetting is 8-aligned");
static_assert(offsetof(RotationSetting, rotation) == 0x10, "RotationSetting +0x10 rotation (0x140026d68)");
