#pragma once
// ============================================================================
// code/Game/Player/PlacementTool.h — ENDGAME B.3.8 TOOL (B38_RULES.md D1-D5)
//
// The single-building placement tool (ghost mesh follows the cursor, left
// click places), a REAL C++ class with a compiler vtable.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6, bytes read 2026-09-26):
//   reflected "PlacementTool", parent "Tool", ti 0x1401c6e10
//   create 0x14002dc40: MemAlloc(0x70, 8), ctor 0x1400fe850: `??0Internal`;
//     Tool part; stamp 0x140161d20; +0x28..+0x48 = 0; qword +0x50 = -1;
//     qword +0x58 = -1; dword +0x60 = -1; dword +0x64 = 0; +0x68 = 0
//   createInPlace 0x14002dc80: `if (storage) ctor(storage)`
//   dtor 0x1400fe810: restamps only, tail-JMP `~Internal`
//   table 0x140161d20, 13 slots:
//     [0]  0x14002dc90 scalar deleting dtor
//     [1]  0x14002dc30 TypeName   [2] 0x14002dc20 Type   [3] = parent
//     [4]  0x1400fe580 Startup: find the 4 descriptions, RotationSetting, mesh
//     [5]  0x1400fe4e0 Shutdown: scene objects, mesh, dialog
//     [6]  0x1400fe2a0 OnKey: rotate / place
//     [7]  0x1400fdee0 Update: the placement preview
//     [8]  0x1400fdcc0 Draw: the footprint overlay
//     [9]  0x1400fe4d0 `XOR AL,AL; RET` (right-click: no drag to cancel)
//     [10] = Tool's (false)
//     [11] 0x140026920 `*ret = TypeId(RotationSetting)` (shared with FencedTool)
//     [12] 0x1400fe800 `MOV EAX,5; RET` (cursor)
//   sizeof 0x70.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Player/Tool.h"
#include "Object/exe_type.h"

class PlacementTool final : public Tool {
    DECLARE_EXE_TYPE(PlacementTool)

public:
    PlacementTool()                                                        // 0x1400fe850
        : descComponent(nullptr), mapComponent(nullptr), createComponent(nullptr),
          radiusComponent(nullptr), rotationIBP(0), lastStartCol(-1), lastStartRow(-1),
          lastCenterCell(-1), radiusDisplayIdx(-1), sceneObjectIdx(-1), meshVariant(0),
          meshInstance(nullptr) {}

    // 0x1400fe810: no body
    ~PlacementTool() override = default;                                   // [0]  0x14002dc90
    void Startup() override;                                               // [4]  0x1400fe580
    void Shutdown() override;                                              // [5]  0x1400fe4e0
    char OnKey(char active) override;                                      // [6]  0x1400fe2a0
    void Update(void* time, char active) override;                         // [7]  0x1400fdee0
    void Draw(void* context, char active) override;                        // [8]  0x1400fdcc0
    char OnCancel() override { return 0; }                                 // [9]  0x1400fe4d0
    Object::Type* GetSettingType(Object::Type* ret) const override;        // [11] 0x140026920
    int  GetCursor() const override { return 5; }                          // [12] 0x1400fe800

    void*    descComponent;     // +0x28 CreatePlacedDescription (DAT_1401c7468)
    void*    mapComponent;      // +0x30 MapDescription (DAT_1401c7a18)
    void*    createComponent;   // +0x38 ModelDescription (mesh; Startup derefs +0x28 UNGUARDED)
    void*    radiusComponent;   // +0x40 RadiusDescription (DAT_1401c4870)
    uint64_t rotationIBP;       // +0x48 InternalBasePtr<RotationSetting>
    int32_t  lastStartCol;      // +0x50 tick-sound change detection
    int32_t  lastStartRow;      // +0x54
    int32_t  lastCenterCell;    // +0x58 radius display
    int32_t  radiusDisplayIdx;  // +0x5c -1 = none
    int32_t  sceneObjectIdx;    // +0x60 Graphics::Scene object index, -1 = none
    int32_t  meshVariant;       // +0x64 mesh variant / rotation index
    void*    meshInstance;      // +0x68 Graphics::MeshInstance* (preview)
};


static_assert(sizeof(PlacementTool) == 0x70, "PlacementTool is 0x70 (create 0x14002dc40 MemAlloc(0x70))");
static_assert(alignof(PlacementTool) == 8, "PlacementTool is 8-aligned");
static_assert(offsetof(PlacementTool, descComponent)    == 0x28, "PlacementTool +0x28 (ctor 0x1400fe88b)");
static_assert(offsetof(PlacementTool, rotationIBP)      == 0x48, "PlacementTool +0x48");
static_assert(offsetof(PlacementTool, lastStartCol)     == 0x50, "PlacementTool +0x50 qword -1 (0x1400fe89f)");
static_assert(offsetof(PlacementTool, lastCenterCell)   == 0x58, "PlacementTool +0x58 qword -1 (0x1400fe8a7)");
static_assert(offsetof(PlacementTool, sceneObjectIdx)   == 0x60, "PlacementTool +0x60 = -1 (0x1400fe8af)");
static_assert(offsetof(PlacementTool, meshVariant)      == 0x64, "PlacementTool +0x64 = 0");
static_assert(offsetof(PlacementTool, meshInstance)     == 0x68, "PlacementTool +0x68 = 0");
