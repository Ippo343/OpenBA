// code/Game/Dialog/optioninputbinddialog.cpp
// OptionInputBindDialog (0xC8 bytes): the key-binding capture dialog.
// Extends MessageDialog (which extends DialogInstance at 0xB8 base).
// ENDGAME B.3.8 block DLG: a real class (Dialog/OptionInputBindDialog.h).
//
// Layout:
//   +0xB8  (from MessageDialog, uninitialized)
//   +0xC0  keyIndex (int32): captured key/button index
//   +0xC4  inputType (int32): 0=none, 1=keyboard, 2=mouse, -1=initial
//
// The dialog captures a single keypress or mouse button click and stores
// the result. The parent OptionsDialog reads +0xC0 and +0xC4 after close.
// Every slot except [0] and [7] is MessageDialog's (retail table 0x14015ce40
// points at MessageDialog's bodies); the old hand-built table carried
// byte-identical copies of them, now inherited.

#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>
#include "input/Input.h"   // ENDGAME B.3.7 callers (K3)
#include "Dialog/OptionInputBindDialog.h"
#include "system/Value.h"
#include "ui/UserInterface.h"
#include "runtime/types/ui/DialogInstance.h"
#include "object/TypeInfo.h"
#include "Object/factory_registration.h"
#include "address_registry.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "input/Device.h"   // ENDGAME B.3.9 SS2: Input::Device::_singleton
#include "Dialog/exe_bodies.h"

// ============================================================
// [7] OnUpdate (0x140071c80): scan for key/mouse input releases
// ============================================================
// Polls Input::Device::Released for all keyboard keys (0..0x68)
// and mouse buttons (4..0xB). When a release is detected, stores
// the key index at +0xC0, input type (1=keyboard, 2=mouse) at +0xC4,
// and calls [10] OnOk (through the vtable) to close with result=1.
void OptionInputBindDialog::OnUpdate(const System::Time& /*time*/) {
    void* inputDevice = ::Input::Device::_singleton;

    // Scan keyboard keys (InputDevice=1, keys 0..0x68)
    for (int key = 0; key < 0x69; key++) {
        if (reinterpret_cast<const Input::Device*>(inputDevice)->Released(Input::Device::InputDevice::Keyboard, 0, key)) {  // B.3.7 bridge
            inputType = 1;  // keyboard
            keyIndex = key;
            OnOk();
            return;
        }
    }

    // Scan mouse buttons (InputDevice=2, buttons 4..0xB)
    for (int btn = 4; btn <= 0xB; btn++) {
        if (reinterpret_cast<const Input::Device*>(inputDevice)->Released(Input::Device::InputDevice::Mouse, 0, btn)) {  // B.3.7 bridge
            inputType = 2;  // mouse
            keyIndex = btn;
            OnOk();
            return;
        }
    }
}

// ============================================================
// Construction, destruction, reflection
// ============================================================
// Original ctor (decompiled @ 0x140071d30):
//     UI::DialogInstance::DialogInstance(self, ui, extRes, &localParent, 0,0,0);
//     *self = MessageDialog's table (0x14015c480);
//     *self = OptionInputBindDialog's table (0x14015ce40);
//     *(int*)(self + 0xC0) = 0;
//     *(int*)(self + 0xC4) = 0xFFFFFFFF;       // -1
// The compiler stamps the two tables (MessageDialog's ctor body is empty).

OptionInputBindDialog::OptionInputBindDialog() : keyIndex(0), inputType(-1) {}

OptionInputBindDialog::OptionInputBindDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                                             Object::InternalPtr<UI::DialogInstance> parent)
    : MessageDialog(ui, dialog, parent), keyIndex(0), inputType(-1) {}

// create 0x140012e60: MemAlloc(0xc8, 8), then the ctor.
void* OptionInputBindDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(OptionInputBindDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) OptionInputBindDialog());
}
// createInPlace 0x140012ed0 (tree's in-place build kept, B8-13).
void* OptionInputBindDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) OptionInputBindDialog());
}

// The ctor DOOR, reached by address: engine_funcs.cpp resolves 0x140071d30 into
// fn_OptionInputBindDialog_Ctor. Signature matches fn_OptionInputBindDialog_Ctor_t:
//   (mem, userInterface, externalResource, internalBasePtrOrNull) -> void*
void* OptionInputBindDialog_Ctor_Standalone(void* self, void* ui, void* extRes,
                                            void* parentIBP) {
    if (!self) return nullptr;
    printf("[OptionInputBindDialog_Ctor] self=%p ui=%p extRes=%p parentIBP=%p\n",
                self, ui, extRes, parentIBP);
    Object::InternalPtr<UI::DialogInstance> parent;
    if (parentIBP)
        parent = *static_cast<const Object::InternalPtr<UI::DialogInstance>*>(parentIBP);
    return ::new (self) OptionInputBindDialog(static_cast<UI::UserInterface*>(ui),
                                              static_cast<const UI::Dialog*>(extRes), parent);
}


// Called from RegisterCodeFunctions in standalone/address_registry.cpp. The
// vtable row (0x14015ce40) is gone: nothing in the tree reads that address back
// (B38 D5). The ctor door stays.
extern "C" void RegisterOptionInputBindDialogVtable() {
    RegisterExeAddr(0x140071d30,
                    reinterpret_cast<uintptr_t>(&OptionInputBindDialog_Ctor_Standalone));
}

// Parent "MessageDialog" — retail RegisterType_OptionInputBindDialog 0x14014c480.
DEFINE_EXE_TYPE(OptionInputBindDialog, "MessageDialog")
