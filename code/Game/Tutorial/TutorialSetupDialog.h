#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialSetupDialog.h — ENDGAME B.3.8 block GOAL
//
// The tutorial setup that greys the UI out except the elements a step names,
// and opens a TutorialHighlightDialog on the one it wants clicked. NOT a
// DialogInstance — an Object::External loaded from Tutorial.rsc.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialSetupDialog", parent "TutorialSetup", ti 0x1401c6710
//   create 0x1400285b0 (MemAlloc(0x78, 8)), createInPlace 0x1400285f0
//   ctor 0x1400e2cc0
//   table 0x140160d20, 12 slots:
//     [0] 0x140028620 scalar deleting dtor: the dtor 0x1400e2bf0 (stamp,
//         ~String +0x58, ~String +0x40, ~Array<String> +0x30 and +0x20,
//         stamp TutorialSetup, ~External), MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400e2be0 GetFileVersion `MOV EAX,6`
//     [4] External::Compile (inherited)  [5] 0x1400e2a60 Serialize
//     [6] 0x140028600 Reconstruct  [7] 0x140001640 Destruct
//     [8] 0x1400e25f0 Begin  [9] 0x1400e24c0 Update  [10] 0x1400e2460 End
//     [11] 0x1400285f0 Construct
//   sizeof 0x78.
//
// The helpers the slots call (show-listed 0x1400e29f0, open-highlight
// 0x1400e2700, set-all-flags 0x1400e2330, find-highlight 0x1400e24f0) are
// ported in tutorial_wave.cpp and reached through engine_funcs' fn_TSD_*.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "Tutorial/TutorialSetup.h"
#include "Object/exe_type.h"
#include "Goal/goal_exe_helpers.h"   // NamedExternalRef
#include "object/Internal.h"         // Object::InternalPtr

namespace System { class String; }
namespace UI { class Dialog; class DialogInstance; class Element; }

class TutorialSetupDialog final : public TutorialSetup {
    DECLARE_EXE_TYPE(TutorialSetupDialog)

public:
    TutorialSetupDialog();                                         // 0x1400e2cc0

    // 0x1400e2bf0
    ~TutorialSetupDialog() override;                               // [0] 0x140028620
    int32_t           GetFileVersion() const override { return 6; }// [3] 0x1400e2be0
    bool              Serialize(IO::Stream& stream) override;            // [5] 0x1400e2a60
    Object::External* Reconstruct() override;                      // [6] 0x140028600
    void              Destruct() override;                         // [7] 0x140001640
    void              Begin() override;                            // [8] 0x1400e25f0
    void              Update(void* time) override;                 // [9] 0x1400e24c0
    void              End() override;                              // [10] 0x1400e2460
    virtual Object::External* Construct();                         // [11] 0x1400285f0

    // ---- non-virtual members (tutorial_wave.cpp) ----
    // 0x1400e2250: show / hide an element tree (retail passes this through unread).
    void SetElementFlagsRecursive(Object::InternalPtr<UI::Element>* elem, char show);
    void SetAllDialogFlags(char show);                             // 0x1400e2330
    Object::InternalPtr<UI::DialogInstance> FindHighlightDialog(); // 0x1400e24f0
    void OpenHighlightDialog(char checkExisting);                  // 0x1400e2700
    void ShowListedElements();                                     // 0x1400e29f0

    // The +0x40 String (H5 raw storage). Defined in tutorialsetupdialog.cpp.
    System::String& HighlightPath();

public:
    // Collection::Array<System::String>: element paths ("Dialog.Element") left
    // visible while everything else is hidden.
    System::String* shownPaths;  // +0x20: Array<String> data (H5 raw triple)
    uint32_t shownPathCount;     // +0x28: size
    uint32_t shownPathCapacity;  // +0x2C: capacity
    // Collection::Array<System::String>: dialog names Begin() walks.
    System::String* dialogNames; // +0x30: Array<String> data (H5 raw triple)
    uint32_t dialogNameCount;    // +0x38: size
    uint32_t dialogNameCapacity; // +0x3C: capacity
    uint64_t highlightPath;      // +0x40: System::String — the element to highlight
    int32_t  arrowRotation;      // +0x48: degrees, lands at highlight +0xD8
    uint8_t  _pad4C[4];          // +0x4C: padding
    const UI::Dialog* highlightDialog; // +0x50: ExternalPtr<UI::Dialog> — the highlight .rsc
    // +0x58..+0x6f: the toolbar template, by name: String, factory generation
    //   (-1 = unresolved), cached External* (resolved by 0x14001b1d0)
    NamedExternalRef toolbarRef;
    int32_t  toolbarIndex;       // +0x70: the toolbar cell to point at
    uint8_t  _pad74[4];          // +0x74: padding
};

static_assert(offsetof(TutorialSetupDialog, shownPaths) == 0x20, "+0x20");
static_assert(offsetof(TutorialSetupDialog, shownPathCount) == 0x28, "+0x28");
static_assert(offsetof(TutorialSetupDialog, dialogNames) == 0x30, "+0x30");
static_assert(offsetof(TutorialSetupDialog, dialogNameCount) == 0x38, "+0x38");
static_assert(offsetof(TutorialSetupDialog, highlightPath) == 0x40, "+0x40");
static_assert(offsetof(TutorialSetupDialog, arrowRotation) == 0x48, "+0x48");
static_assert(offsetof(TutorialSetupDialog, highlightDialog) == 0x50, "+0x50");
static_assert(offsetof(TutorialSetupDialog, toolbarRef) == 0x58, "+0x58");
static_assert(offsetof(TutorialSetupDialog, toolbarRef) + offsetof(NamedExternalRef, generation) == 0x60, "+0x60");
static_assert(offsetof(TutorialSetupDialog, toolbarRef) + offsetof(NamedExternalRef, resolved) == 0x68, "+0x68");
static_assert(offsetof(TutorialSetupDialog, toolbarIndex) == 0x70, "+0x70");
static_assert(sizeof(TutorialSetupDialog) == 0x78, "TutorialSetupDialog is 0x78 (create 0x1400285b0)");
static_assert(alignof(TutorialSetupDialog) == 8, "qword-aligned");
