#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialHighlightDialog.h — ENDGAME B.3.8 block GOAL
//
// The glow ring + bobbing arrow a TutorialSetupDialog opens over the UI
// element (or world entity) the current tutorial step wants clicked.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialHighlightDialog", parent "UI::DialogInstance",
//   ti 0x1401c6550, create 0x1400278c0 (MemAlloc(0xf0, 8) + the default ctor
//   0x1400e0290), createInPlace 0x140027900
//   ctors 0x1400e0290 (default) / 0x1400e01d0 (the one TutorialSetupDialog's
//   open-highlight helper 0x1400e2700 calls); dtor body 0x1400e01a0 (stamp,
//   ~DialogInstance)
//   table 0x1401609e0, 10 slots:
//     [0] 0x140027910 scalar deleting dtor: stamp, ~DialogInstance, MemFree on &1
//     [1] TypeName  [2] Type
//     [3] DialogInstance::Persist (import) — inherited
//     [4] 0x1400e0020 OnOpened: cache glowGroup / glow / arrow
//     [5] DialogInstance::OnClosed (import) — inherited
//     [6] 0x1400dffe0 OnUpdateLayout: place the ring and the arrow
//     [7] 0x1400e0000 OnUpdate: accumulate the elapsed time
//     [8] 0x14003aa1a thunk -> DialogInstance::OnElementEvent — inherited
//     [9] DialogInstance::OnSerializeSettings (import) — inherited
//   sizeof 0xf0.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "runtime/types/ui/DialogInstance.h"
#include "Object/exe_type.h"

class TutorialHighlightDialog final : public UI::DialogInstance {
    DECLARE_EXE_TYPE(TutorialHighlightDialog)

public:
    TutorialHighlightDialog();                                     // 0x1400e0290
    // 0x1400e01d0. Retail passes `parent`, `targetElement` and `targetEntity`
    // by address (R9 and two stack slots) and copies the handles; the base
    // gets (ui, dialog, parent, dataSource 0, userData 0, userFlags 0).
    TutorialHighlightDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                            Object::InternalPtr<UI::DialogInstance> parent,
                            void* targetElement, int32_t rotation, void* targetEntity);

    Object::Base* DeletingDtor(unsigned flags) override;           // [0] 0x140027910
    // [3] Persist: DialogInstance's, inherited (B38 D3.1).
    void OnOpened() override;                                      // [4] 0x1400e0020
    // [5] OnClosed: DialogInstance's, inherited.
    void OnUpdateLayout() override;                                // [6] 0x1400dffe0
    void OnUpdate(const System::Time& time) override;              // [7] 0x1400e0000
    // [8] OnElementEvent, [9] OnSerializeSettings: DialogInstance's, inherited.

protected:
    ~TutorialHighlightDialog() = default;                          // 0x1400e01a0: restamp only

private:
    void PlaceOnElement();                                         // 0x1400dfac0
    void PlaceOnEntity();                                          // 0x1400dfda0

public:
    int64_t  elapsedTicks;       // +0xB8: System::Time — accumulated by OnUpdate
    void*    glowGroup;          // +0xC0: InternalPtr<UI::Element> "glowGroup"
    void*    glow;               // +0xC8: InternalPtr<UI::ImageElement> "glow"
    void*    arrow;              // +0xD0: InternalPtr<UI::ImageElement> "arrow"
    int32_t  rotation;           // +0xD8: arrow rotation, degrees (0/90/180/270)
    uint8_t  _padDC[4];          // +0xDC: padding
    void*    targetElement;      // +0xE0: InternalPtr<UI::Element> (element-path branch)
    void*    targetEntity;       // +0xE8: entity handle (toolbar-cell branch)
};

static_assert(offsetof(TutorialHighlightDialog, elapsedTicks) == 0xB8, "+0xB8");
static_assert(offsetof(TutorialHighlightDialog, glowGroup) == 0xC0, "+0xC0");
static_assert(offsetof(TutorialHighlightDialog, glow) == 0xC8, "+0xC8");
static_assert(offsetof(TutorialHighlightDialog, arrow) == 0xD0, "+0xD0");
static_assert(offsetof(TutorialHighlightDialog, rotation) == 0xD8, "+0xD8");
static_assert(offsetof(TutorialHighlightDialog, targetElement) == 0xE0, "+0xE0");
static_assert(offsetof(TutorialHighlightDialog, targetEntity) == 0xE8, "+0xE8");
static_assert(sizeof(TutorialHighlightDialog) == 0xF0, "TutorialHighlightDialog is 0xf0 (create 0x1400278c0)");
static_assert(alignof(TutorialHighlightDialog) == 8, "qword-aligned");
