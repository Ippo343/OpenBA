// engine/src/ui/immigrationui.cpp
// Clean reimplementation of ImmigrationUI game logic.
//
// ImmigrationUI (0x50 bytes) — immigration allow/deny panel.
// Extends DialogController. Fields:
//   +0x30 textNone, +0x38 textNomad (InternalBasePtr)
//   +0x40 buttonAllow, +0x48 buttonDeny (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "ui/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Helper: Find element by name in dialog, return InternalPtr value (8 bytes)
static uint64_t FindElemByName(void* dialogElem, const char* name) {
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, name);

    uint64_t result = 0;
    fn_FindElement(&result, dialogElem, nameBuf);

    fn_String_Dtor(nameBuf);
    return result;
}

// Helper: Find element by name, dereference, wrap in InternalBasePtr
static uint64_t FindElemAsBasePtr(void* dialogElem, const char* name) {
    uint64_t intPtr = FindElemByName(dialogElem, name);
    void* rawElem = (void*)intPtr;
    uint8_t basePtrBuf[8];
    fn_InternalBasePtr_Ctor(basePtrBuf, rawElem);
    return *(uint64_t*)basePtrBuf;
}

// ============================================================
// ImmigrationUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Finds textNone, textNomad, buttonAllow, and buttonDeny
// elements in the dialog. Stores as InternalBasePtr and sets
// element IDs on the buttons.
//
// Original: 0x14006B330.

void ImmigrationUI_Slot4(ImmigrationUI* self) {
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);

    // Find text elements
    self->data30 = FindElemAsBasePtr(dialogElem, "textNone");
    self->data38 = FindElemAsBasePtr(dialogElem, "textNomad");

    // Find button elements
    self->data40 = FindElemAsBasePtr(dialogElem, "buttonAllow");
    self->data48 = FindElemAsBasePtr(dialogElem, "buttonDeny");

    // Set element IDs on buttons
    fn_Element_SetElementId((void*)self->data40, 39000);
    fn_Element_SetElementId((void*)self->data48, 0x9859);
}
