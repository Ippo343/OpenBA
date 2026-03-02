// engine/src/ui/titledesctooltipui.cpp
// Clean reimplementation of TitleDescToolTipUI game logic.
//
// TitleDescToolTipUI (0x30 bytes) — tooltip that shows a title and description.
// Extends DialogController. No subclass fields.
//
// Decompiled from Application-x64.exe @ 0x14007b730 (Serialize / vtable +0x20).

#include "ui/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

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
    uint8_t titleNameBuf[8];
    fn_String_Ctor_CStr(titleNameBuf, "textTitle");

    void* titleResult = nullptr;
    fn_FindElement(dialogInstance, &titleResult, titleNameBuf);
    void* titleElement = titleResult;

    fn_String_Dtor(titleNameBuf);

    // --- Find "textDesc" element ---
    uint8_t descNameBuf[8];
    fn_String_Ctor_CStr(descNameBuf, "textDesc");

    void* descResult = nullptr;
    fn_FindElement(dialogInstance, &descResult, descNameBuf);
    void* descElement = descResult;

    fn_String_Dtor(descNameBuf);

    // --- Check data source type and get user data ---
    // The original checks IsDerivedFrom on the data source, then calls
    // Element::GetUserData to get a Base* pointing to two consecutive
    // String objects (title at +0, description at +8).
    //
    // The IsDerivedFrom check verifies the data source is an Internal-derived
    // object. For correctness we replicate this check.
    // However, the type ID comes from an exe-internal _exref. Since we can't
    // easily resolve that, we use GetUserData directly with a null check.
    void* userData = fn_Element_GetUserData(dataSource);

    if (userData) {
        // userData points to two consecutive System::String objects:
        //   +0x00: title string
        //   +0x08: description string
        if (titleElement) {
            fn_TextElement_SetParameter(titleElement, 0, userData);
        }
        if (descElement) {
            fn_TextElement_SetParameter(descElement, 0, (void*)((uintptr_t)userData + 8));
        }
    }
}
