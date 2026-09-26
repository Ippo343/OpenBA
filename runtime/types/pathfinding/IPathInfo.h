#pragma once
// ============================================================================
// runtime/types/pathfinding/IPathInfo.h — Pathfinding::IPathInfo, 8 bytes,
//                                          2 pure slots
// ============================================================================
//
// ENDGAME B.3 step 7, block B6 (RULE B7-9): DECLARATIONS ONLY. The class is
// DEAD in retail — `??0IPathInfo@Pathfinding@@QEAA@XZ` (0x18006c970) has no
// code reference in Runtime-x64.dll and the exe imports nothing of it
// (B37_RULES 6.2). Nothing in this tree constructs, calls or links one.
//
// `??_7IPathInfo@Pathfinding@@6B@` @ 0x180158f30, TWO slots (B37_SLOTS.md):
//   [0] +0x00 0x18007ec28  _purecall
//   [1] +0x08 0x18007ec28  _purecall
// NO deleting-destructor slot (B7-1.5): slot 0 is an ordinary pure virtual, so
// this class is the one Object-free interface here without `DeletingDtor`. The
// two slots' names are NOT attested anywhere in the binary (no override is
// exported), so they are spelled by position.
//
//   sizeof 8      `??0IPathInfo` is `LEA RAX,[vtable] ; MOV [RCX],RAX ; RET` —
//                 the vptr and nothing else (B37_SLOTS.md section 1a).
//   parent        none (no `_ParentTypeName`, no base ctor call).
//
// Its only consumer in retail is `Pathfinding::Scene`'s constructor, which
// stores a `const IPathInfo*` at Scene+0 (runtime/types/pathfinding/Scene.h).
// ============================================================================

#include <cstdint>

namespace Pathfinding {

class IPathInfo {
public:
    // Slot signatures are NOT RECOVERABLE (both entries are `_purecall` and no
    // subclass exists in either binary); spelled as the bare shape only.
    virtual void Slot0() = 0;   // [0] 0x18007ec28 _purecall — name not attested
    virtual void Slot1() = 0;   // [1] 0x18007ec28 _purecall — name not attested

    // ---- exported, NOT PORTED, dead in retail: declared, undefined ----------
    IPathInfo(const IPathInfo& src);                    // 0x18006c980
    IPathInfo& operator=(const IPathInfo& src);         // 0x18006c990

protected:
    IPathInfo() = default;      // NOT retail's 0x18006c970 (the vptr store only)
    ~IPathInfo() = default;     // no destructor is exported; none is virtual
};

static_assert(sizeof(IPathInfo) == 8,
              "Pathfinding::IPathInfo is the vptr only (??0IPathInfo 0x18006c970)");
static_assert(alignof(IPathInfo) == 8, "Pathfinding::IPathInfo aligns to 8 (the vptr)");

}  // namespace Pathfinding
