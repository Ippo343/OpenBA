#pragma once
// ============================================================================
// code/Game/Dialog/OptionInputBindDialog.h — ENDGAME B.3.8 block DLG
//
// The "press a key" capture box the Options dialog opens to rebind an input.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "OptionInputBindDialog", parent "MessageDialog", ti 0x1401c5398
//   create 0x140012e60 (MemAlloc(0xc8, 8)), createInPlace 0x140012ed0
//   ctor 0x140071d30: DialogInstance(ui, dialog, parent, 0, 0, 0), stamp
//     0x14015c480 (MessageDialog), stamp 0x14015ce40, +0xc0 = 0, +0xc4 = -1
//   table 0x14015ce40, 14 slots: every slot is MessageDialog's (= parent)
//   except [0] 0x140012f80 (dtor), [1]/[2] and [7] 0x140071c80 OnUpdate.
//   The hand-built table carried byte-identical copies of MessageDialog's
//   4/5/8/10-13; they are INHERITED now (B8-6).
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "Dialog/MessageDialog.h"

class OptionInputBindDialog final : public MessageDialog {
    DECLARE_EXE_TYPE(OptionInputBindDialog)

public:
    OptionInputBindDialog();                                        // create 0x140012e60
    OptionInputBindDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                          Object::InternalPtr<UI::DialogInstance> parent);  // 0x140071d30

    Object::Base* DeletingDtor(unsigned flags) override;           // [0] 0x140012f80
    void OnUpdate(const System::Time& time) override;              // [7] 0x140071c80

protected:
    ~OptionInputBindDialog() = default;

public:
    int32_t keyIndex;                                              // +0xc0 captured key / button
    int32_t inputType;                                             // +0xc4 1 keyboard, 2 mouse, -1 none
};

static_assert(offsetof(OptionInputBindDialog, keyIndex) == 0xc0, "OptionInputBindDialog +0xc0");
static_assert(offsetof(OptionInputBindDialog, inputType) == 0xc4, "OptionInputBindDialog +0xc4");
static_assert(sizeof(OptionInputBindDialog) == 0xc8, "OptionInputBindDialog is 0xc8 (create 0x140012e60)");
static_assert(alignof(OptionInputBindDialog) == 8, "OptionInputBindDialog is qword-aligned");
