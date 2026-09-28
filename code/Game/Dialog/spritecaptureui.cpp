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
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A — Object::External
#include "io/FileSystem.h"     // ENDGAME B.3.4 caller batch P
#include <new>
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/UserInterface.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/EditElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "input/Input.h"  // ENDGAME B.3.7 callers (K3)
#include "runtime/types/ui/ButtonElement.h"  // X::TypeId() (B.3.9 CG2)
#include "Component/CaptureBoxElement.h"   // X::TypeId() (B.3.9 CG2)
#include "input/Device.h"   // ENDGAME B.3.9 SS2: Input::Device::_singleton

// ============================================================
// SpriteCaptureUI::Serialize (vtable +0x20, 0x140079640)
// ============================================================
void SpriteCaptureUI_Serialize(SpriteCaptureUI* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Find "editCapture" element, check IsDerivedFrom(EditElement) ---
    uint64_t foundBuf[2] = {0, 0};
    {
        System::String nameStr("editCapture");
        ::new (foundBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)(s + 0x20))->FindElement(nameStr));  // B.3.6 bridge
    }

    void* elem = *(void**)foundBuf;

    void* editElem = nullptr;
    if (elem && fn_IsDerivedFrom(elem, UI::EditElement::TypeId()))
        editElem = elem;

    uint64_t tempIBP = 0;
    new (&tempIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(editElem));   // B.3.3 bridge
    *(uint64_t*)(s + 0x30) = tempIBP;

    // --- Find "buttonCapture" element, check IsDerivedFrom(ButtonElement) ---
    foundBuf[0] = 0; foundBuf[1] = 0;
    {
        System::String nameStr("buttonCapture");
        ::new (foundBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)(s + 0x20))->FindElement(nameStr));  // B.3.6 bridge
    }

    elem = *(void**)foundBuf;

    // ENDGAME B.3.9 CG2: TypeId_HelpButtonElem shares its initialiser (exref
    // 0x1401514f8) with TypeId_ButtonElement -> UI::ButtonElement::TypeId()
    // (Table 3 name lie: this is not a distinct "help button" element class).
    void* buttonElem = nullptr;
    if (elem && fn_IsDerivedFrom(elem, UI::ButtonElement::TypeId()))
        buttonElem = elem;

    tempIBP = 0;
    new (&tempIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(buttonElem));   // B.3.3 bridge
    *(uint64_t*)(s + 0x38) = tempIBP;

    // --- Find "captureBox" element, check IsDerivedFrom(CaptureBoxElement) ---
    foundBuf[0] = 0; foundBuf[1] = 0;
    {
        System::String nameStr("captureBox");
        ::new (foundBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)(s + 0x20))->FindElement(nameStr));  // B.3.6 bridge
    }

    elem = *(void**)foundBuf;

    void* captureBoxElem = nullptr;
    if (elem && fn_IsDerivedFrom(elem, CaptureBoxElement::TypeId()))
        captureBoxElem = elem;

    tempIBP = 0;
    new (&tempIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(captureBoxElem));   // B.3.3 bridge
    *(uint64_t*)(s + 0x40) = tempIBP;

    // --- Set button element ID to 1000 ---
    reinterpret_cast<UI::Element*>(*(void**)(s + 0x38))->SetElementId(1000);  // B.3.6 bridge

    // --- Set default capture path in edit element ---
    {
        System::String pathStr("Game/Help/Dialog/");
        reinterpret_cast<UI::EditElement*>(*(void**)(s + 0x30))->SetText(pathStr);  // B.3.6 bridge
    }

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
    void* inputDevice = ::Input::Device::_singleton;
    if (!inputDevice) return;

    // 1. If currently dragging, check if left mouse button (axis 4) was released
    if (self->dragging) {
        short btnHeld = reinterpret_cast<const Input::Device*>(inputDevice)->GetValue(Input::Device::InputDevice::Mouse, 0, 4);  // B.3.7 bridge
        if (btnHeld == 0) {
            self->dragging = 0;
            goto check_pressed;
        }
    }

    // 2. While dragging, update anchor position to current mouse pos
    if (self->dragging) {
        short mx = reinterpret_cast<const Input::Device*>(inputDevice)->GetValue(Input::Device::InputDevice::Mouse, 0, 0);  // B.3.7 bridge
        short my = reinterpret_cast<const Input::Device*>(inputDevice)->GetValue(Input::Device::InputDevice::Mouse, 0, 1);  // B.3.7 bridge
        self->anchorX = (int)mx;
        self->anchorY = (int)my;
    }

check_pressed:
    // 3. Check for new capture start: left mouse pressed + key 0x3E held
    if (reinterpret_cast<const Input::Device*>(inputDevice)->Pressed(Input::Device::InputDevice::Mouse, 0, 4)) {  // B.3.7 bridge
        short keyHeld = reinterpret_cast<const Input::Device*>(inputDevice)->GetValue(Input::Device::InputDevice::Keyboard, 0, 0x3e);  // B.3.7 bridge
        if (keyHeld != 0) {
            short mx = reinterpret_cast<const Input::Device*>(inputDevice)->GetValue(Input::Device::InputDevice::Mouse, 0, 0);  // B.3.7 bridge
            short my = reinterpret_cast<const Input::Device*>(inputDevice)->GetValue(Input::Device::InputDevice::Mouse, 0, 1);  // B.3.7 bridge
            self->curX   = (int)mx;
            self->curY   = (int)my;
            self->anchorX = (int)mx;
            self->anchorY = (int)my;
            self->dragging = 1;
        }
    }

    // 4. While dragging, push current bounds into captureBox element (+0xD0..0xDC)
    if (self->dragging) {
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
            void* dialogInst = *(void**)pIBP18;
            void* ui = *(void**)((uintptr_t)dialogInst + 0x10);
            reinterpret_cast<UI::UserInterface*>(ui)->EndSpriteCapture();  // B.3.6 bridge

            // b. Get source directory as System::String (non-trivial return: RCX=this, RDX=retval)
            void* fsInstance = ::IO::FileSystem::_singleton;
            System::String sourceDirStr;
            // B.3.4 bridge. The guard stays. The member returns
            // `System::Filename` BY VALUE through MSVC's hidden return slot, and
            // `&sourceDirStr` is the slot the door was handed — Filename and
            // String are the same 8 bytes with the same release sequence
            // (Filename.h), which is why the door wrote a Filename into a String
            // local. One AddRef, and ~String at scope exit is the one release.
            if (fsInstance)
                ::new (reinterpret_cast<System::Filename*>(&sourceDirStr)) System::Filename(
                    reinterpret_cast<const IO::FileSystem*>(fsInstance)->GetSourceDirectory());

            // c. Get edit element text (String at editCapture + 0xC8)
            void* editElem = *(void**)&self->editCapture;
            void* editText = editElem ? (void*)((uintptr_t)editElem + 0xc8) : nullptr;

            // d. Concat: sourceDir + editText -> concatStr1
            //    Then: concatStr1 + ".rsc" literal -> concatStr2
            //    Filename ctor from concatStr2 -> pathFilename
            System::String concatStr1;
            if (editText)
                concatStr1 = sourceDirStr + *reinterpret_cast<System::String*>(editText);  // B.3.1 bridge

            // ".rsc" — retail's literal at 0x14015d424 in the exe .rdata
            const char* rscLiteral = ".rsc";
            System::String concatStr2 = concatStr1 + rscLiteral;

            System::Filename pathFilename(concatStr2);

            // e. Build subdir Filename from "resource" literal at 0x14015d430
            System::Filename filenameRsc("resource");

            // f. Save: External::SaveText(this, pathFilename, subdirFilename)
            reinterpret_cast<::Object::External*>(pending)                 // B.3.3 bridge
                ->SaveText(pathFilename, filenameRsc);

            // g. Call vtable[0] on the External object (scalar dtor with flag=1, frees it)
            typedef void (*fn_VtableDtor_t)(void* self, uint32_t flags);
            void** vtable = *(void***)pending;
            if (vtable)
                ((fn_VtableDtor_t)vtable[0])(pending, 1);

            // h. Clear pendingExternal
            self->pendingExternal = nullptr;

            // i. Hide captureBox element: SetFlagsOnOff(elem, hideFlags=0, showFlags=1)
            void* box = *(void**)&self->captureBox;
            if (box)
                reinterpret_cast<UI::Element*>(box)->SetFlagsOnOff(0, 1);  // B.3.6 bridge

            // j. filenameRsc/pathFilename release via their destructors (System::Filename).
        } else {
            // Not yet at frame 1: increment counter
            self->saveFrameCount++;
        }
    }
}

// ============================================================
