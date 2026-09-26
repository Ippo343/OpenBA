#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "Dialog/MessageDialog.h"
#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ENDGAME B.3.8 block DLG: MessageDialog is a real class (Dialog/MessageDialog.h)
// with a compiler-generated vtable. Retail evidence per slot is in the header.

namespace {
// The shared tail of slots 10-13: result into +0x24, then
// UserInterface::CloseDialog(InternalPtr(this)).
void CloseWithResult(MessageDialog* self, int32_t result) {
    self->_result = result;
    uint8_t selfIBP[16];
    new (selfIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
    static_cast<UI::UserInterface*>(self->_ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(selfIBP));  // B.3.6 bridge
}
}  // namespace

// [4] OnOpened (0x14006D140) — Setup
// Finds buttonCancel, buttonOk, buttonYes, buttonNo elements by name.
// Sets element IDs: Cancel=0, Ok=1, Yes=2, No=3.
void MessageDialog::OnOpened() {
    struct { const char* name; int id; } buttons[] = {
        { "buttonCancel", 0 },
        { "buttonOk",     1 },
        { "buttonYes",    2 },
        { "buttonNo",     3 },
    };

    for (auto& btn : buttons) {
        System::String nameStr(btn.name);
        Object::InternalPtr<UI::Element> found = FindElement(nameStr);
        void* elem = found.Get();

        if (elem) {
            static_cast<UI::Element*>(elem)->SetElementId(btn.id);
        }
    }
}

// [5] OnClosed (0x14006D0C0) — `RET 0`: an empty override of the base body.
void MessageDialog::OnClosed() {}

// [6] / [7] PORT GAP (B38 D3.1): retail inherits DialogInstance::OnUpdateLayout
// / OnUpdate; the hand-built table had a no-op in both. Kept for this commit.
void MessageDialog::OnUpdateLayout() {}
void MessageDialog::OnUpdate(const System::Time& /*time*/) {}

// [8] OnElementEvent (0x14006D070) — dispatches a button click (msg 0x12)
// through the vtable, so a subclass override runs:
//   id 0 -> [11] OnCancel, 1 -> [10] OnOk, 2 -> [12] OnYes, 3 -> [13] OnNo.
void MessageDialog::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/,
                                   int32_t a, int32_t b, int32_t /*c*/,
                                   int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (a != 0x12)
        return;

    switch (b) {
    case 0: OnCancel(); break;
    case 1: OnOk();     break;
    case 2: OnYes();    break;
    case 3: OnNo();     break;
    }
}

// [10] 0x14006CFF0 / [11] 0x14006D030 / [12] 0x14006CFB0 / [13] 0x14006CF70:
// result 1 / 0 / 2 / 3 into +0x24, then close.
void MessageDialog::OnOk()     { CloseWithResult(this, 1); }
void MessageDialog::OnCancel() { CloseWithResult(this, 0); }
void MessageDialog::OnYes()    { CloseWithResult(this, 2); }
void MessageDialog::OnNo()     { CloseWithResult(this, 3); }

// ============================================================
// Construction, destruction, reflection
// ============================================================
// Original ctor (decompiled @ 0x14006d300): DialogInstance::DialogInstance then
// the vtable store. No additional fields beyond DialogInstance's.

#include "Object/factory_registration.h"
#include "address_registry.h"
#include "resolve.h"
#include <cstdio>

MessageDialog::MessageDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                             Object::InternalPtr<UI::DialogInstance> parent)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0) {}

// [0] 0x140012660: ~MessageDialog -> ~DialogInstance -> ~Internal, MemFree on &1.
Object::Base* MessageDialog::DeletingDtor(unsigned flags) {
    this->~MessageDialog();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x1400125d0: MemAlloc(0xc0, 8), then the default ctor.
void* MessageDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MessageDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) MessageDialog());
}
// createInPlace 0x140012620 (retail returns null for Internal-rooted classes;
// the tree's in-place build is kept, B8-13).
void* MessageDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) MessageDialog());
}

// The ctor DOOR, reached by address: engine_funcs.cpp resolves 0x14006d300 into
// fn_MessageDialog_Ctor. Signature matches fn_MessageDialog_Ctor_t:
//   (mem, userInterface, externalResource, internalBasePtrOrNull) -> void*
void* MessageDialog_Ctor_Standalone(void* self, void* ui, void* extRes,
                                    void* parentIBP) {
    if (!self) return nullptr;
    printf("[MessageDialog_Ctor] self=%p ui=%p extRes=%p parentIBP=%p\n",
                self, ui, extRes, parentIBP);
    Object::InternalPtr<UI::DialogInstance> parent;
    if (parentIBP)
        parent = *static_cast<const Object::InternalPtr<UI::DialogInstance>*>(parentIBP);
    return ::new (self) MessageDialog(static_cast<UI::UserInterface*>(ui),
                                      static_cast<const UI::Dialog*>(extRes), parent);
}

DEFINE_EXE_VTABLE(MessageDialog, "_ZTV13MessageDialog")

// B38 D5: engine_funcs.cpp:1742 still reads 0x14015c480 back
// (g_ReloadDialog_Vtable), so the exe address serves the compiler table.
extern "C" void RegisterMessageDialogVtable() {
    RegisterExeAddr(0x14015c480,
                    reinterpret_cast<uintptr_t>(MessageDialog::Vtable()));
    RegisterExeAddr(0x14006d300,
                    reinterpret_cast<uintptr_t>(&MessageDialog_Ctor_Standalone));
}

DEFINE_EXE_TYPE(MessageDialog, "UI::DialogInstance")
