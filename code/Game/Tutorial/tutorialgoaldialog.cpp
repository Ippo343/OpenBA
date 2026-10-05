// code/Game/Tutorial/tutorialgoaldialog.cpp
// TutorialGoalDialog — a tutorial goal met while a named dialog is open.
//
// TutorialGoalDialog (0x40 bytes), table 0x140160870, TutorialGoal ->
// Object::External (NOT a DialogInstance). ENDGAME B.3.8 block GOAL: a real
// class (Tutorial/TutorialGoalDialog.h); the hand-built 48-slot table in
// tutorial_dialog_vtables.cpp is gone.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/TutorialGoalDialog.h"
#include "object/External.h"
#include "Tutorial/exe_bodies.h"
#include "Component/exe_bodies.h"
#include "object/TypeInfo.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/Internal.h"   // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include "Component/ComponentDescription.h"
#include "Entity/ComponentEntity.h"
#include <cstdint>
#include <new>

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// ctor (inlined in create 0x140027440, Construct 0x1400274b0 and Reconstruct
// 0x140027520): External(), the TutorialGoal and TutorialGoalDialog stamps
// (the compiler's), then +0x20 = +0x28 = 0, +0x30 = -1, +0x38 = 0.
TutorialGoalDialog::TutorialGoalDialog()
    : dialogName(), target() {}

// B4 X7-GAME-A (H5): the +0x20 String is a member: String() is the qword 0 the
// ctor stored; its ~String runs after the dtor body, i.e. after target's name,
// retail's order (0x140027590: ~String +0x28, then +0x20).
System::String& TutorialGoalDialog::DialogName() {
    return dialogName;
}

// The dtor inside [0] 0x140027590: ~String on +0x28, then +0x20; the compiler
// re-stamps TutorialGoal and runs ~External (the tree ran a guarded ~Internal).
TutorialGoalDialog::~TutorialGoalDialog() {
    // B4 Y GAMEA: +0x28 target's ~String (0x1400275c0), then +0x20 (0x1400275cb):
    // the members', after this empty body.
}

// ============================================================
// TutorialGoalDialog::Serialize (vtable +0x28, 0x1400defd0)
// ============================================================
// 1. TutorialGoal::Serialize (its empty Serializer scope, inlined in retail)
// 2. a Serializer over the stream: String +0x20, String +0x28; on load the
//    ExternalRef at +0x28 is resolved by name.
bool TutorialGoalDialog::Serialize(IO::Stream& stream) {
    TutorialGoal::Serialize(stream);   // QUALIFIED: the base body

    IO::Serializer ser(stream, 0);

    DialogName().Serialize(ser);

    // Filename temp retail's compiler emitted for this scope; only ever
    // constructed and destroyed.
    System::Filename tempFilename;
    (void)tempFilename;

    target.name.Serialize(ser);

    // On load (direction 0): resolve the ExternalRef at +0x28 by name.
    if (static_cast<int>(ser.GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
        ResolveExternalByName_Impl(&target);
    }
    return true;
}

// ============================================================
// Reconstruct [6] 0x140027520 / Destruct [7] 0x140001640 / Construct [9] 0x1400274b0
// ============================================================
// Reconstruct: vtable[0](this, 0), then the ctor on `this`.
Object::External* TutorialGoalDialog::Reconstruct() {
    this->~TutorialGoalDialog();
    return ::new (static_cast<void*>(this)) TutorialGoalDialog();
}
void TutorialGoalDialog::Destruct() { this->~TutorialGoalDialog(); }
Object::External* TutorialGoalDialog::Construct() {
    return ::new (static_cast<void*>(this)) TutorialGoalDialog();
}

// ============================================================
// TutorialGoalDialog::IsComplete (vtable +0x40, 0x1400deee0)
// ============================================================
// Finds the open dialog by name (+0x20). Met when it is open and either no
// target description is named, or the dialog's data source is that
// description (directly, or through a ComponentEntity's +0x28 Description).
bool TutorialGoalDialog::IsComplete(const System::Time* /*time*/, void* /*instance*/) {
    Object::InternalPtr<UI::DialogInstance> dialogPtr;
    FindDialogByName_Impl(&dialogPtr, &DialogName());
    if (dialogPtr.Get() == nullptr) return false;   // dialog not open

    // The dialog instance's data source (DialogInstance +0x30).
    Object::Base* userData = dialogPtr.Get()->_dataSource;
    const Object::External* description = nullptr;
    if (userData != nullptr) {
        if (Object::IsDerivedFromNullSafe(userData, ComponentDescription::TypeId())) {
            description = static_cast<const ComponentDescription*>(userData);
        } else if (Object::IsDerivedFromNullSafe(userData, ComponentEntity::TypeId())) {
            description = static_cast<const ComponentEntity*>(userData)->_description;
        }
    }

    ResolveExternalByName_Impl(&target);
    if (target.resolved != nullptr && target.resolved != description) return false;
    return true;
}

// create 0x140027440: MemAlloc(0x40, 8), then the (inlined) ctor.
::Object::Base* TutorialGoalDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialGoalDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialGoalDialog());
}
// createInPlace 0x1400274b0.
::Object::Base* TutorialGoalDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialGoalDialog());
}

// DEFINE_EXE_TYPE(TutorialGoalDialog, "TutorialGoal") stays where its
// REGISTER_TYPE was — tutorial_dialog_vtables.cpp — so the registration order
// is unchanged (B38_RULES B8-7).
