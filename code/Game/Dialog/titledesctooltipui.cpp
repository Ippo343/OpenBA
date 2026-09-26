// engine/src/ui/titledesctooltipui.cpp
// Clean reimplementation of TitleDescToolTipUI game logic.
//
// TitleDescToolTipUI (0x30 bytes) — tooltip that shows a title and description.
// Extends DialogController. No subclass fields.
//
// Decompiled from Application-x64.exe @ 0x14007b730 (Serialize / vtable +0x20).

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/TextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)

// ============================================================
// TitleDescToolTipUI::Serialize (vtable +0x20)
// ============================================================
// Finds "textTitle" and "textDesc" elements in the parent dialog instance,
// then reads the user data (two consecutive String pointers) from the
// controller's data source and sets them as text parameters.
//
// Original: 0x14007b730, 50 instructions.

void TitleDescToolTipUI_Serialize(TitleDescToolTipUI* self) {
    // DialogController base field at +0x18 holds a pointer to the parent
    // DialogInstance. At +0x28 is the data source (Base* element).
    void* dialogInstance = *(void**)((uintptr_t)self + 0x18);
    void* dataSource = *(void**)((uintptr_t)self + 0x28);

    if (!dialogInstance || !dataSource)
        return;

    // --- Find "textTitle" element ---
    // FindElement: member with non-trivial return: RCX=this, RDX=retval, R8=name
    System::String titleNameStr("textTitle");

    void* titleResult = nullptr;
    ::new (&titleResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogInstance)->FindElement(titleNameStr));  // B.3.6 bridge
    void* titleElement = titleResult;

    // --- Find "textDesc" element ---
    System::String descNameStr("textDesc");

    void* descResult = nullptr;
    ::new (&descResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogInstance)->FindElement(descNameStr));  // B.3.6 bridge
    void* descElement = descResult;

    // --- Check data source type and get user data ---
    // The original checks IsDerivedFrom on the data source, then calls
    // Element::GetUserData to get a Base* pointing to two consecutive
    // String objects (title at +0, description at +8).
    //
    // The IsDerivedFrom check verifies the data source is an Internal-derived
    // object. For correctness we replicate this check.
    // However, the type ID comes from an exe-internal _exref. Since we can't
    // easily resolve that, we use GetUserData directly with a null check.
    void* userData = reinterpret_cast<UI::Element*>(dataSource)->GetUserData();  // B.3.6 bridge

    if (userData) {
        // userData points to two consecutive System::String objects:
        //   +0x00: title string
        //   +0x08: description string
        if (titleElement) {
            reinterpret_cast<UI::TextElement*>(titleElement)->SetParameter(0, *reinterpret_cast<const System::String*>(userData));  // B.3.6 bridge
        }
        if (descElement) {
            reinterpret_cast<UI::TextElement*>(descElement)->SetParameter(0, *reinterpret_cast<const System::String*>(((uintptr_t)userData + 8)));  // B.3.6 bridge
        }
    }
}
