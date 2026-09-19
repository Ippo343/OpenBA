// code/Game/Dialog/tutorialsetupdialog.cpp
// Clean reimplementation of TutorialSetupDialog vtable methods.
//
// TutorialSetupDialog (0x78 bytes), vtable at 0x140160d20.
// Extends Object::External (NOT DialogInstance). Base ~0x20 bytes.
//
// Struct layout (from Ctor + Shutdown decompilation):
//   +0x00: vtable ptr
//   +0x08: Object::External base data (0x18 bytes)
//   +0x20: void* — string array ptr (1st vector data)
//   +0x28: uint32_t — count of 1st string vector
//   +0x2C: uint32_t — capacity of 1st string vector (managed by fn_StringArray_Resize)
//   +0x30: void* — string array ptr (2nd vector data)
//   +0x38: uint32_t — count of 2nd string vector
//   +0x3C: uint32_t — capacity of 2nd string vector
//   +0x40: System::String (inline, 8 bytes)
//   +0x48: int32_t field
//   +0x4C: padding
//   +0x50: External* — external resource pointer (serialized as reference)
//   +0x58: System::String (inline, 8 bytes) — external path string
//   +0x60: int32_t = -1 sentinel
//   +0x64: padding
//   +0x68: void* pointer field
//   +0x70: uint32_t field
//   +0x74: padding
//
// All exe function pointers (TSD slot helpers, SerializeExternalPtr, etc.) are
// now resolved centrally in engine_funcs.cpp — no ResolveAddr in this file.

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include "system/Filename.h"
#include "system/String.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "resolve.h"
#include "collection/Collection.h"
#include <cstdint>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// ============================================================
// TutorialSetupDialog::Slot3 (vtable +0x18, exe 0x1400e2be0): MOV EAX,6; RET
// ============================================================
// Returns 6 — indicates this External subclass type (constant).
int TutorialSetupDialog_Slot3(void* self) {
    (void)self;
    return 6;
}

// ============================================================
// TutorialSetupDialog::Slot7 (vtable +0x38, exe 0x140001640): Delegator
// ============================================================
// MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]
// Calls vtable slot 0 (scalar dtor) with flags=0 (destruct without freeing).
void TutorialSetupDialog_Slot7(void* self) {
    typedef void* (*fn_VDtor_t)(void* self, uint32_t flags);
    void** vtbl = *(void***)self;
    fn_VDtor_t vdtor = (fn_VDtor_t)vtbl[0];
    vdtor(self, 0);
}

// ============================================================
// TutorialSetupDialog::Slot11 (vtable +0x58, exe 0x1400285f0): Ctor wrapper
// ============================================================
// TEST RCX,RCX / JZ skip / CALL TutorialSetupDialog_Ctor / RET
// This IS the TutorialSetupDialog constructor entry point (also used as a vtable slot).
// If self is null, returns null. Otherwise delegates to the full Ctor at 0x1400e2cc0.
void* TutorialSetupDialog_Slot11(void* self) {
    if (self == nullptr) return nullptr;

    // Delegate to the full TutorialSetupDialog Ctor (same one used in OnEvent)
    typedef void* (*fn_TSDCtor_t)(void* self);
    static fn_TSDCtor_t fn_Ctor = nullptr;
    if (!fn_Ctor) fn_Ctor = (fn_TSDCtor_t)ResolveAddr(Module::Exe, TutorialSetupDialogAddr::Ctor);
    return fn_Ctor(self);
}

// ============================================================
// TutorialSetupDialog::ScalarDtor (vtable slot 0, exe 0x140028620)
// ============================================================
//
// Sets TutorialSetupDialog vtable, destroys inline strings and string vector arrays,
// calls Object::External::~External, then conditionally frees the object memory.
//
// The "body" sub-function (FUN_1400e2bf0) is inlined here since it is only
// called from this destructor:
//   1. Restore TutorialSetupDialog vtable
//   2. Destroy inline String at +0x58
//   3. Destroy inline String at +0x40
//   4. Destroy each String in the 2nd vector (ptrs at *(+0x30), count at +0x38)
//   5. Free the 2nd vector buffer; zero +0x30 and +0x38
//   6. Destroy each String in the 1st vector (ptrs at *(+0x20), count at +0x28)
//   7. Free the 1st vector buffer; zero +0x20 and +0x28
//   8. Restore TutorialSetup base vtable (0x140160c30)
//   9. Call Object::External::~External
//  10. Conditionally call MemFreeStatic if flags & 1
void* TutorialSetupDialog_ScalarDtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Step 1: restore TutorialSetupDialog vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, TutorialSetupDialogAddr::Vtable);
    *(void**)self = s_vtable;

    // Step 2: destroy inline String at +0x58
    reinterpret_cast<System::String*>(s + 0x58)->~String();

    // Step 3: destroy inline String at +0x40
    reinterpret_cast<System::String*>(s + 0x40)->~String();

    // Step 4-7: destroy the 2nd (+0x30) then the 1st (+0x20) String vector.
    // ENDGAME B.3.2 step 2: each WAS an inlined Collection::Array<System::String>::
    // ~Array — ~String on every live element (8-byte stride), MemFree the buffer
    // (null-guarded), zero _data, zero the _size/_capacity qword. The trait-derived
    // Array<System::String> row (destroy == ~String per element, byteCopy false) is
    // that body: `if (destroy && _size > 0) destroy(...)` then ArrayDtorImpl. The old
    // code additionally skipped the element loop when the buffer was null with a
    // positive count, a state no consistent header can be in.
    // B.3.2 bridge ×2: TutorialSetupDialog is still an untyped exe struct.
    reinterpret_cast<Collection::Array<System::String>*>(s + 0x30)->~Array();
    reinterpret_cast<Collection::Array<System::String>*>(s + 0x20)->~Array();

    // Step 8: restore TutorialSetup base vtable
    static void* s_baseVtable = nullptr;
    if (!s_baseVtable) s_baseVtable = (void*)ResolveAddr(Module::Exe, TutorialSetupDialogAddr::BaseVtable);
    *(void**)self = s_baseVtable;

    // Step 9: call Object::External::~External
    fn_External_Dtor(self);

    // Step 10: conditional memory free
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// TutorialSetupDialog::Shutdown (vtable slot 5, exe 0x1400e2a60)
// ============================================================
//
// Serialization method: saves/loads tutorial setup state via IO::Serializer.
// Handles two dynamic String vectors, two inline Strings, an External pointer,
// two int fields, and resolves external reference by name on load.
//
// Returns 1 (true) always.
//
// StringArray_Resize, SerializeExternalPtr and ResolveExternalByName come from
// engine_funcs.h.

uint64_t TutorialSetupDialog_Shutdown(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    // Create the serializer on the stack (24 bytes for Serializer object)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize 1st string vector count at +0x28
    uint32_t count1 = *(uint32_t*)(s + 0x28);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&count1), nullptr);   // B.3.4 bridge

    // On load (serializer mode == 0): resize 1st vector to the loaded count
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        fn_StringArray_Resize((void*)(s + 0x20), (int)count1);
    }

    // Serialize each string in 1st vector
    if ((int32_t)count1 > 0) {
        uint8_t* p = *(uint8_t**)(s + 0x20);
        for (uint32_t i = 0; i < count1; ++i, p += 8) {
            reinterpret_cast<System::String*>(p)->Serialize(serializer);
        }
    }

    // Serialize 2nd string vector count at +0x38
    uint32_t count2 = *(uint32_t*)(s + 0x38);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&count2), nullptr);   // B.3.4 bridge

    // On load: resize 2nd vector
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        fn_StringArray_Resize((void*)(s + 0x30), (int)count2);
    }

    // Serialize each string in 2nd vector
    if ((int32_t)count2 > 0) {
        uint8_t* p = *(uint8_t**)(s + 0x30);
        for (uint32_t i = 0; i < count2; ++i, p += 8) {
            reinterpret_cast<System::String*>(p)->Serialize(serializer);
        }
    }

    // Serialize inline String at +0x40 (B.3.1 bridge: dialog is untyped bytes)
    reinterpret_cast<System::String*>(s + 0x40)->Serialize(serializer);

    // Serialize External* pointer at +0x50 (as external reference)
    fn_SerializeExternalPtr((void*)(s + 0x50), serializer);

    // Serialize int32 at +0x48
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x48), nullptr);   // B.3.4 bridge

    // Temporary Filename retail's compiler emitted as this scope's guard.
    System::Filename tempFilename;
    (void)tempFilename;

    // Serialize inline String at +0x58 (external path)
    reinterpret_cast<System::String*>(s + 0x58)->Serialize(serializer);

    // On load: resolve the external resource by name (FUN_14001b1d0).
    // Checks factory revision stamp at param+8 vs factory+8; if changed,
    // does Find+IsDerivedFrom(BuildComponent), stores result at param+0x10.
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        void* namePtr = (void*)(s + 0x58);   // System::String at self+0x58
        void* factory = *g_pExternalFactorySingleton;
        if (*(int32_t*)((uintptr_t)namePtr + 8) != *(int32_t*)((uintptr_t)factory + 8)) {
            // Construct Filename from the String and look it up in the factory
            System::Filename filenameBuf(*reinterpret_cast<const System::String*>(namePtr));
            ::Object::ExternalFactory* efactory =
                reinterpret_cast<::Object::ExternalFactory*>(factory);  // B.3.3 bridge
            void* found = const_cast<void*>(static_cast<const void*>(efactory->Find(filenameBuf)));
            // Cache the result if it passes the BuildComponent type check
            bool valid = found && g_TypeId_ComponentDescription_FC && fn_IsDerivedFrom(found, *g_TypeId_ComponentDescription_FC);
            *(void**)((uintptr_t)namePtr + 0x10) = valid ? found : nullptr;
            *(int32_t*)((uintptr_t)namePtr + 8) = *(int32_t*)((uintptr_t)factory + 8);
        }
    }

    // Serialize int32 at +0x70
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x70), nullptr);   // B.3.4 bridge

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge

    return 1;
}

// ============================================================
// TutorialSetupDialog::OnEvent (vtable slot 6, exe 0x140028600)
// ============================================================
//
// Resets the TutorialSetupDialog to a clean state:
//   1. Calls vtable[0](self, 0) — virtual destructor with no-free flag,
//      which destroys all sub-objects (strings, vectors, External dtor)
//   2. Re-constructs the object by calling TutorialSetupDialog_Ctor,
//      which sets vtables and zeros all fields.
//
// This is a "reset" operation used when the tutorial setup needs to be reused.
void TutorialSetupDialog_OnEvent(void* self) {
    // Step 1: call virtual destructor (slot 0) with flags=0 (destruct without freeing)
    typedef void* (*fn_VDtor_t)(void* self, uint32_t flags);
    void** vtbl = *(void***)self;
    fn_VDtor_t vdtor = (fn_VDtor_t)vtbl[0];
    vdtor(self, 0);

    // Step 2: re-construct via TutorialSetupDialog_Ctor
    // (sets External base, both vtables, zeros all fields)
    typedef void* (*fn_TSDCtor_t)(void* self);
    static fn_TSDCtor_t fn_Ctor = nullptr;
    if (!fn_Ctor) fn_Ctor = (fn_TSDCtor_t)ResolveAddr(Module::Exe, TutorialSetupDialogAddr::Ctor);
    fn_Ctor(self);
}

// ============================================================
// TutorialSetupDialog::Slot8 (vtable +0x40, exe 0x1400e25f0)
// ============================================================
//
// Called when the player selects an entity from the tutorial setup list.
// Calls setup helper (fn_TSD_Slot9Helper) then validates dialog state
// (fn_TSD_OpenDialog(self, false)).
// Iterates the 2nd string vector (+0x30/+0x38): for each slot, looks up the matching
// open dialog instance by type name string. If found, reads the Base* at instance+0x30:
//   - If entity IsA BuildComponent: IsDerivedFrom check, calls fn_SetFollowBuilding
//     to set the player's followed building entity on the playerController.
//   - Else if entity IsA ComponentEntity: calls fn_SetEditMode(playerController, 0)
//     to clear edit selection, then CloseDialog on the dialog instance.
// This implements the "click on a tutorial target building" selection flow.
void TutorialSetupDialog_Slot8(void* self) {
    uintptr_t s = (uintptr_t)self;

    fn_TSD_Slot9Helper(self);
    fn_TSD_OpenDialog(self, '\0');

    int count2 = *(int32_t*)(s + 0x38);
    if (count2 <= 0) return;

    for (int i = 0; i < count2; ++i) {
        uint64_t instance = 0;
        void* strPtr = (void*)(*(uintptr_t*)(s + 0x30) + (uintptr_t)i * 8);
        FindDialogByName_Impl(&instance, strPtr);
        if (instance == 0) continue;

        void* basePtr = *(void**)(instance + 0x30);

        // Original uses IsA first (null/type-safe), then IsDerivedFrom
        bool isBuild = fn_IsA(basePtr, *g_TypeId_ComponentDescription_FC);
        if (isBuild) {
            bool derived = fn_IsDerivedFrom(basePtr, *g_TypeId_ComponentDescription_FC);
            void* entityForFollow = derived ? basePtr : nullptr;
            uint64_t retval[2] = {0, 0};
            uintptr_t gameSingleton = (uintptr_t)(*g_pGameWorldSingleton);
            void* playerController = *(void**)(gameSingleton + 0x120);
            fn_SetFollowBuilding(playerController, retval, (uint64_t)entityForFollow);
        } else {
            // Original: SetEditMode only if IsA(ComponentEntity), but CloseDialog always
            bool isEntity = fn_IsA(basePtr, *g_TypeId_ComponentEntity);
            uintptr_t gameSingleton = (uintptr_t)(*g_pGameWorldSingleton);
            if (isEntity) {
                void* playerController = *(void**)(gameSingleton + 0x120);
                fn_SetEditMode(playerController, 0);
            }
            void* ui = *(void**)(gameSingleton + 0x98);
            uint64_t instCopy = instance;
            fn_UI_CloseDialog(ui, &instCopy);
        }
    }
}

// ============================================================
// TutorialSetupDialog::Slot9 (vtable +0x48, exe 0x1400e24c0)
// ============================================================
//
// Called to activate the tutorial setup — runs setup helper then opens the dialog
// with param=true (indicating a new setup/start rather than just validation).
//
// fn_TSD_Slot9Helper(self): resets all open dialogs to hidden
//   (fn_TSD_SetDialogFlags(self, false)),
//   then iterates the 1st string vector (+0x20/+0x28) and shows the matching ones.
// fn_TSD_OpenDialog(self, true): checks/opens the tutorial setup dialog resource
//   (reads +0x40/+0x50/+0x58/+0x68 fields) if GetAddDialogCount == 0.
void TutorialSetupDialog_Slot9(void* self) {
    fn_TSD_Slot9Helper(self);
    fn_TSD_OpenDialog(self, '\x01');
}

// ============================================================
// TutorialSetupDialog::Slot10 (vtable +0x50, exe 0x1400e2460)
// ============================================================
//
// Closes the currently open TutorialSetupDialog instance.
// Sets all dialogs to shown state (fn_TSD_SetDialogFlags(self, true)),
// finds the current TutorialSetupDialog instance, and if found, calls
// UI::UserInterface::CloseDialog on it.
void TutorialSetupDialog_Slot10(void* self) {
    // Show all dialogs (unhide)
    fn_TSD_SetDialogFlags(self, '\x01');

    // Find current TutorialSetupDialog instance
    uint64_t instance = 0;
    fn_TSD_FindInstance(self, &instance);

    if (instance != 0) {
        // CloseDialog
        uintptr_t gameSingleton = (uintptr_t)(*g_pGameWorldSingleton);
        void* ui = *(void**)(gameSingleton + 0x98);
        uint64_t instPtr = instance;
        fn_UI_CloseDialog(ui, &instPtr);
    }
}
