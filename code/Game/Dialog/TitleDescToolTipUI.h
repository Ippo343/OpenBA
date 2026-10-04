#pragma once
// ============================================================================
// code/Game/Dialog/TitleDescToolTipUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "TitleDescToolTipUI", parent
//   "UI::DialogController", table 0x14015d9f0, 11 slots, sizeof 0x30.
//   create 0x1400144c0: MemAlloc(0x30, 8), ??0DialogController, stamp, fields.
//   [0] 0x140014510 scalar deleting dtor (~TitleDescToolTipUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x14007b8a0  RET 0
//   [4] 0x14007b730  OnCreated
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"
#include "system/String.h"

// The record the tooltip's data-source element carries as its user data: two
// consecutive System::Strings. OnCreated 0x14007b730 hands `userData` and
// `userData + 8` to TextElement::SetParameter(0, ...) for "textTitle" and
// "textDesc". Only ever viewed through the element's Object::Base* user-data
// slot, never constructed by the tree (its producer is not ported).
struct TitleDescToolTipData {
    System::String title;   // +0x00
    System::String desc;    // +0x08
};
static_assert(sizeof(TitleDescToolTipData) == 0x10, "two Strings (0x14007b730 reads +0 / +8)");
static_assert(offsetof(TitleDescToolTipData, title) == 0x00, "");
static_assert(offsetof(TitleDescToolTipData, desc) == 0x08, "");

class TitleDescToolTipUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(TitleDescToolTipUI)

public:
    TitleDescToolTipUI() = default;                      // 0x1400144c0 (ctor inlined in the factory)

    // 0x140014510 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~TitleDescToolTipUI() override = default;                     // [0] 0x140014510
    void Persist(IO::Stream&/*stream*/) override {}   // [3] 0x14007b8a0  RET 0 in retail
    void OnCreated() override;   // [4] 0x14007b730

};

static_assert(sizeof(TitleDescToolTipUI) == 0x30, "TitleDescToolTipUI is 0x30 (create 0x1400144c0 MemAlloc(0x30))");
static_assert(alignof(TitleDescToolTipUI) == 8, "qword-aligned (the vptr)");
