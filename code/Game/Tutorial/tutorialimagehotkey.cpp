// code/Game/Tutorial/tutorialimagehotkey.cpp
// Clean reimplementation of TutorialImageHotKey vtable methods.
//
// TutorialImageHotKey (0x48 bytes) — displays hot key image in tutorial steps.
// Vtable: 0x140160ab0
// Extends TutorialImage -> Object::External
//
// Fields (from Ghidra decompilation of Ctor at 0x1400e0680):
//   +0x00: vtable pointer
//   +0x08: External base (0x18 bytes)
//   +0x20: void* imageRef — External pointer to image resource (TutorialImage base)
//   +0x28: int32_t field28 — base image param 1 (serialized)
//   +0x2c: int32_t field2c — base image param 2 (serialized)
//   +0x30: System::String hotkeyName — hotkey identifier string
//   +0x38: int32_t field38 — initialized to -1
//   +0x3c: padding
//   +0x40: void* field40 — zeroed (pointer, used in Slot9 for KeyMap lookup)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/TutorialImageHotKey.h"
#include "Object/factory_registration.h"
#include "ui/UserInterface.h"
#include "Tutorial/exe_bodies.h"
#include "object/TypeInfo.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/Dialog.h"         // ENDGAME B.3.6 UI callers (K4)
#include "ui/Element.h"        // ENDGAME B.3.6 UI callers (K4)
#include "ui/TextElement.h"    // ENDGAME B.3.6 UI callers (K4)
#include "input/Input.h"       // ENDGAME B.3.7 callers (K3)

// hotkeytooltipui.cpp - the existing byte-faithful port of exe 0x140099c20.
namespace Game { void Hotkey_BuildKeyName(void* retval, int device, int keyIndex); }

// ============================================================
// Exe helpers, resolved PER CALL (ENDGAME B.3.8, B6-5: the loader thread
// serializes tutorial resources; no first-call cache)
// ============================================================

// TutorialImage External ref serialize helper (0x140027ad0)
// called by name: TutBase_SerializeImageRef (Tutorial/exe_bodies.h)

// FUN_140027de0 — resolves hotkey string to External ref via ExternalFactory
// called by name: TutBase_ResolveToolbarRef (Tutorial/exe_bodies.h)

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// ctor 0x1400e0680: External(), the TutorialImage stamp + its two zeroed
// fields (TutorialImage()), the own stamp, then the derived fields.
TutorialImageHotKey::TutorialImageHotKey()
    : hotkeyName(0), _field38(-1), _pad3C{}, _field40(nullptr) {}

// The dtor inside [0] 0x140027d70: ~String +0x30; the compiler re-stamps
// TutorialImage and runs ~External.
TutorialImageHotKey::~TutorialImageHotKey() {
    reinterpret_cast<System::String*>(&hotkeyName)->~String();   // B.3.1 bridge
}

// ============================================================
// TutorialImageHotKey::Serialize (vtable +0x28, 0x1400e0540)
// ============================================================
// Phase 1: the TutorialImage fields (int +0x28, int +0x2c, external ref +0x20)
// Phase 2: the hot key string +0x30, resolved on load
bool TutorialImageHotKey::Serialize(void* stream) {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // Phase 1: Base TutorialImage fields
    alignas(8) uint8_t ser1[32];
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<int*>(s + 0x2c), nullptr);   // B.3.4 bridge

    TutBase_SerializeImageRef((void*)(s + 0x20), ser1);

    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Phase 2: HotKey-specific fields
    alignas(8) uint8_t ser2[32];
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize String at +0x30
    reinterpret_cast<System::String*>(s + 0x30)->Serialize(ser2);   // B.3.1 bridge

    // If loading (version == 0), resolve the hotkey string to an External ref
    int version = reinterpret_cast<IO::Serializer*>(ser2)->GetVersion();   // B.3.4 bridge
    if (version == 0) {
        TutBase_ResolveToolbarRef((void*)(s + 0x30));
    }

    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// TutorialImageHotKey::Reconstruct [6] 0x140027d50 / Construct [8] 0x140027d40
// ============================================================
Object::External* TutorialImageHotKey::Reconstruct() {
    this->~TutorialImageHotKey();
    return ::new (static_cast<void*>(this)) TutorialImageHotKey();
}
Object::External* TutorialImageHotKey::Construct() {
    return ::new (static_cast<void*>(this)) TutorialImageHotKey();
}

// ============================================================
// TutorialImageHotKey::Slot9 (vtable +0x48, 0x1400e0420)
// ============================================================
// Creates a UI element from imageRef, finds a TextElement child,
// resolves the hotkey ref, looks up the bound key name from KeyMap,
// and sets the display text on the TextElement.
//
// Params (Win64 ABI, reordered from original):
//   param_1 = self (TutorialImageHotKey*)
//   param_2 = retIBP (InternalBasePtr* — return buffer, also output)
//   param_3 = dialog (Dialog* — parent dialog to create element in)
void* TutorialImageHotKey::CreateElement(void* retIBP, void* dialog) {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // 1. Create UI element from imageRef at +0x20
    ::new (retIBP) Object::InternalPtr<UI::Element>(
        reinterpret_cast<const UI::Dialog*>(dialog)->CreateElement(  // B.3.6 bridge
            Object::ExternalPtr<UI::ElementDescription>(
                reinterpret_cast<UI::ElementDescription*>(*(void**)(s + 0x20)))));

    // 2. CheckPtr on the returned IBP

    // 3. FindElementType — find TextElement child
    void* element = *(void**)retIBP;
    uint8_t foundIBP[16] = {};
    ::new (foundIBP) Object::InternalPtr<UI::Element>(
        reinterpret_cast<UI::Element*>(element)->FindElementType(UI::TextElement::TypeId()));  // B.3.6 bridge

    // 4. CheckPtr on found element

    // 5. IsDerivedFrom check — verify it's a TextElement
    void* foundObj = *(void**)foundIBP;
    void* textElement = nullptr;
    if (foundObj) {
        if (Object::IsDerivedFromNullSafe(foundObj, UI::TextElement::TypeId()))
            textElement = foundObj;
    }

    // 6. Wrap in InternalBasePtr
    void* textRef = nullptr;
    new (&textRef) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(textElement));   // B.3.3 bridge

    // 7. Resolve hotkey ref from string at +0x30
    TutBase_ResolveToolbarRef((void*)(s + 0x30));

    // 8. Get actual KeyMap object (triple deref from exref)
    void* keymap = nullptr;
    if (Input::Configuration::_singleton != nullptr) {
        void* step2 = Input::Configuration::_singleton;
        if (step2) keymap = *(void**)step2;
    }

    // 9. Read key ID from resolved ref at field40->+0x40
    void* resolvedRef = *(void**)(s + 0x40);
    int keyId = 0;
    if (resolvedRef)
        keyId = *(int*)((uintptr_t)resolvedRef + 0x40);

    // 10. Get device type for this key
    int deviceType = 0;
    if (keymap)
        deviceType = reinterpret_cast<const Input::KeyMap*>(keymap)->GetDeviceForId(keyId, 0);  // B.3.7 bridge

    // 11. Resolve hotkey ref again (matches original)
    TutBase_ResolveToolbarRef((void*)(s + 0x30));

    // 12. Get input index for this key
    int inputIndex = 0;
    if (keymap)
        inputIndex = reinterpret_cast<const Input::KeyMap*>(keymap)->GetInputForId(keyId, 0);  // B.3.7 bridge

    // 13. Get key name as UI::String
    uint8_t uiStr[16] = {};
    // LANE1 row 36: 0x140099c20 is unregistered, so fn_GetInputKeyName was
    // permanently null and every tutorial key name rendered blank. The function is
    // already ported byte-for-byte as Hotkey_BuildKeyName (hotkeytooltipui.cpp);
    // retail's param_1 is the KeyMap container and that port looks up the same
    // *g_pKeyMapSingleton, so this is the identical call, not a fourth copy.
    Game::Hotkey_BuildKeyName(uiStr, deviceType, inputIndex);

    // 14. Set text on the TextElement
    if (textRef)
        reinterpret_cast<UI::TextElement*>(textRef)->SetText(  // B.3.6 bridge
            reinterpret_cast<const UI::String*>(uiStr));

    return retIBP;
}

// create 0x140027d00: MemAlloc(0x48, 8), then the ctor.
void* TutorialImageHotKey::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialImageHotKey), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialImageHotKey());
}
// createInPlace 0x140027d40.
void* TutorialImageHotKey::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialImageHotKey());
}

DEFINE_EXE_TYPE(TutorialImageHotKey, "TutorialImage")
