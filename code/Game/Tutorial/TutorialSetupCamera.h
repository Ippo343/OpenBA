#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialSetupCamera.h — ENDGAME B.3.8 block GOAL
//
// A tutorial setup that moves the camera to a map position when the step opens.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialSetupCamera", parent "TutorialSetup", ti 0x1401c66d8
//   create 0x140028400 (MemAlloc(0x30, 8)), createInPlace 0x140028470
//   ctor 0x1400e2200
//   table 0x140160ca0, 12 slots:
//     [0] 0x140028530 scalar deleting dtor: stamp, stamp TutorialSetup,
//         ~External, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400e21b0 GetFileVersion `MOV EAX,1`
//     [4] External::Compile (inherited)  [5] 0x1400e2120 Serialize
//     [6] 0x1400284d0 Reconstruct  [7] 0x140001640 Destruct
//     [8] 0x1400e20d0 Begin  [9] 0x1400e20c0 Update `RET 0`
//     [10] 0x1400e20b0 End `RET 0`  [11] 0x140028470 Construct
//   sizeof 0x30.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "Tutorial/TutorialSetup.h"
#include "Object/exe_type.h"

class TutorialSetupCamera final : public TutorialSetup {
    DECLARE_EXE_TYPE(TutorialSetupCamera)

public:
    TutorialSetupCamera();                                         // 0x1400e2200

    Object::Base*     DeletingDtor(unsigned flags) override;       // [0] 0x140028530
    int32_t           GetFileVersion() const override { return 1; }// [3] 0x1400e21b0
    bool              Serialize(void* stream) override;            // [5] 0x1400e2120
    Object::External* Reconstruct() override;                      // [6] 0x1400284d0
    void              Destruct() override;                         // [7] 0x140001640
    void              Begin() override;                            // [8] 0x1400e20d0
    void              Update(void* /*time*/) override {}           // [9] 0x1400e20c0 RET
    void              End() override {}                            // [10] 0x1400e20b0 RET
    virtual Object::External* Construct();                         // [11] 0x140028470

protected:
    ~TutorialSetupCamera() = default;

public:
    int32_t  cameraX;            // +0x20: camera cell X (-1 = unset)
    int32_t  cameraChunkRow;     // +0x24: camera chunk row (-1 = unset)
    int32_t  cameraY;            // +0x28: camera cell Y (-1 = unset)
    int32_t  cameraChunkCol;     // +0x2C: camera chunk column (-1 = unset)
};

static_assert(offsetof(TutorialSetupCamera, cameraX) == 0x20, "cameraX offset");
static_assert(offsetof(TutorialSetupCamera, cameraChunkRow) == 0x24, "cameraChunkRow offset");
static_assert(offsetof(TutorialSetupCamera, cameraY) == 0x28, "cameraY offset");
static_assert(offsetof(TutorialSetupCamera, cameraChunkCol) == 0x2C, "cameraChunkCol offset");
static_assert(sizeof(TutorialSetupCamera) == 0x30, "TutorialSetupCamera is 0x30 (create 0x140028400)");
static_assert(alignof(TutorialSetupCamera) == 8, "qword-aligned");
