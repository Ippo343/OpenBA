#pragma once
// ============================================================================
// code/Game/Dialog/LoaderDialog.h — ENDGAME B.3.8 block DLG
//
// The splash / loading-screen dialog with the spinning image.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "LoaderDialog", parent "UI::DialogInstance", ti 0x1401c5be8
//   create 0x140020bb0 (MemAlloc(0xc8, 8)), createInPlace 0x140020c20
//   ctor 0x140099070 (from the splash loop), 0x1400b2df0
//   table 0x14015f300, 10 slots:
//     [0] 0x140020c70 scalar deleting dtor  [1] TypeName  [2] Type
//     [3] DialogInstance::Persist (import)  [4] 0x1400b2d90 OnOpened
//     [5] DialogInstance::OnClosed          [6] 0x1400b2d00 OnUpdateLayout
//     [7] 0x1400b2d50 OnUpdate              [8] 0x14003aa1a -> DialogInstance::OnElementEvent
//     [9] DialogInstance::OnSerializeSettings
//   sizeof 0xc8.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "runtime/types/ui/DialogInstance.h"
#include "Object/exe_type.h"

class LoaderDialog final : public UI::DialogInstance {
    DECLARE_EXE_TYPE(LoaderDialog)

public:
    LoaderDialog();                                                 // create 0x140020bb0
    LoaderDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                 Object::InternalPtr<UI::DialogInstance> parent);   // 0x140099070

    ~LoaderDialog() override = default;                            // [0] 0x140020c70
    void OnOpened() override;                                      // [4] 0x1400b2d90
    // [5] OnClosed / [8] OnElementEvent: DialogInstance's, inherited (B38 D3.1).
    void OnUpdateLayout() override;                                // [6] 0x1400b2d00
    void OnUpdate(const System::Time& time) override;              // [7] 0x1400b2d50

public:
    Object::InternalPtr<UI::Element> loadingImage;   // +0xB8: "loadingImage" (an ImageElement)
    float    rotation;         // +0xC0: spinner angle, degrees
    uint8_t  _padC4[4];        // +0xC4
};

static_assert(offsetof(LoaderDialog, loadingImage) == 0xB8, "LoaderDialog +0xb8");
static_assert(offsetof(LoaderDialog, rotation) == 0xC0, "LoaderDialog +0xc0");
static_assert(sizeof(LoaderDialog) == 0xC8, "LoaderDialog is 0xc8 (create 0x140020bb0)");
static_assert(alignof(LoaderDialog) == 8, "LoaderDialog is qword-aligned");
