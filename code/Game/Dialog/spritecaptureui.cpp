// code/Game/Dialog/spritecaptureui.cpp
// Clean reimplementation of SpriteCaptureUI from Application-x64.exe.
//
// SpriteCaptureUI (0x70 bytes), vtable at 0x14015d4f0.
// Extends DialogController (0x30 base). Dev tool for capturing UI element sprites.
// Fields: Dialog/SpriteCaptureUI.h.
//
// OnCreated (vtable +0x20, 0x140079640):
//   Finds 3 elements: "editCapture", "buttonCapture", "captureBox".
//   Sets button ID 1000, default capture path, initializes capture state.
//
// OnUpdate (vtable +0x40, 0x1400793a0) — per-frame update:
//   Handles mouse drag for capture box. When pendingExternal set and
//   saveFrameCount reaches 1, calls EndSpriteCapture, builds save path from
//   FileSystem::GetSourceDirectory + edit text + ".rsc", calls External::SaveText.

#include "ui/Panels.h"
#include "Dialog/SpriteCaptureUI.h"
#include "system/Value.h"
#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A — Object::External
#include "io/FileSystem.h"     // ENDGAME B.3.4 caller batch P
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/DialogInstance.h"
#include "runtime/types/ui/UserInterface.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/EditElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "input/Input.h"  // ENDGAME B.3.7 callers (K3)
#include "runtime/types/ui/ButtonElement.h"  // X::TypeId() (B.3.9 CG2)
#include "Component/CaptureBoxElement.h"   // X::TypeId() (B.3.9 CG2)
#include "input/Device.h"   // ENDGAME B.3.9 SS2: Input::Device::_singleton

// The element named `name` under the panel's root, if it is a `typeId`.
static UI::Element* FindTyped(UI::Element* root, const char* name, uint32_t typeId) {
    System::String nameStr(name);
    UI::Element* elem = root->FindElement(nameStr).Get();
    if (elem && ::Object::IsDerivedFromNullSafe(elem, typeId))
        return elem;
    return nullptr;
}

// ============================================================
// SpriteCaptureUI::OnCreated (vtable +0x20, 0x140079640)
// ============================================================
// [4] 0x140079640
void SpriteCaptureUI::OnCreated() {
    SpriteCaptureUI* self = this;
    // --- the three elements, each checked with IsDerivedFrom ---
    self->editCapture = static_cast<UI::EditElement*>(
        FindTyped(self->_container, "editCapture", UI::EditElement::TypeId()));
    // ENDGAME B.3.9 CG2: TypeId_HelpButtonElem shares its initialiser (exref
    // 0x1401514f8) with TypeId_ButtonElement -> UI::ButtonElement::TypeId()
    // (Table 3 name lie: this is not a distinct "help button" element class).
    self->buttonCapture = static_cast<UI::ButtonElement*>(
        FindTyped(self->_container, "buttonCapture", UI::ButtonElement::TypeId()));
    self->captureBox = static_cast<CaptureBoxElement*>(
        FindTyped(self->_container, "captureBox", CaptureBoxElement::TypeId()));

    // --- Set button element ID to 1000 ---
    self->buttonCapture->SetElementId(1000);

    // --- Set default capture path in edit element ---
    {
        System::String pathStr("Game/Help/Dialog/");
        self->editCapture->SetText(pathStr);
    }

    // --- Initialize capture state fields ---
    self->curX = 0;
    self->curY = 0;
    self->anchorX = 100;
    self->anchorY = 100;
    self->dragging = 0;
    self->pendingExternal = nullptr;
}

// ============================================================
// SpriteCaptureUI::OnUpdate (vtable +0x40, 0x1400793a0)
// Per-frame update: drag capture box with left mouse + key 0x3E.
// When pendingExternal is set, waits one frame then saves.
// ============================================================
// [8] 0x1400793a0
void SpriteCaptureUI::OnUpdate(const System::Time& /*time*/) {
    SpriteCaptureUI* self = this;
    // Input::Device singleton (the tree's null guard; retail reads it unguarded)
    const Input::Device* inputDevice = ::Input::Device::_singleton;
    if (!inputDevice) return;

    // 1. If currently dragging, check if left mouse button (axis 4) was released
    if (self->dragging) {
        short btnHeld = inputDevice->GetValue(Input::Device::InputDevice::Mouse, 0, 4);
        if (btnHeld == 0) {
            self->dragging = 0;
            goto check_pressed;
        }
    }

    // 2. While dragging, update anchor position to current mouse pos
    if (self->dragging) {
        short mx = inputDevice->GetValue(Input::Device::InputDevice::Mouse, 0, 0);
        short my = inputDevice->GetValue(Input::Device::InputDevice::Mouse, 0, 1);
        self->anchorX = (int)mx;
        self->anchorY = (int)my;
    }

check_pressed:
    // 3. Check for new capture start: left mouse pressed + key 0x3E held
    if (inputDevice->Pressed(Input::Device::InputDevice::Mouse, 0, 4)) {
        short keyHeld = inputDevice->GetValue(Input::Device::InputDevice::Keyboard, 0, 0x3e);
        if (keyHeld != 0) {
            short mx = inputDevice->GetValue(Input::Device::InputDevice::Mouse, 0, 0);
            short my = inputDevice->GetValue(Input::Device::InputDevice::Mouse, 0, 1);
            self->curX   = (int)mx;
            self->curY   = (int)my;
            self->anchorX = (int)mx;
            self->anchorY = (int)my;
            self->dragging = 1;
        }
    }

    // 4. While dragging, push current bounds into the captureBox's corners
    //    (the tree's null guard on the box; retail has none)
    if (self->dragging) {
        CaptureBoxElement* box = self->captureBox;
        if (box) {
            box->_x0 = self->curX;
            box->_y0 = self->curY;
            box->_x1 = self->anchorX;
            box->_y1 = self->anchorY;
        }
    }

    // 5. Handle pending External save
    Object::External* pending = self->pendingExternal;
    if (pending) {
        if (self->saveFrameCount == 1) {
            // a. End sprite capture mode on the dialog's UserInterface
            self->_dialog->_ui->EndSpriteCapture();

            // b. The source directory (the tree's guard on the FileSystem kept)
            const IO::FileSystem* fs = ::IO::FileSystem::_singleton;
            System::String sourceDirStr;
            if (fs)
                sourceDirStr = fs->GetSourceDirectory().AsString();

            // c. The edit element's text (the tree's guard on the element kept)
            UI::EditElement* editElem = self->editCapture;

            // d. Concat: sourceDir + editText -> concatStr1
            //    Then: concatStr1 + ".rsc" literal -> concatStr2
            //    Filename ctor from concatStr2 -> pathFilename
            System::String concatStr1;
            if (editElem)
                concatStr1 = sourceDirStr + editElem->GetText();

            // ".rsc" — retail's literal at 0x14015d424 in the exe .rdata
            const char* rscLiteral = ".rsc";
            System::String concatStr2 = concatStr1 + rscLiteral;

            System::Filename pathFilename(concatStr2);

            // e. Build subdir Filename from "resource" literal at 0x14015d430
            System::Filename filenameRsc("resource");

            // f. Save: External::SaveText(this, pathFilename, subdirFilename)
            pending->SaveText(pathFilename, filenameRsc);

            // g. Delete the External through its own slot 0 (flag 1 = free)
            delete pending;

            // h. Clear pendingExternal
            self->pendingExternal = nullptr;

            // i. Hide captureBox element: SetFlagsOnOff(elem, hideFlags=0, showFlags=1)
            if (self->captureBox)
                self->captureBox->SetFlagsOnOff(0, 1);

            // j. filenameRsc/pathFilename release via their destructors (System::Filename).
        } else {
            // Not yet at frame 1: increment counter
            self->saveFrameCount++;
        }
    }
}

// ============================================================
