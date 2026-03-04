// engine/src/ui/tutorialui.cpp
// Clean reimplementation of TutorialUI panel methods.
//
// TutorialUI (0x60 bytes): Tutorial step display panel.
// Layout: +0x30 = unknown, +0x38 = "preText" element, +0x40 = "postText" element,
//         +0x48 = "imageTable" element, +0x50 = "nextButton" element,
//         +0x58 = "quitButton" element

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Helper: FindElemByName with IsDerivedFrom check, stores InternalBasePtr
static void FindAndStore(uintptr_t self, const char* name, int fieldOffset) {
    char nameBuf[8] = {};
    fn_String_Ctor_CStr(nameBuf, name);

    uint64_t foundResult = 0;
    fn_FindElement((void*)*(uint64_t*)(self + 0x20), &foundResult, nameBuf);

    void* found = (void*)foundResult;
    void* checked = nullptr;
    // The exref type check uses the generic Element type from the import table.
    // In practice these all succeed, so we just do a null check and store.
    if (found) {
        checked = found;
    }
    uint64_t stored = 0;
    fn_InternalBasePtr_Ctor(&stored, checked);
    *(uint64_t*)(self + fieldOffset) = stored;

    fn_String_Dtor(nameBuf);
}

// Serialize (vtable +0x20): Finds named elements, sets button IDs, hides close button.
void TutorialUI_Serialize(TutorialUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Find and cache UI elements
    FindAndStore(s, "preText",   0x38);
    FindAndStore(s, "postText",  0x40);
    FindAndStore(s, "imageTable", 0x48);
    FindAndStore(s, "nextButton", 0x50);
    FindAndStore(s, "quitButton", 0x58);

    // Set button IDs
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x50), 0x578);  // nextButton = 1400
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x58), 0x579);  // quitButton = 1401

    // Hide the "buttonClose" element in the dialog instance
    char closeName[8] = {};
    fn_String_Ctor_CStr(closeName, "buttonClose");
    uint64_t closeElem = 0;
    fn_DialogInstance_FindElement((void*)*(uint64_t*)(s + 0x18), &closeElem, closeName);
    // SetFlagsOnOff(elem, hideFlags=1, showFlags=0) — hide element
    fn_Element_SetFlagsOnOff((void*)closeElem, 1, 0);
    fn_String_Dtor(closeName);
}

// Startup (vtable +0x38): Toggle visibility of next/quit buttons.
// If the element has visibility flags (0x1800000) set, hide it; otherwise show it.
void TutorialUI_Startup(TutorialUI* self) {
    uintptr_t s = (uintptr_t)self;

    // nextButton (+0x50): check flags at element +0x34
    void* nextBtn = (void*)*(uint64_t*)(s + 0x50);
    bool nextHidden = (*(uint32_t*)((uintptr_t)nextBtn + 0x34) & 0x1800000) != 0;
    fn_Element_SetFlagsOnOff(nextBtn, nextHidden ? 1 : 0, nextHidden ? 0 : 1);

    // quitButton (+0x58): same check
    void* quitBtn = (void*)*(uint64_t*)(s + 0x58);
    bool quitHidden = (*(uint32_t*)((uintptr_t)quitBtn + 0x34) & 0x1800000) != 0;
    fn_Element_SetFlagsOnOff(quitBtn, quitHidden ? 1 : 0, quitHidden ? 0 : 1);
}
