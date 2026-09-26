#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialImageToolbarIcon.h — ENDGAME B.3.8 block GOAL
//
// A tutorial image showing a toolbar button's icon.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialImageToolbarIcon", parent "TutorialImage", ti 0x1401c6630
//   create 0x140028020 (MemAlloc(0x48, 8)), createInPlace 0x140028060
//   ctor 0x1400e0c30
//   table 0x140160b90, 10 slots:
//     [0] 0x140028090 scalar deleting dtor: stamp, ~String +0x30, stamp
//         TutorialImage, ~External, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400e0bd0 GetFileVersion `MOV EAX,0x1001`
//     [4] External::Compile (inherited)  [5] 0x1400e0af0 Serialize
//     [6] 0x140028070 Reconstruct  [7] 0x140001640 Destruct (= parent)
//     [8] 0x140028060 Construct  [9] 0x1400e0940 CreateElement
//   sizeof 0x48.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "Tutorial/TutorialImage.h"

class TutorialImageToolbarIcon final : public TutorialImage {
    DECLARE_EXE_TYPE(TutorialImageToolbarIcon)

public:
    TutorialImageToolbarIcon();                                    // 0x1400e0c30

    Object::Base*     DeletingDtor(unsigned flags) override;       // [0] 0x140028090
    int32_t           GetFileVersion() const override { return 0x1001; }  // [3] 0x1400e0bd0
    bool              Serialize(void* stream) override;            // [5] 0x1400e0af0
    Object::External* Reconstruct() override;                      // [6] 0x140028070
    // [7] Destruct: TutorialImage's (the shared 0x140001640 body)
    Object::External* Construct() override;                        // [8] 0x140028060
    void*             CreateElement(void* ret, void* dialog) override;  // [9] 0x1400e0940

protected:
    ~TutorialImageToolbarIcon();

public:
    void*    _toolbarString;     // +0x30: ExternalPtr triple: System::String (toolbar name)
    int32_t  _toolbarStringTag;  // +0x38: ExternalFactory generation (-1)
    uint8_t  _pad3C[4];          // +0x3C: padding
    void*    _toolbarRef;        // +0x40: resolved Toolbar*
};

static_assert(offsetof(TutorialImageToolbarIcon, _toolbarString) == 0x30, "+0x30");
static_assert(offsetof(TutorialImageToolbarIcon, _toolbarStringTag) == 0x38, "+0x38");
static_assert(offsetof(TutorialImageToolbarIcon, _toolbarRef) == 0x40, "+0x40");
static_assert(sizeof(TutorialImageToolbarIcon) == 0x48, "TutorialImageToolbarIcon is 0x48 (create 0x140028020)");
static_assert(alignof(TutorialImageToolbarIcon) == 8, "qword-aligned");
