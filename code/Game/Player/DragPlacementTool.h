#pragma once
// ============================================================================
// code/Game/Player/DragPlacementTool.h — ENDGAME B.3.8 TOOL (B38_RULES.md D1-D5)
//
// The drag-to-size placement tool (crop fields and other area buildings), a
// REAL C++ class with a compiler vtable.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6, bytes read 2026-09-26):
//   reflected "DragPlacementTool", parent "DragTool", ti 0x1401c6d68
//   create 0x14002d8b0: MemAlloc(0x48, 8), ctor 0x1400fc530: `??0Internal`;
//     Tool part; DragTool part; stamp 0x140161b90; +0x38 = +0x40 = 0
//   createInPlace 0x14002d8f0: `if (storage) ctor(storage)`
//   dtor 0x1400fc4e0: restamps only, tail-JMP `~Internal`
//   table 0x140161b90, 15 slots:
//     [0]  0x14002d900 scalar deleting dtor
//     [1]  0x14002d8a0 TypeName   [2] 0x14002d890 Type   [3] = parent
//     [4]  0x1400fc3e0 Startup: find the placement + area descriptions
//     [5]  0x1400fc3d0 `RET`      [7] 0x1400fc3c0 `RET`
//     [6]  = DragTool::OnKey      [9] = DragTool::OnCancel
//     [8]  0x1400fbe40 Draw: the area preview
//     [10], [11] = Tool's   [12] = DragTool's (cursor 5)
//     [13] 0x1400fb900 CalcPath: create the placed entity for the area
//     [14] 0x1400fbae0 CalcCost
//   sizeof 0x48.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Player/dragtool.h"
#include "Object/exe_type.h"

class DragPlacementTool final : public DragTool {
    DECLARE_EXE_TYPE(DragPlacementTool)

public:
    DragPlacementTool() : placementDesc(nullptr), areaDesc(nullptr) {}     // 0x1400fc530

    // 0x1400fc4e0: no body
    ~DragPlacementTool() override = default;                               // [0]  0x14002d900
    void Startup() override;                                               // [4]  0x1400fc3e0
    void Shutdown() override {}                                            // [5]  0x1400fc3d0 RET
    void Update(void*, char) override {}                                   // [7]  0x1400fc3c0 RET
    void Draw(void* context, char active) override;                        // [8]  0x1400fbe40
    bool CalcPath(int startCell, int endCell) override;                    // [13] 0x1400fb900
    int  CalcCost(int startCell, int endCell, uint32_t* outValid,
                  int* outCostA, int* outCostB, int* outLength) override;  // [14] 0x1400fbae0

    void* placementDesc;   // +0x38 CreatePlacedDescription* (DAT_1401c7430) — Startup
    void* areaDesc;        // +0x40 area (Map) description* (DAT_1401c7a18) — Startup
};


static_assert(sizeof(DragPlacementTool) == 0x48, "DragPlacementTool is 0x48 (create 0x14002d8b0 MemAlloc(0x48))");
static_assert(alignof(DragPlacementTool) == 8, "DragPlacementTool is 8-aligned");
static_assert(offsetof(DragPlacementTool, placementDesc) == 0x38, "DragPlacementTool +0x38 (ctor 0x1400fc588)");
static_assert(offsetof(DragPlacementTool, areaDesc)      == 0x40, "DragPlacementTool +0x40 (ctor 0x1400fc58c)");
