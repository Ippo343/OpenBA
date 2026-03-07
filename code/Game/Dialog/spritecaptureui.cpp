// code/Game/Dialog/spritecaptureui.cpp
// Clean reimplementation of SpriteCaptureUI from Application-x64.exe.
//
// SpriteCaptureUI (0x70 bytes), vtable at 0x14015d4f0.
// Extends DialogController (0x30 base). Dev tool for capturing UI element sprites.
//
// Serialize (vtable +0x20, 0x140079640):
//   Finds 3 elements: "editCapture", "buttonCapture", "captureBox".
//   Sets button ID 1000, default capture path, initializes capture state.
//
// OnCreate (vtable +0x40, 0x1400793a0) — per-frame update:
//   Handles mouse drag for capture box. When pendingExternal set and
//   saveFrameCount reaches 1, calls EndSpriteCapture, builds save path from
//   FileSystem::GetSourceDirectory + edit text + ".rsc", calls External::SaveText.

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
    *(uint64_t*)(s + 0x48) = 0;       // curX=0, curY=0
    *(int32_t*)(s + 0x50) = 100;      // anchorX = 100
    *(int32_t*)(s + 0x54) = 100;      // anchorY = 100
    *(uint8_t*)(s + 0x58) = 0;        // dragging = false
    *(uint64_t*)(s + 0x60) = 0;       // pendingExternal = nullptr
}

// ============================================================
// SpriteCaptureUI::OnCreate (vtable +0x40, 0x1400793a0)
// Per-frame update: drag capture box with left mouse + key 0x3E.
// When pendingExternal is set, waits one frame then saves.
// ============================================================
void SpriteCaptureUI_OnCreate(SpriteCaptureUI* self) {
    // Get Input::Device singleton
    void* inputDevice = g_pInputDeviceSingleton ? *g_pInputDeviceSingleton : nullptr;
    if (!inputDevice) return;

    // 1. If currently dragging, check if left mouse button (axis 4) was released
    if (self->dragging) {
        short btnHeld = fn_Input_Device_GetValue(inputDevice, 2, 0, 4);
        if (btnHeld == 0) {
            self->dragging = 0;
            goto check_pressed;
        }
    }

    // 2. While dragging, update anchor position to current mouse pos
    if (self->dragging) {
        short mx = fn_Input_Device_GetValue(inputDevice, 2, 0, 0);
        short my = fn_Input_Device_GetValue(inputDevice, 2, 0, 1);
        self->anchorX = (int)mx;
        self->anchorY = (int)my;
    }

check_pressed:
    // 3. Check for new capture start: left mouse pressed + key 0x3E held
    if (fn_Input_Device_Pressed(inputDevice, 2, 0, 4)) {
        short keyHeld = fn_Input_Device_GetValue(inputDevice, 1, 0, 0x3e);
        if (keyHeld != 0) {
            short mx = fn_Input_Device_GetValue(inputDevice, 2, 0, 0);
            short my = fn_Input_Device_GetValue(inputDevice, 2, 0, 1);
            self->curX   = (int)mx;
            self->curY   = (int)my;
            self->anchorX = (int)mx;
            self->anchorY = (int)my;
            self->dragging = 1;
        }
    }

    // 4. While dragging, push current bounds into captureBox element (+0xD0..0xDC)
    if (self->dragging) {
        fn_InternalBasePtr_CheckPtr(&self->captureBox);
        void* boxElem = *(void**)&self->captureBox;
        if (boxElem) {
            int32_t* bounds = (int32_t*)((uintptr_t)boxElem + 0xd0);
            bounds[0] = self->curX;
            bounds[1] = self->curY;
            bounds[2] = self->anchorX;
            bounds[3] = self->anchorY;
        }
    }

    // 5. Handle pending External save
    void* pending = self->pendingExternal;
    if (pending) {
        if (self->saveFrameCount == 1) {
            // a. End sprite capture mode on UserInterface
            //    DialogController+0x18 = dialogInstance (InternalBasePtr); dialogInstance+0x10 = UserInterface*
            void* pIBP18 = (void*)((uintptr_t)self + 0x18);
            fn_InternalBasePtr_CheckPtr(pIBP18);
            void* dialogInst = *(void**)pIBP18;
            void* ui = *(void**)((uintptr_t)dialogInst + 0x10);
            fn_UI_EndSpriteCapture(ui);

            // b. Get source directory as System::String (non-trivial return: RCX=this, RDX=retval)
            void* fsInstance = g_pFileSystemSingleton ? *g_pFileSystemSingleton : nullptr;
            uint64_t sourceDirStr[2] = {0, 0};
            if (fsInstance)
                fn_FileSystem_GetSourceDirectory(fsInstance, sourceDirStr);

            // c. Get edit element text (String at editCapture + 0xC8)
            fn_InternalBasePtr_CheckPtr(&self->editCapture);
            void* editElem = *(void**)&self->editCapture;
            void* editText = editElem ? (void*)((uintptr_t)editElem + 0xc8) : nullptr;

            // d. Concat: sourceDir + editText -> concatStr1
            //    Then: concatStr1 + ".rsc" literal -> concatStr2
            //    Filename ctor from concatStr2 -> pathFilename
            // ABI: RCX=retval, RDX=lhs, R8=rhs (free function, non-trivial return)
            uint64_t concatStr1[2] = {0, 0};
            uint64_t concatStr2[2] = {0, 0};
            uint64_t filenameRsc[2] = {0, 0};   // Filename for "resource" subdir
            uint64_t pathFilename[2] = {0, 0};  // Filename for full path

            if (fn_String_ConcatSS && editText)
                fn_String_ConcatSS(concatStr1, sourceDirStr, editText);

            // ".rsc" string literal lives at 0x14015d424 in the exe .rdata
            const void* rscLiteral = (const void*)0x14015d424;
            if (fn_String_Concat)
                fn_String_Concat(concatStr2, concatStr1, rscLiteral);

            if (fn_Filename_CtorString)
                fn_Filename_CtorString(pathFilename, concatStr2);

            // Cleanup intermediate strings
            fn_String_Dtor(concatStr2);
            fn_String_Dtor(concatStr1);
            fn_String_Dtor(sourceDirStr);

            // e. Build subdir Filename from "resource" literal at 0x14015d430
            if (fn_Filename_CtorCStr)
                fn_Filename_CtorCStr(filenameRsc, "resource");

            // f. Save: External::SaveText(this, pathFilename, subdirFilename)
            fn_External_SaveText(pending, pathFilename, filenameRsc);

            // g. Call vtable[0] on the External object (scalar dtor with flag=1, frees it)
            typedef void (*fn_VtableDtor_t)(void* self, uint32_t flags);
            void** vtable = *(void***)pending;
            if (vtable)
                ((fn_VtableDtor_t)vtable[0])(pending, 1);

            // h. Clear pendingExternal
            self->pendingExternal = nullptr;

            // i. Hide captureBox element: SetFlagsOnOff(elem, hideFlags=0, showFlags=1)
            fn_InternalBasePtr_CheckPtr(&self->captureBox);
            void* box = *(void**)&self->captureBox;
            if (box)
                fn_Element_SetFlagsOnOff(box, 0, 1);

            // j. Cleanup filenames
            if (fn_Filename_Dtor) {
                fn_Filename_Dtor(filenameRsc);
                fn_Filename_Dtor(pathFilename);
            }
        } else {
            // Not yet at frame 1: increment counter
            self->saveFrameCount++;
        }
    }
}
