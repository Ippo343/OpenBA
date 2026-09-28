#pragma once
// ============================================================================
// code/Game/Player/RemovalTool.h — ENDGAME B.3.8 TOOL (B38_RULES.md D1-D5)
//
// The demolish/clear drag tool, a REAL C++ class with a compiler vtable.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6, bytes read 2026-09-26):
//   reflected "RemovalTool", parent "DragTool", ti 0x1401c6e48
//   create 0x14002e7a0: MemAlloc(0x40, 8), ctor 0x140104cc0: `??0Internal`;
//     Tool part; DragTool part; stamp 0x140161de0; +0x38 = 0
//   createInPlace 0x14002e7e0: `if (storage) ctor(storage)`
//   dtor 0x140104c70: restamps only, tail-JMP `~Internal`
//   table 0x140161de0, 15 slots:
//     [0]  0x14002e7f0 scalar deleting dtor
//     [1]  0x14002e790 TypeName   [2] 0x14002e780 Type   [3] = parent
//     [4]  0x140104bd0 Startup: find the removal description (+0x38)
//     [5]  0x140104bc0 `RET`      [7] 0x140104bb0 `RET`
//     [6]  = DragTool::OnKey      [9] = DragTool::OnCancel
//     [8]  0x140104050 Draw: the removal preview
//     [10], [11] = Tool's   [12] = DragTool's (cursor 5)
//     [13] 0x1401035e0 CalcPath (keystone_a_removalcalcpath.cpp, reached by
//          address through reg_keystone_a_removal.cpp)
//     [14] 0x140103ef0 CalcCost
//   sizeof 0x40.
//
// RemovalDescription fields used by Draw/CalcCost/CalcPath (from the old POD):
//   +0x38 entity template for preview entities, +0x40 removeRoads (u8),
//   +0x43 removeBuildings (u8), +0x80 hasConstraints (char),
//   +0x98 / +0x9c constrained start / end cell.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Player/dragtool.h"
#include "Object/exe_type.h"

class RemovalTool final : public DragTool {
    DECLARE_EXE_TYPE(RemovalTool)

public:
    RemovalTool() : removalDesc(nullptr) {}                                // 0x140104cc0

    // 0x140104c70: no body
    ~RemovalTool() override = default;                                     // [0]  0x14002e7f0
    void Startup() override;                                               // [4]  0x140104bd0
    void Shutdown() override {}                                            // [5]  0x140104bc0 RET
    void Update(void*, char) override {}                                   // [7]  0x140104bb0 RET
    void Draw(void* context, char active) override;                        // [8]  0x140104050
    bool CalcPath(int startCell, int endCell) override;                    // [13] 0x1401035e0
    int  CalcCost(int startCell, int endCell, uint32_t* outValid,
                  int* outCostB, int* outCostA, int* outLength) override;  // [14] 0x140103ef0

    void* removalDesc;   // +0x38 RemovalDescription* — set by Startup
};

namespace RemovalToolAddr {
    constexpr uintptr_t Vtable = 0x140161de0;   // retail table (documentation)
}

static_assert(sizeof(RemovalTool) == 0x40, "RemovalTool is 0x40 (create 0x14002e7a0 MemAlloc(0x40))");
static_assert(alignof(RemovalTool) == 8, "RemovalTool is 8-aligned");
static_assert(offsetof(RemovalTool, removalDesc) == 0x38, "RemovalTool +0x38 (ctor 0x140104d18)");
