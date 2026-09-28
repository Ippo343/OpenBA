#pragma once
// ============================================================================
// code/Game/Player/BridgeTool.h — ENDGAME B.3.8 TOOL (B38_RULES.md D1-D5)
//
// The bridge placement tool, a REAL C++ class with a compiler vtable.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6, bytes read 2026-09-26):
//   reflected "BridgeTool", parent "DragTool", ti 0x1401c6d30
//   create 0x14002d7a0: MemAlloc(0x40, 8), ctor 0x1400fb890: `??0Internal`;
//     Tool part (+0x18 = +0x20 = 0); DragTool part (word +0x28 = 0, cells -1);
//     stamp 0x140161b00; +0x38 = 0
//   dtor 0x1400fb840: stamp 0x140161b00, 0x140161c10, 0x140161f10, tail-JMP
//     `~Internal` — no other work
//   table 0x140161b00, 15 slots:
//     [0]  0x14002d7f0 scalar deleting dtor (the dtor + MemFree on &1)
//     [1]  0x14002d790 TypeName   [2] 0x14002d780 Type   [3] = parent
//     [4]  0x1400fb7a0 Startup: find the CreateBridgeDescription (+0x38)
//     [5]  0x1400fb790 `RET`      [7] 0x1400fb2c0 `RET`
//     [6]  = DragTool::OnKey 0x1400fc6b0    [9] = DragTool::OnCancel 0x1400fc640
//     [8]  0x1400fadc0 Draw: the red/green drag preview (bridgetool_draw.cpp)
//     [10], [11] = Tool's (false; no setting)   [12] = DragTool's (cursor 5)
//     [13] 0x1400f9cd0 CalcPath: build the segment entities (bridgetool_calcpath.cpp)
//     [14] 0x1400facf0 CalcCost: snap, validate, report the body length
//   sizeof 0x40.
//
// CreateBridgeDescription fields used by CalcPath/CalcCost (from the old POD):
//   +0x20 / +0x24 / +0x2c / +0x30 terrain masks (body / approach / crossing /
//   neighbour), +0x34 neighbour radius, +0x38 segmentTemplates[5] (entity
//   templates for the 0/1/2/4/8-cell segments).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Player/dragtool.h"
#include "Object/exe_type.h"

class BridgeTool final : public DragTool {
    DECLARE_EXE_TYPE(BridgeTool)

public:
    BridgeTool() : bridgeDesc(nullptr) {}                                  // 0x1400fb890

    // 0x1400fb840: no body
    ~BridgeTool() override = default;                                      // [0]  0x14002d7f0
    void Startup() override;                                               // [4]  0x1400fb7a0
    void Shutdown() override {}                                            // [5]  0x1400fb790 RET
    void Update(void*, char) override {}                                   // [7]  0x1400fb2c0 RET
    void Draw(void* context, char active) override;                        // [8]  0x1400fadc0
    bool CalcPath(int startCell, int endCell) override;                    // [13] 0x1400f9cd0
    int  CalcCost(int startCell, int endCell, uint32_t* outValid,
                  int* outCostB, int* outCostA, int* outLength) override;  // [14] 0x1400facf0

    void* bridgeDesc;   // +0x38 CreateBridgeDescription* — set by Startup
};


static_assert(sizeof(BridgeTool) == 0x40, "BridgeTool is 0x40 (create 0x14002d7a0 MemAlloc(0x40))");
static_assert(alignof(BridgeTool) == 8, "BridgeTool is 8-aligned");
static_assert(offsetof(BridgeTool, bridgeDesc) == 0x38, "BridgeTool +0x38 (ctor 0x1400fb8e8)");
