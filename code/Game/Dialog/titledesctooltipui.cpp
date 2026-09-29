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
    Object::Base* userData = static_cast<UI::Element*>(_dataSource)->GetUserData();

    if (userData) {
        // LEFT: the user data is two consecutive System::Strings (title at +0,
        // description at +8) that the tooltip's owner hands over as an
        // Object::Base*; retail reads them the same way.
        const System::String* strings = reinterpret_cast<const System::String*>(userData);
        if (titleElement) {
            titleElement->SetParameter(0, strings[0]);
        }
        if (descElement) {
            descElement->SetParameter(0, strings[1]);
        }
    }
}
