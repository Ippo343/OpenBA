#pragma once
// ============================================================================
// src/game/Tutorial/TutorialStep.h — ENDGAME B.3.8 block GOAL (B38_RULES D1-D5)
//
// One tutorial step: its text, images, setups and goals.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialStep", parent "Object::External", ti 0x1401c67b8
//   create 0x140028c50 (MemAlloc(0x70, 8)), createInPlace 0x140028c90
//   ctor 0x1400e3480; dtor body 0x1400e33f0
//   table 0x140160e90, 9 slots:
//     [0] 0x140028cc0 scalar deleting dtor: the dtor 0x1400e33f0 (stamp, free
//         the three arrays, ~String +0x38, ~String +0x30, ~External), MemFree
//         on &1
//     [1] TypeName  [2] Type  [3] 0x1400e33e0 GetFileVersion `MOV EAX,1`
//     [4] Object::External::Compile (import) — inherited
//     [5] 0x1400e32a0 Serialize
//     [6] 0x140028ca0 Reconstruct  [7] 0x140001640 Destruct
//     [8] 0x140028c90 Construct (null test, then the ctor 0x1400e3480)
//   sizeof 0x70.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "object/External.h"
#include "system/String.h"
#include "collection/Collection.h"
#include "Object/exe_type.h"

namespace UI { class StringTable; }
class TutorialImage;
class TutorialSetup;
class TutorialGoal;

class TutorialStep final : public Object::External {
    DECLARE_EXE_TYPE(TutorialStep)

public:
    TutorialStep();                                                // 0x1400e3480

    // 0x1400e33f0
    ~TutorialStep() override;                                      // [0] 0x140028cc0
    int32_t           GetFileVersion() const override { return 1; }// [3] 0x1400e33e0
    bool              Serialize(IO::Stream& stream) override;            // [5] 0x1400e32a0
    Object::External* Reconstruct() override;                      // [6] 0x140028ca0
    void              Destruct() override;                         // [7] 0x140001640
    virtual Object::External* Construct();                         // [8] 0x140028c90

public:
    bool     completed;         // +0x20: step completed flag (Serialize(bool&))
    uint8_t  _pad21[7];         // +0x21: padding
    UI::StringTable* stringTable; // +0x28: ExternalPtr<StringTable>
    // +0x30 .. +0x6F: typed together (behaviour: B4 wave Y). Retail's dtor body
    // 0x1400e33f0 frees +0x60, +0x50, +0x40 (MemFree, then the data / size+capacity
    // qwords zeroed) and runs ~String on +0x38 then +0x30: reverse declaration
    // order, which is what the compiler's member destruction does after the body.
    System::String titleStr;    // +0x30: title text
    System::String descStr;     // +0x38: description text
    Collection::Array<TutorialImage*> images;   // +0x40: Array<ExternalPtr<TutorialImage>>
    Collection::Array<TutorialSetup*> setups;   // +0x50: Array<ExternalPtr<TutorialSetup>>
    Collection::Array<TutorialGoal*>  goals;    // +0x60: Array<ExternalPtr<TutorialGoal>>
};

static_assert(offsetof(TutorialStep, completed) == 0x20, "+0x20");
static_assert(offsetof(TutorialStep, stringTable) == 0x28, "+0x28");
static_assert(offsetof(TutorialStep, titleStr) == 0x30, "+0x30");
static_assert(offsetof(TutorialStep, descStr) == 0x38, "+0x38");
static_assert(offsetof(TutorialStep, images) == 0x40, "+0x40");
static_assert(offsetof(TutorialStep, images._size) == 0x48, "+0x48");
static_assert(offsetof(TutorialStep, setups) == 0x50, "+0x50");
static_assert(offsetof(TutorialStep, setups._size) == 0x58, "+0x58");
static_assert(offsetof(TutorialStep, goals) == 0x60, "+0x60");
static_assert(offsetof(TutorialStep, goals._size) == 0x68, "+0x68");
static_assert(sizeof(TutorialStep) == 0x70, "TutorialStep is 0x70 (create 0x140028c50 MemAlloc(0x70))");
static_assert(offsetof(TutorialStep, _pad21) == 0x21, "TutorialStep::_pad21");
static_assert(alignof(TutorialStep) == 8, "qword-aligned");
