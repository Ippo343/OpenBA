// code/Game/Tutorial/tutorialimageinputkey.cpp
// Clean reimplementation of TutorialImageInputKey vtable methods.
//
// TutorialImageInputKey (0x38 bytes) -- input key image display.
// Vtable: 0x140160b20, extends TutorialImage.
//
// Fields (from Ghidra decompilation):
//   +0x00: vtable ptr
//   +0x08: External base (0x18 bytes)
//   +0x20: ExternalPtr (image resource ref)
//   +0x28: int32 (serialized)
//   +0x2C: int32 (serialized)
//   +0x30: int32 inputId (serialized via stream vtable)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/TutorialImageInputKey.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
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
// Exe helper, resolved PER CALL (ENDGAME B.3.8, B6-5). The External ctor
// dispatch slot and the two vtable addresses this file cached lazily are
// gone: the compiler runs External() and stamps both tables.
// ============================================================

// TutorialImage_sub_7ad0 -- ExternalPtr serialize helper (exe 0x140027ad0)
typedef void (*fn_ExternalPtr_SerializeImage_t)(void* externalPtrField, void* serializer);
static fn_ExternalPtr_SerializeImage_t ExternalPtrSerializeImageFn() {
    return reinterpret_cast<fn_ExternalPtr_SerializeImage_t>(ResolveAddr(Module::Exe, 0x140027ad0));
}

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// ctor 0x1400e08f0: External(), TutorialImage's stamp + zeroed fields
// (TutorialImage()), the own stamp. +0x30 is NOT written (retail's).
TutorialImageInputKey::TutorialImageInputKey() {}

// [0] 0x140027fa0: stamp, stamp TutorialImage, ~External, MemFree on &1.
Object::Base* TutorialImageInputKey::DeletingDtor(unsigned flags) {
    this->~TutorialImageInputKey();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// Slot5: Serialize (0x1400e07f0)
// ============================================================
bool TutorialImageInputKey::Serialize(void* stream) {
    auto p = reinterpret_cast<uint8_t*>(this);

    // First serializer block: serialize +0x28, +0x2C as ints, then ExternalPtr at +0x20
    alignas(8) uint8_t serializerBuf[32] = {};
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int*>(p + 0x28), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int*>(p + 0x2C), nullptr);   // B.3.4 bridge

    if (auto serializeImage = ExternalPtrSerializeImageFn())
        serializeImage(p + 0x20, serializerBuf);

    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge

    // Second serializer block: serialize +0x30 (inputId) via stream vtable +0x98
    alignas(8) uint8_t serializerBuf2[32] = {};
    ::new (static_cast<void*>(serializerBuf2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // +0x30 (inputId) through the UINT door. Serializer+0x08 is NOT "the
    // internal stream": the Stream is at Serializer+0x00, and +0x08 is the
    // READERWRITER, whose vtable +0x98 is rw_vtbl::kScalarUInt.
    reinterpret_cast<IO::Serializer*>(serializerBuf2)->Serialize(*reinterpret_cast<unsigned int*>(p + 0x30), nullptr);   // B.3.4 bridge  (0x98 = uint door)

    reinterpret_cast<IO::Serializer*>(serializerBuf2)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// Slot6: Reconstruct (0x140027f50) / Slot8: Construct (0x140027f00)
// ============================================================
Object::External* TutorialImageInputKey::Reconstruct() {
    DeletingDtor(0);
    return ::new (static_cast<void*>(this)) TutorialImageInputKey();
}
Object::External* TutorialImageInputKey::Construct() {
    return ::new (static_cast<void*>(this)) TutorialImageInputKey();
}

// ============================================================
// Slot9: Slot9/Update (0x1400e06e0)
// ============================================================
// Creates a UI element from imageRef, finds a TextElement child,
// looks up the bound key name from KeyMap using inputId at +0x30,
// and sets the display text on the TextElement.
//
// Params (Win64 ABI):
//   self = TutorialImageInputKey*
//   retIBP = InternalBasePtr* (return buffer, also output)
//   dialog = Dialog* (parent dialog)
// Returns retIBP, exactly as retail does (every TutorialImage*::Slot9 ends
// `return param_2`). This is NOT cosmetic: TutWave_RefreshStepUI feeds the
// return value straight to TableElement::SetChild as the child pointer, so a
// void return leaves whatever the last call left in RAX and the table gets a
// garbage child, which faults later inside UI::UserInterface::Update.
void* TutorialImageInputKey::CreateElement(void* retIBP, void* dialog) {
    auto p = reinterpret_cast<uint8_t*>(this);

    // 1. Create UI element from imageRef at +0x20
    ::new (retIBP) Object::InternalPtr<UI::Element>(
        reinterpret_cast<const UI::Dialog*>(dialog)->CreateElement(  // B.3.6 bridge
            Object::ExternalPtr<UI::ElementDescription>(
                reinterpret_cast<UI::ElementDescription*>(*(void**)(p + 0x20)))));

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
    if (fn_IsDerivedFrom && foundObj) {
        if (fn_IsDerivedFrom(foundObj, UI::TextElement::TypeId()))
            textElement = foundObj;
    }

    // 6. Wrap in InternalBasePtr
    void* textRef = nullptr;
    if (fn_InternalBasePtr_Ctor)
        new (&textRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(textElement));   // B.3.3 bridge

    // 7. Get actual KeyMap object (triple deref from exref)
    void* keymap = nullptr;
    if (g_pInputConfigSingletonExref && *g_pInputConfigSingletonExref) {
        void* step2 = *(void**)*g_pInputConfigSingletonExref;
        if (step2) keymap = *(void**)step2;
    }

    // 8. Get device type and input index for inputId at +0x30
    int inputId = *(int*)(p + 0x30);
    int deviceType = 0;
    if (keymap)
        deviceType = reinterpret_cast<const Input::KeyMap*>(keymap)->GetDeviceForId(inputId, 0);  // B.3.7 bridge

    int inputIndex = 0;
    if (keymap)
        inputIndex = reinterpret_cast<const Input::KeyMap*>(keymap)->GetInputForId(inputId, 0);  // B.3.7 bridge

    // 9. Get key name as UI::String
    uint8_t uiStr[16] = {};
    // LANE1 row 36: 0x140099c20 is unregistered, so fn_GetInputKeyName was
    // permanently null and every tutorial key name rendered blank. The function is
    // already ported byte-for-byte as Hotkey_BuildKeyName (hotkeytooltipui.cpp);
    // retail's param_1 is the KeyMap container and that port looks up the same
    // *g_pKeyMapSingleton, so this is the identical call, not a fourth copy.
    Game::Hotkey_BuildKeyName(uiStr, deviceType, inputIndex);

    // 10. Set text on the TextElement
    if (textRef)
        reinterpret_cast<UI::TextElement*>(textRef)->SetText(  // B.3.6 bridge
            reinterpret_cast<const UI::String*>(uiStr));
    return retIBP;
}

// create 0x140027ea0: MemAlloc(0x38, 8), then the ctor.
void* TutorialImageInputKey::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialImageInputKey), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialImageInputKey());
}
// createInPlace 0x140027f00.
void* TutorialImageInputKey::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialImageInputKey());
}

DEFINE_EXE_VTABLE(TutorialImageInputKey, "_ZTV21TutorialImageInputKey")
DEFINE_EXE_TYPE(TutorialImageInputKey, "TutorialImage")
