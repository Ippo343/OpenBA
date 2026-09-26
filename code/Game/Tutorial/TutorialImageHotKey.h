#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialImageHotKey.h — ENDGAME B.3.8 block GOAL
//
// A tutorial image labelled with the key currently bound to a hot key.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialImageHotKey", parent "TutorialImage", ti 0x1401c65c0
//   create 0x140027d00 (MemAlloc(0x48, 8)), createInPlace 0x140027d40
//   ctor 0x1400e0680
//   table 0x140160ab0, 10 slots:
//     [0] 0x140027d70 scalar deleting dtor: stamp, ~String +0x30, stamp
//         TutorialImage, ~External, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400e0620 GetFileVersion `MOV EAX,0x1001`
//     [4] External::Compile (inherited)  [5] 0x1400e0540 Serialize
//     [6] 0x140027d50 Reconstruct  [7] 0x140001640 Destruct (= parent)
//     [8] 0x140027d40 Construct  [9] 0x1400e0420 CreateElement
//   sizeof 0x48.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "Tutorial/TutorialImage.h"

class TutorialImageHotKey final : public TutorialImage {
    DECLARE_EXE_TYPE(TutorialImageHotKey)

public:
    TutorialImageHotKey();                                         // 0x1400e0680

    Object::Base*     DeletingDtor(unsigned flags) override;       // [0] 0x140027d70
    int32_t           GetFileVersion() const override { return 0x1001; }  // [3] 0x1400e0620
    bool              Serialize(void* stream) override;            // [5] 0x1400e0540
    Object::External* Reconstruct() override;                      // [6] 0x140027d50
    // [7] Destruct: TutorialImage's (the shared 0x140001640 body)
    Object::External* Construct() override;                        // [8] 0x140027d40
    void*             CreateElement(void* ret, void* dialog) override;  // [9] 0x1400e0420

protected:
    ~TutorialImageHotKey();

public:
    uint64_t hotkeyName;         // +0x30: ExternalPtr triple: System::String name
    int32_t  _field38;           // +0x38: ExternalFactory generation (-1)
    uint8_t  _pad3C[4];          // +0x3C: padding
    void*    _field40;           // +0x40: resolved External* (the hot key)
};

static_assert(offsetof(TutorialImageHotKey, hotkeyName) == 0x30, "+0x30");
static_assert(offsetof(TutorialImageHotKey, _field38) == 0x38, "+0x38");
static_assert(offsetof(TutorialImageHotKey, _field40) == 0x40, "+0x40");
static_assert(sizeof(TutorialImageHotKey) == 0x48, "TutorialImageHotKey is 0x48 (create 0x140027d00)");
static_assert(alignof(TutorialImageHotKey) == 8, "qword-aligned");
