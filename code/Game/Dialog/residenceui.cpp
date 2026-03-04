// engine/src/ui/residenceui.cpp
// Clean reimplementation of ResidenceUI game logic.
//
// ResidenceUI (0x40 bytes) — residence info panel.
// Extends DialogController (0x30 base).
// +0x30 = dynamic entry array (0x30-byte entries), +0x38 = entry count.
//
// Each entry (0x30 bytes):
//   +0x00 = imageMale element ptr
//   +0x08 = imageFemale element ptr
//   +0x10 = textName element ptr
//   +0x18 = textAge element ptr
//   +0x20 = textProfession element ptr
//   +0x28 = buttonGoto element ptr
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Helper: find element by name in parent element, returns raw InternalPtr value
static void* FindChildElem(void* parentElem, const char* name) {
    uint8_t nameStr[8], retBuf[16];
    fn_String_Ctor_CStr(nameStr, name);
    fn_FindElement(parentElem, retBuf, nameStr);
    void* result = *(void**)retBuf;
    fn_String_Dtor(nameStr);
    return result;
}

// Helper: find element by name, wrap as InternalBasePtr
static uint64_t FindElemAsBasePtr(void* parentElem, const char* name) {
    void* elem = FindChildElem(parentElem, name);
    uint8_t bpBuf[8];
    fn_InternalBasePtr_Ctor(bpBuf, elem);
    return *(uint64_t*)bpBuf;
}

// ============================================================
// ResidenceUI::Slot4 (vtable +0x20) — Setup resident table
// ============================================================
// Finds "residenceTable" (or "residenceLongTable" fallback), iterates
// rows to find child elements (buttonGoto, textName, imageMale, imageFemale,
// textAge, textProfession), stores in dynamic array at +0x30.
// Sets element IDs on buttonGoto: 300 + rowIndex.
//
// Original: 0x1400B5850.

void ResidenceUI_Slot4(ResidenceUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* dialogElem = *(void**)(s + 0x20);

    // Try "residenceTable" first, fall back to "residenceLongTable"
    void* tableRaw = FindChildElem(dialogElem, "residenceTable");
    void* tableElem;

    if (!tableRaw) {
        // Fallback: find "residenceLongTable"
        void* longTable = FindChildElem(dialogElem, "residenceLongTable");
        uint8_t basePtrBuf[8];
        fn_InternalBasePtr_Ctor(basePtrBuf, longTable);
        tableElem = *(void**)basePtrBuf;
    } else {
        uint8_t basePtrBuf[8];
        fn_InternalBasePtr_Ctor(basePtrBuf, tableRaw);
        tableElem = *(void**)basePtrBuf;
    }

    // Get row count from table element (ChildCount)
    int rowCount = fn_Element_ChildCount(tableElem);

    for (int i = 0; i < rowCount; i++) {
        // 0x30-byte entry: imageMale, imageFemale, textName, textAge, textProfession, buttonGoto
        uint64_t entry[6] = {0, 0, 0, 0, 0, 0};

        // Get child row element
        uint8_t childBuf[8];

        // Find "buttonGoto" in row, set ID 300+i
        fn_Element_Child(tableElem, childBuf, i);
        entry[5] = FindElemAsBasePtr(*(void**)childBuf, "buttonGoto");
        fn_Element_SetElementId((void*)entry[5], 300 + i);

        // Find "textName" in row
        fn_Element_Child(tableElem, childBuf, i);
        entry[2] = FindElemAsBasePtr(*(void**)childBuf, "textName");

        // Find "imageMale" in row
        fn_Element_Child(tableElem, childBuf, i);
        entry[0] = FindElemAsBasePtr(*(void**)childBuf, "imageMale");

        // Find "imageFemale" in row
        fn_Element_Child(tableElem, childBuf, i);
        entry[1] = FindElemAsBasePtr(*(void**)childBuf, "imageFemale");

        // Find "textAge" in row
        fn_Element_Child(tableElem, childBuf, i);
        entry[3] = FindElemAsBasePtr(*(void**)childBuf, "textAge");

        // Find "textProfession" in row
        fn_Element_Child(tableElem, childBuf, i);
        entry[4] = FindElemAsBasePtr(*(void**)childBuf, "textProfession");

        // Push entry into dynamic array at +0x30
        // FUN_140021020: array resize/push with 0x30-byte elements
        fn_ResidenceUI_ArrayPush((void*)(s + 0x30), *(int*)(s + 0x38) + 1, entry);
    }
}

// ============================================================
// ResidenceUI::Slot9 (vtable +0x48) — Button dispatch
// ============================================================
// Handles resident row button clicks. Each row has element ID = 300 + index.
// Clicking navigates camera to that resident citizen.
//
// Original: 0x1400B5820.

void ResidenceUI_Slot9(ResidenceUI* self, void* param2, int msg, int id) {
    (void)param2;
    uintptr_t s = (uintptr_t)self;

    if (msg != 0x12) return;

    int count = *(int*)(s + 0x38);
    if (id >= 300 && id < 300 + count) {
        fn_ResidenceUI_GoToCitizen(self, id - 300);
    }
}
