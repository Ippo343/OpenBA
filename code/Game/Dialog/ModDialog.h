#pragma once
// ============================================================================
// code/Game/Dialog/ModDialog.h — ENDGAME B.3.8 block DLG
//
// The local mod list (main menu / pause menu "Mods" button).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "ModDialog", parent "UI::DialogInstance", ti 0x1401c5280
//   create 0x140012900 (MemAlloc(0xd8, 8)), createInPlace 0x140012970
//   ctors 0x140070030 (the door the tree registers), 0x1400700c0
//   dtor ~ModDialog 0x14006ff00 (Ghidra labels it a ctor; per entry: SpriteSheet, MaterialInstance,
//   ImageBuffer deleting dtors; MemFree the entry array; the two IBPs nulled)
//   table 0x14015c870, 10 slots:
//     [0] 0x140012a30 scalar deleting dtor  [1] TypeName  [2] Type
//     [3] DialogInstance::Persist (import)  [4] 0x14006f150 OnOpened
//     [5] DialogInstance::OnClosed          [6] DialogInstance::OnUpdateLayout
//     [7] DialogInstance::OnUpdate          [8] 0x14006ed40 OnElementEvent
//     [9] DialogInstance::OnSerializeSettings
//   sizeof 0xd8. There is NO secondary vtable: Dialogs.h's old
//   `SecondaryVtable = 0x14015c8c0` is a separately allocated
//   Video::ImageBuffer's table (the per-mod thumbnail), not a second vptr.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "runtime/types/ui/DialogInstance.h"
#include "Object/exe_type.h"

class ModDialog final : public UI::DialogInstance {
    DECLARE_EXE_TYPE(ModDialog)

public:
    ModDialog();                                                    // create 0x140012900
    ModDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
              Object::InternalPtr<UI::DialogInstance> parent,
              uint64_t parentMenu);                                 // 0x140070030

    // 0x14006ff00
    ~ModDialog() override;                                         // [0] 0x140012a30
    void OnOpened() override;                                      // [4] 0x14006f150
    void OnElementEvent(Object::InternalPtr<UI::Element>* element,
                        int32_t a, int32_t b, int32_t c,
                        int32_t d, int32_t e, void* userData) override;  // [8] 0x14006ed40

public:
    uint64_t dataB8;           // +0xB8: entry array data (stride 0x58)
    uint64_t dataC0;           // +0xC0: entry count / capacity
    uint64_t dataC8;           // +0xC8: InternalBasePtr (the browser controller)
    uint64_t dataD0;           // +0xD0: the owning MainMenuDialog (InternalBasePtr)
};

static_assert(offsetof(ModDialog, dataB8) == 0xB8, "ModDialog +0xb8");
static_assert(offsetof(ModDialog, dataD0) == 0xD0, "ModDialog +0xd0");
static_assert(sizeof(ModDialog) == 0xD8, "ModDialog is 0xd8 (create 0x140012900)");
static_assert(alignof(ModDialog) == 8, "ModDialog is qword-aligned");
