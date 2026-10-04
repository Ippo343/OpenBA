// engine/src/ui/titledesctooltipui.cpp
// Clean reimplementation of TitleDescToolTipUI game logic.
//
// TitleDescToolTipUI (0x30 bytes) — tooltip that shows a title and description.
// Extends DialogController. No subclass fields.
//
// Decompiled from Application-x64.exe @ 0x14007b730 (Serialize / vtable +0x20).

#include "Dialog/ui_panels.h"
#include "ui/UserInterface.h"
#include "object/Internal.h"
#include "system/String.h"
#include <cstdint>
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/TextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/DialogInstance.h"

// ============================================================
// TitleDescToolTipUI::OnCreated (vtable +0x20)
// ============================================================
// Finds "textTitle" and "textDesc" in the owning dialog, then reads the data
// source's user data (two consecutive Strings, the title and the description)
// and sets them as each text's parameter 0.
//
// Original: 0x14007b730, 50 instructions.

// [4] 0x14007b730 — the class's only real slot (every other entry of its 11-slot
// table is a shared micro-return).
void TitleDescToolTipUI::OnCreated() {
    // The tree's guards (retail has neither): an unbound controller returns.
    if (!_dialog || !_dataSource)
        return;

    // --- "textTitle" / "textDesc": DialogInstance::FindElement, as retail ---
    // (retail narrows each with IsDerivedFrom against the type id Ghidra labels
    // LabelElement's; the tree does not narrow: known_port_issues 14a.)
    UI::TextElement* titleElement =
        static_cast<UI::TextElement*>(_dialog->FindElement(System::String("textTitle")).Get());
    UI::TextElement* descElement =
        static_cast<UI::TextElement*>(_dialog->FindElement(System::String("textDesc")).Get());

    // --- The data source's user data ---
    // (retail narrows the data source to UI::Element first; the tree takes it
    // as one.)
    Object::Base* dataSource = _dataSource;
    Object::Base* userData = static_cast<UI::Element*>(dataSource)->GetUserData();

    if (userData) {
        // The user data is a TitleDescToolTipData (TitleDescToolTipUI.h) that the
        // tooltip's owner hands over through the Object::Base* slot; retail reads
        // it the same way.
        const TitleDescToolTipData* strings = reinterpret_cast<const TitleDescToolTipData*>(userData);   // LEFT: UI::Element's user data is Object::Base* (R-UI); the producer is not ported
        if (titleElement) {
            titleElement->SetParameter(0, strings->title);
        }
        if (descElement) {
            descElement->SetParameter(0, strings->desc);
        }
    }
}
