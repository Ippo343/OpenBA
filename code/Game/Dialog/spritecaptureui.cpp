// code/Game/Dialog/spritecaptureui.cpp
// Clean reimplementation of SpriteCaptureUI::Serialize from Application-x64.exe.
//
// SpriteCaptureUI (0x70 bytes), vtable at 0x14015d4f0.
// Extends DialogController (0x30 base). Dev tool for capturing UI element sprites.
//
// Serialize finds 3 elements: "editCapture" (EditElement at +0x30),
// "buttonCapture" (ButtonElement at +0x38), "captureBox" (CaptureBoxElement at +0x40).
// Sets button ID to 1000, sets default capture path, initializes capture state.
//
// OnCreate (per-frame update) stays as trampoline passthrough due to many
// one-off dependencies (Input::Device, FileSystem, Filename, External::SaveText).

#include "ui/Panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// SpriteCaptureUI::Serialize (vtable +0x20, 0x140079640)
// ============================================================
void SpriteCaptureUI_Serialize(SpriteCaptureUI* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Find "editCapture" element, check IsDerivedFrom(EditElement) ---
    uint64_t strBuf[2] = {0, 0};
    fn_String_Ctor_CStr(strBuf, "editCapture");

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(*(void**)(s + 0x20), foundBuf, strBuf);

    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* elem = *(void**)foundBuf;

    void* editElem = nullptr;
    if (elem && g_TypeId_EditElement) {
        if (fn_IsDerivedFrom(elem, *g_TypeId_EditElement))
            editElem = elem;
    }

    uint64_t tempIBP = 0;
    fn_InternalBasePtr_Ctor(&tempIBP, editElem);
    *(uint64_t*)(s + 0x30) = tempIBP;
    fn_String_Dtor(strBuf);

    // --- Find "buttonCapture" element, check IsDerivedFrom(ButtonElement) ---
    strBuf[0] = 0; strBuf[1] = 0;
    fn_String_Ctor_CStr(strBuf, "buttonCapture");

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    foundBuf[0] = 0; foundBuf[1] = 0;
    fn_FindElement(*(void**)(s + 0x20), foundBuf, strBuf);

    fn_InternalBasePtr_CheckPtr(foundBuf);
    elem = *(void**)foundBuf;

    void* buttonElem = nullptr;
    if (elem && g_TypeId_HelpButtonElem) {
        if (fn_IsDerivedFrom(elem, *g_TypeId_HelpButtonElem))
            buttonElem = elem;
    }

    tempIBP = 0;
    fn_InternalBasePtr_Ctor(&tempIBP, buttonElem);
    *(uint64_t*)(s + 0x38) = tempIBP;
    fn_String_Dtor(strBuf);

    // --- Find "captureBox" element, check IsDerivedFrom(CaptureBoxElement) ---
    strBuf[0] = 0; strBuf[1] = 0;
    fn_String_Ctor_CStr(strBuf, "captureBox");

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    foundBuf[0] = 0; foundBuf[1] = 0;
    fn_FindElement(*(void**)(s + 0x20), foundBuf, strBuf);

    fn_InternalBasePtr_CheckPtr(foundBuf);
    elem = *(void**)foundBuf;

    void* captureBoxElem = nullptr;
    if (elem && g_TypeId_CaptureBoxElement) {
        if (fn_IsDerivedFrom(elem, *g_TypeId_CaptureBoxElement))
            captureBoxElem = elem;
    }

    tempIBP = 0;
    fn_InternalBasePtr_Ctor(&tempIBP, captureBoxElem);
    *(uint64_t*)(s + 0x40) = tempIBP;
    fn_String_Dtor(strBuf);

    // --- Set button element ID to 1000 ---
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x38));
    fn_Element_SetElementId(*(void**)(s + 0x38), 1000);

    // --- Set default capture path in edit element ---
    strBuf[0] = 0; strBuf[1] = 0;
    fn_String_Ctor_CStr(strBuf, "Game/Help/Dialog/");
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
    fn_EditElement_SetText(*(void**)(s + 0x30), strBuf);
    fn_String_Dtor(strBuf);

    // --- Initialize capture state fields ---
    *(uint64_t*)(s + 0x48) = 0;       // capture state (inactive)
    *(int32_t*)(s + 0x50) = 100;      // capture width
    *(int32_t*)(s + 0x54) = 100;      // capture height
    *(uint8_t*)(s + 0x58) = 0;        // capture pending flag
    *(uint64_t*)(s + 0x60) = 0;       // capture data pointer
}
