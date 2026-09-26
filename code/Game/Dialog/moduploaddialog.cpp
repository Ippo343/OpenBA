// code/Game/Dialog/moduploaddialog.cpp
// ModUploadDialog (0xB8 bytes), vtable at 0x14015c6c0.
// Extends DialogInstance (0xB8 base). No extra fields.
//
// A bare DialogInstance with no overrides beyond the standard dtor pattern: every
// other vtable slot (3-9) is a 6-byte JMP thunk to the Runtime, i.e. inherited.
// ENDGAME B.3.8 block DLG: a real class (Dialog/ModUploadDialog.h).

#include "Dialog/ModUploadDialog.h"
#include "Object/factory_registration.h"
#include "object/Internal.h"
#include <new>

ModUploadDialog::ModUploadDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                                 Object::InternalPtr<UI::DialogInstance> parent)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0) {}

// [0] 0x140012840: ~ModUploadDialog -> ~DialogInstance -> ~Internal, MemFree on &1.
Object::Base* ModUploadDialog::DeletingDtor(unsigned flags) {
    this->~ModUploadDialog();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140012bc0: MemAlloc(0xb8, 8), then the ctor.
void* ModUploadDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ModUploadDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) ModUploadDialog());
}
// createInPlace 0x140012c10 (the tree's in-place build is kept, B8-13).
void* ModUploadDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) ModUploadDialog());
}

// The reflection record stays at the TU position of the UI_DIALOG_STUB row it
// replaces (ui_dialog_stub_factories.cpp, B38 D4).
DEFINE_EXE_VTABLE(ModUploadDialog, "_ZTV15ModUploadDialog")
