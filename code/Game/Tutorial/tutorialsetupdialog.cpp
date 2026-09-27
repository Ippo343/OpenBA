// code/Game/Tutorial/tutorialsetupdialog.cpp
// TutorialSetupDialog — the tutorial setup that hides the UI except the
// elements a step names and opens a highlight on the one it wants clicked.
//
// TutorialSetupDialog (0x78 bytes), table 0x140160d20, TutorialSetup ->
// Object::External (NOT a DialogInstance). ENDGAME B.3.8 block GOAL: a real
// class (Tutorial/TutorialSetupDialog.h); the hand-built 48-slot table in
// tutorial_dialog_vtables.cpp is gone. Field map: the header.

#include "Tutorial/TutorialSetupDialog.h"
#include "engine_funcs.h"
#include "system/Filename.h"
#include "system/String.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "object/Internal.h"   // Object::detail::InternalMemAlloc / InternalMemFree
#include "collection/Collection.h"
#include "Object/factory_registration.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K4)
#include "Component/ComponentDescription.h"
#include "Entity/ComponentEntity.h"
#include <cstdint>
#include <new>

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// ctor 0x1400e2cc0: External() (the base's real ctor), the TutorialSetup and
// TutorialSetupDialog stamps (the compiler's), +0x20..+0x58 zeroed, +0x60 = -1,
// +0x68 = 0, +0x70 = 0.
TutorialSetupDialog::TutorialSetupDialog()
    : shownPaths(0), shownPathCount(0), shownPathCapacity(0),
      dialogNames(0), dialogNameCount(0), dialogNameCapacity(0),
      highlightPath(0), arrowRotation(0), _pad4C{}, highlightDialog(nullptr),
      toolbarName(0), toolbarStamp(-1), _pad64{}, toolbarCached(nullptr),
      toolbarIndex(0), _pad74{} {}

// The dtor 0x1400e2bf0: ~String +0x58, ~String +0x40, then the two String
// arrays (+0x30, then +0x20) — each an inlined Collection::Array<System::String>
// ::~Array (~String per element, MemFree the buffer, zero the header). The
// compiler re-stamps TutorialSetup and runs ~External (the tree ran a guarded
// ~Internal instead).
TutorialSetupDialog::~TutorialSetupDialog() {
    reinterpret_cast<System::String*>(&toolbarName)->~String();       // B.3.1 bridge
    reinterpret_cast<System::String*>(&highlightPath)->~String();     // B.3.1 bridge
    reinterpret_cast<Collection::Array<System::String>*>(&dialogNames)->~Array();   // B.3.2 bridge
    reinterpret_cast<Collection::Array<System::String>*>(&shownPaths)->~Array();    // B.3.2 bridge
}

// [0] 0x140028620: the dtor 0x1400e2bf0, MemFree on &1.
Object::Base* TutorialSetupDialog::DeletingDtor(unsigned flags) {
    this->~TutorialSetupDialog();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// Reconstruct [6] 0x140028600 / Destruct [7] 0x140001640 / Construct [11] 0x1400285f0
// ============================================================
// Reconstruct: vtable[0](this, 0), then the ctor 0x1400e2cc0 on `this`.
Object::External* TutorialSetupDialog::Reconstruct() {
    DeletingDtor(0);
    return ::new (static_cast<void*>(this)) TutorialSetupDialog();
}
void TutorialSetupDialog::Destruct() { DeletingDtor(0); }
// Construct (also the createInPlace): `TEST RCX,RCX; JZ; CALL ctor`.
Object::External* TutorialSetupDialog::Construct() {
    return ::new (static_cast<void*>(this)) TutorialSetupDialog();
}

// ============================================================
// TutorialSetupDialog::Serialize (vtable +0x28, 0x1400e2a60)
// ============================================================
// Two String arrays (count, resize on load, elements), the highlight path,
// the highlight Dialog ExternalPtr, the arrow rotation, the toolbar
// ExternalRef (resolved on load) and the toolbar index. Retail does NOT call
// the base TutorialSetup::Serialize here. Returns true (MOV AL,1).
bool TutorialSetupDialog::Serialize(void* stream) {
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    IO::Serializer* ser = reinterpret_cast<IO::Serializer*>(serializer);   // B.3.4 bridge

    // 1st String array (+0x20)
    uint32_t count1 = shownPathCount;
    ser->Serialize(*reinterpret_cast<int*>(&count1), nullptr);
    if (static_cast<int>(ser->GetDirection()) == 0) {   // (IO::Serializer::GetDirection, retail 0x180006210)
        fn_StringArray_Resize(&shownPaths, static_cast<int>(count1));
    }
    if (static_cast<int32_t>(count1) > 0) {
        uint8_t* p = reinterpret_cast<uint8_t*>(shownPaths);
        for (uint32_t i = 0; i < count1; ++i, p += 8) {
            reinterpret_cast<System::String*>(p)->Serialize(serializer);   // B.3.1 bridge
        }
    }

    // 2nd String array (+0x30)
    uint32_t count2 = dialogNameCount;
    ser->Serialize(*reinterpret_cast<int*>(&count2), nullptr);
    if (static_cast<int>(ser->GetDirection()) == 0) {
        fn_StringArray_Resize(&dialogNames, static_cast<int>(count2));
    }
    if (static_cast<int32_t>(count2) > 0) {
        uint8_t* p = reinterpret_cast<uint8_t*>(dialogNames);
        for (uint32_t i = 0; i < count2; ++i, p += 8) {
            reinterpret_cast<System::String*>(p)->Serialize(serializer);   // B.3.1 bridge
        }
    }

    reinterpret_cast<System::String*>(&highlightPath)->Serialize(serializer);   // B.3.1 bridge
    fn_SerializeExternalPtr(&highlightDialog, serializer);   // ExternalPtr<UI::Dialog> (0x140028650)
    ser->Serialize(arrowRotation, nullptr);

    // Temporary Filename retail's compiler emitted as this scope's guard.
    System::Filename tempFilename;
    (void)tempFilename;

    reinterpret_cast<System::String*>(&toolbarName)->Serialize(serializer);   // B.3.1 bridge

    // On load: resolve the ExternalRef at +0x58 (FUN_14001b1d0 inlined): when
    // the factory generation moved, Find the name and keep it if it IsDerivedFrom
    // the building description type.
    if (static_cast<int>(ser->GetDirection()) == 0) {
        void* factory = *g_pExternalFactorySingleton;
        if (toolbarStamp != *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(factory) + 8)) {
            System::Filename filenameBuf(*reinterpret_cast<const System::String*>(&toolbarName));   // B.3.1 bridge
            ::Object::ExternalFactory* efactory =
                reinterpret_cast<::Object::ExternalFactory*>(factory);  // B.3.3 bridge
            void* found = const_cast<void*>(static_cast<const void*>(efactory->Find(filenameBuf)));
            bool valid = found && fn_IsDerivedFrom(found, ComponentDescription::TypeId());
            toolbarCached = valid ? found : nullptr;
            toolbarStamp = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(factory) + 8);
        }
    }

    ser->Serialize(toolbarIndex, nullptr);

    ser->~Serializer();
    return true;
}

// ============================================================
// TutorialSetupDialog::Begin (vtable +0x40, 0x1400e25f0)
// ============================================================
// The step opens: show the listed elements, open the highlight (without the
// already-open check), then for every named dialog that is open: follow its
// building (a building description data source) or close it (clearing edit
// mode first when the data source is a ComponentEntity).
void TutorialSetupDialog::Begin() {
    fn_TSD_Slot9Helper(this);
    fn_TSD_OpenDialog(this, '\0');

    int count2 = static_cast<int32_t>(dialogNameCount);
    for (int i = 0; i < count2; ++i) {
        uint64_t instance = 0;
        void* strPtr = reinterpret_cast<void*>(dialogNames + static_cast<uintptr_t>(i) * 8);
        FindDialogByName_Impl(&instance, strPtr);
        if (instance == 0) continue;

        void* basePtr = *reinterpret_cast<void**>(instance + 0x30);   // DialogInstance::_dataSource

        // Original uses IsA first (null/type-safe), then IsDerivedFrom
        bool isBuild = fn_IsA(basePtr, ComponentDescription::TypeId());
        if (isBuild) {
            bool derived = fn_IsDerivedFrom(basePtr, ComponentDescription::TypeId());
            void* entityForFollow = derived ? basePtr : nullptr;
            uint64_t retval[2] = {0, 0};
            uintptr_t gameSingleton = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
            void* playerController = *reinterpret_cast<void**>(gameSingleton + 0x120);
            fn_SetFollowBuilding(playerController, retval, reinterpret_cast<uint64_t>(entityForFollow));
        } else {
            // Original: SetEditMode only if IsA(ComponentEntity), but CloseDialog always
            bool isEntity = fn_IsA(basePtr, ComponentEntity::TypeId());
            uintptr_t gameSingleton = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
            if (isEntity) {
                void* playerController = *reinterpret_cast<void**>(gameSingleton + 0x120);
                fn_SetEditMode(playerController, 0);
            }
            void* ui = *reinterpret_cast<void**>(gameSingleton + 0x98);
            uint64_t instCopy = instance;
            reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(  // B.3.6 bridge
                reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&instCopy));
        }
    }
}

// ============================================================
// TutorialSetupDialog::Update (vtable +0x48, 0x1400e24c0)
// ============================================================
// Every tick: re-show the listed elements, re-open the highlight unless one
// is already open. `time` is unused.
void TutorialSetupDialog::Update(void* /*time*/) {
    fn_TSD_Slot9Helper(this);
    fn_TSD_OpenDialog(this, '\x01');
}

// ============================================================
// TutorialSetupDialog::End (vtable +0x50, 0x1400e2460)
// ============================================================
// The step completes: un-hide every dialog, close the open highlight.
void TutorialSetupDialog::End() {
    fn_TSD_SetDialogFlags(this, '\x01');

    uint64_t instance = 0;
    fn_TSD_FindInstance(this, &instance);
    if (instance != 0) {
        uintptr_t gameSingleton = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
        void* ui = *reinterpret_cast<void**>(gameSingleton + 0x98);
        uint64_t instPtr = instance;
        reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(  // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&instPtr));
    }
}

// create 0x1400285b0: MemAlloc(0x78, 8), then the ctor 0x1400e2cc0.
void* TutorialSetupDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialSetupDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialSetupDialog());
}
// createInPlace 0x1400285f0.
void* TutorialSetupDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialSetupDialog());
}

DEFINE_EXE_VTABLE(TutorialSetupDialog, "_ZTV19TutorialSetupDialog")
// DEFINE_EXE_TYPE(TutorialSetupDialog, "TutorialSetup") stays where its
// REGISTER_TYPE was — tutorial_dialog_vtables.cpp (B38_RULES B8-7).
