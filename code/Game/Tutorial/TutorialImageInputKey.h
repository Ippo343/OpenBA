#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialImageInputKey.h — ENDGAME B.3.8 block GOAL
//
// A tutorial image labelled with the key bound to an input id.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialImageInputKey", parent "TutorialImage", ti 0x1401c65f8
//   create 0x140027ea0 (MemAlloc(0x38, 8)), createInPlace 0x140027f00
//   ctor 0x1400e08f0 (+0x30 is NOT written by the ctor)
//   table 0x140160b20, 10 slots:
//     [0] 0x140027fa0 scalar deleting dtor: stamp, stamp TutorialImage,
//         ~External, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400e08a0 GetFileVersion `MOV EAX,0x1001`
//     [4] External::Compile (inherited)  [5] 0x1400e07f0 Serialize
//     [6] 0x140027f50 Reconstruct  [7] 0x140001640 Destruct (= parent)
//     [8] 0x140027f00 Construct  [9] 0x1400e06e0 CreateElement
//   sizeof 0x38.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "Tutorial/TutorialImage.h"

class TutorialImageInputKey final : public TutorialImage {
    DECLARE_EXE_TYPE(TutorialImageInputKey)

public:
    TutorialImageInputKey();                                       // 0x1400e08f0

    ~TutorialImageInputKey() override = default;                   // [0] 0x140027fa0
    int32_t           GetFileVersion() const override { return 0x1001; }  // [3] 0x1400e08a0
    bool              Serialize(void* stream) override;            // [5] 0x1400e07f0
    Object::External* Reconstruct() override;                      // [6] 0x140027f50
    // [7] Destruct: TutorialImage's (the shared 0x140001640 body)
    Object::External* Construct() override;                        // [8] 0x140027f00
    Object::InternalPtr<UI::Element>* CreateElement(Object::InternalPtr<UI::Element>* ret,
                                                    const UI::Dialog* dialog) override;  // [9] 0x1400e06e0

public:
    uint32_t _inputId;           // +0x30: input key id (H2: Serialize(unsigned int&), 0x1400e07f0)
    uint8_t  _pad34[4];          // +0x34: padding
};

static_assert(offsetof(TutorialImageInputKey, _inputId) == 0x30, "+0x30");
static_assert(sizeof(TutorialImageInputKey) == 0x38, "TutorialImageInputKey is 0x38 (create 0x140027ea0)");
static_assert(alignof(TutorialImageInputKey) == 8, "qword-aligned");
