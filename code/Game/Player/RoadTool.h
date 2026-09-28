#pragma once
// ============================================================================
// code/Game/Player/RoadTool.h — ENDGAME B.3.8 TOOL (B38_RULES.md D1-D5)
//
// The road drag tool, a REAL C++ class with a compiler vtable.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6, bytes read 2026-09-26):
//   reflected "RoadTool", parent "DragTool", ti 0x1401c6e80
//   create 0x14002e870: MemAlloc(0x48, 8), ctor 0x1401057d0: `??0Internal`;
//     Tool part; DragTool part; stamp 0x140161e70; +0x38 = 0; byte +0x40 = 0
//   createInPlace 0x14002e8b0: `if (storage) ctor(storage)`
//   dtor 0x140105780: restamps only, tail-JMP `~Internal`
//   table 0x140161e70, 15 slots:
//     [0]  0x14002e8c0 scalar deleting dtor
//     [1]  0x14002e860 TypeName   [2] 0x14002e850 Type   [3] = parent
//     [4]  0x1401056e0 Startup: find the CreateRoadDescription (+0x38)
//     [5]  0x1401056d0 `RET`      [7] 0x140105680 `RET`
//     [6]  0x140105690 OnKey: DIRECT call to DragTool::OnKey (0x1400fc6b0),
//          then +0x40 = (KeyMap::GetValue(0, 0x16) == 0)
//     [8]  0x140105190 Draw: the road preview
//     [9]  = DragTool::OnCancel   [10], [11] = Tool's   [12] = DragTool's (5)
//     [13] 0x140104d30 CalcPath (roadtool_calcpath.cpp)
//     [14] 0x140104fe0 CalcCost
//   sizeof 0x48.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Player/dragtool.h"
#include "Object/exe_type.h"

class RoadTool final : public DragTool {
    DECLARE_EXE_TYPE(RoadTool)

public:
    RoadTool() : roadDesc(nullptr), modifierFlag(0), _pad41{} {}           // 0x1401057d0

    // 0x140105780: no body
    ~RoadTool() override = default;                                        // [0]  0x14002e8c0
    void Startup() override;                                               // [4]  0x1401056e0
    void Shutdown() override {}                                            // [5]  0x1401056d0 RET
    char OnKey(char active) override;                                      // [6]  0x140105690
    void Update(void*, char) override {}                                   // [7]  0x140105680 RET
    void Draw(void* context, char active) override;                        // [8]  0x140105190
    bool CalcPath(int startCell, int endCell) override;                    // [13] 0x140104d30
    int  CalcCost(int startCell, int endCell, uint32_t* outValid,
                  int* outCostB, int* outCostA, int* outLength) override;  // [14] 0x140104fe0

    void*   roadDesc;       // +0x38 CreateRoadDescription* — set by Startup
    uint8_t modifierFlag;   // +0x40 0 = new road, 1 = upgrade (OnKey, KeyMap 0x16)
    uint8_t _pad41[7];      // +0x41 never written by retail (zeroed here)
};

namespace RoadToolAddr {
    constexpr uintptr_t Vtable = 0x140161e70;   // retail table (documentation)
}

static_assert(sizeof(RoadTool) == 0x48, "RoadTool is 0x48 (create 0x14002e870 MemAlloc(0x48))");
static_assert(alignof(RoadTool) == 8, "RoadTool is 8-aligned");
static_assert(offsetof(RoadTool, roadDesc)     == 0x38, "RoadTool +0x38 (ctor 0x140105828)");
static_assert(offsetof(RoadTool, modifierFlag) == 0x40, "RoadTool +0x40 (ctor 0x14010582c)");
