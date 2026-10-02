#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialSetupRestrictPlayer.h — ENDGAME B.3.8 block GOAL
//
// A tutorial setup that restricts the player (tool, camera bounds) for a step.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialSetupRestrictPlayer", parent "TutorialSetup", ti 0x1401c6780
//   create 0x140028b40 (MemAlloc(0x68, 8)), createInPlace 0x140028b80
//   ctor 0x1400e3220
//   table 0x140160e20, 12 slots:
//     [0] 0x140028bb0 scalar deleting dtor: stamp, ~String +0x50, ~String
//         +0x38, stamp TutorialSetup, ~External, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400e31b0 GetFileVersion `MOV EAX,4`
//     [4] External::Compile (inherited)  [5] 0x1400e3090 Serialize
//     [6] 0x140028b90 Reconstruct  [7] 0x140001640 Destruct
//     [8] 0x1400e2f20 Begin (apply the restriction)
//     [9] 0x1400e2f10 Update `RET 0`
//     [10] 0x1400e2e90 End (clear the restriction)
//     [11] 0x140028b80 Construct
//   sizeof 0x68.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "Tutorial/TutorialSetup.h"
#include "Object/exe_type.h"
#include "Goal/goal_exe_helpers.h"   // NamedExternalRef

class TutorialSetupRestrictPlayer final : public TutorialSetup {
    DECLARE_EXE_TYPE(TutorialSetupRestrictPlayer)

public:
    TutorialSetupRestrictPlayer();                                 // 0x1400e3220

    ~TutorialSetupRestrictPlayer() override;                       // [0] 0x140028bb0
    int32_t           GetFileVersion() const override { return 4; }// [3] 0x1400e31b0
    bool              Serialize(void* stream) override;            // [5] 0x1400e3090
    Object::External* Reconstruct() override;                      // [6] 0x140028b90
    void              Destruct() override;                         // [7] 0x140001640
    void              Begin() override;                            // [8] 0x1400e2f20
    void              Update(void* /*time*/) override {}           // [9] 0x1400e2f10 RET
    void              End() override;                              // [10] 0x1400e2e90
    virtual Object::External* Construct();                         // [11] 0x140028b80

public:
    int32_t  restrictMinX;       // +0x20: cell X min (-1 = unrestricted)
    int32_t  restrictMinChunkRow;// +0x24: chunk row for min pos
    int32_t  restrictMaxX;       // +0x28: cell X max (-1 = unrestricted)
    int32_t  restrictMaxChunkCol;// +0x2C: chunk col for max pos
    int32_t  restrictToolFlags;  // +0x30: tool restriction bitmask (0 = none)
    uint8_t  _pad34[4];          // +0x34: padding
    // +0x38 / +0x50: the tool and camera restriction templates, by name (String,
    //   generation -1 = unresolved, cached External*; resolved by 0x14001b1d0)
    NamedExternalRef toolRef;    // +0x38
    NamedExternalRef cameraRef;  // +0x50
};

static_assert(offsetof(TutorialSetupRestrictPlayer, restrictMinX) == 0x20, "+0x20");
static_assert(offsetof(TutorialSetupRestrictPlayer, restrictToolFlags) == 0x30, "+0x30");
static_assert(offsetof(TutorialSetupRestrictPlayer, toolRef) == 0x38, "+0x38");
static_assert(offsetof(TutorialSetupRestrictPlayer, toolRef) + offsetof(NamedExternalRef, generation) == 0x40, "+0x40");
static_assert(offsetof(TutorialSetupRestrictPlayer, toolRef) + offsetof(NamedExternalRef, resolved) == 0x48, "+0x48");
static_assert(offsetof(TutorialSetupRestrictPlayer, cameraRef) == 0x50, "+0x50");
static_assert(offsetof(TutorialSetupRestrictPlayer, cameraRef) + offsetof(NamedExternalRef, generation) == 0x58, "+0x58");
static_assert(offsetof(TutorialSetupRestrictPlayer, cameraRef) + offsetof(NamedExternalRef, resolved) == 0x60, "+0x60");
static_assert(sizeof(TutorialSetupRestrictPlayer) == 0x68, "TutorialSetupRestrictPlayer is 0x68 (create 0x140028b40)");
static_assert(alignof(TutorialSetupRestrictPlayer) == 8, "qword-aligned");
