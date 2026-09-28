// code/Game/Tutorial/tutorialimage.cpp
// Clean reimplementation of TutorialImage vtable methods.
//
// TutorialImage (0x30 bytes) — base tutorial image class.
// Vtable: 0x140160a40, extends Object::External.
//
// Fields (from Ghidra decompilation + Serialize analysis):
//   +0x00: void*    vtable
//   +0x08: uint8_t  _extBase[0x18]  — External base fields
//   +0x20: void*    imageRef         — External* to image resource
//   +0x28: int32_t  imageWidth       — image width
//   +0x2C: int32_t  imageHeight      — image height
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/TutorialImage.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/Dialog.h"         // ENDGAME B.3.6 UI callers (K4)
#include "ui/Element.h"        // ENDGAME B.3.6 UI callers (K4)

// ExternalPtr serializer for the imageRef field at +0x20 (exe 0x140027ad0).
// ENDGAME B.3.8 (B6-5): resolved per call, not through a lazily filled static
// (the loader thread serializes tutorial resources).
typedef void (*fn_ExternalPtr_SerializeImage_t)(void* fieldPtr, void* serializer);
static fn_ExternalPtr_SerializeImage_t ExternalPtrSerializeImageFn() {
    return reinterpret_cast<fn_ExternalPtr_SerializeImage_t>(ResolveAddr(Module::Exe, 0x140027ad0));
}

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// The ctor inlined in create 0x140027980 / createInPlace 0x1400279e0:
// External(), the stamp (the compiler's), the image fields zeroed.
TutorialImage::TutorialImage() : imageRef(nullptr), imageWidth(0), imageHeight(0) {}

// ============================================================
// TutorialImage::Serialize (vtable +0x28, 0x1400e0330)
// ============================================================
// imageWidth (+0x28), imageHeight (+0x2C), then the ExternalPtr at +0x20.
bool TutorialImage::Serialize(void* stream) {
    // Create Serializer on stack (24 bytes), version=0
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(imageWidth, nullptr);    // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(imageHeight, nullptr);   // B.3.4 bridge

    // Serialize ExternalPtr (imageRef) at +0x20
    if (auto serializeImage = ExternalPtrSerializeImageFn()) {
        serializeImage(static_cast<void*>(&imageRef), serializer);
    }

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// TutorialImage::Reconstruct [6] 0x140027a30 / Destruct [7] 0x140001640
// ============================================================
Object::External* TutorialImage::Reconstruct() {
    this->~TutorialImage();
    return ::new (static_cast<void*>(this)) TutorialImage();
}
void TutorialImage::Destruct() { this->~TutorialImage(); }

// ============================================================
// TutorialImage::Construct (vtable +0x40, 0x1400279e0)
// ============================================================
Object::External* TutorialImage::Construct() {
    return ::new (static_cast<void*>(this)) TutorialImage();
}

// ============================================================
// TutorialImage::CreateElement (vtable +0x48, 0x1400e0300)
// ============================================================
// Dialog::CreateElement(dialog, retval, imageRef); returns the RETBUF (retail
// `return param_2`). NOT cosmetic: TutWave_RefreshStepUI feeds the return
// value straight to TableElement::SetChild as the child pointer.
void* TutorialImage::CreateElement(void* ret, void* dialog) {
    ::new (ret) Object::InternalPtr<UI::Element>(
        reinterpret_cast<const UI::Dialog*>(dialog)->CreateElement(  // B.3.6 bridge
            Object::ExternalPtr<UI::ElementDescription>(
                reinterpret_cast<UI::ElementDescription*>(imageRef))));
    return ret;
}

// create 0x140027980: MemAlloc(0x30, 8), then the (inlined) ctor.
void* TutorialImage::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialImage), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialImage());
}
// createInPlace 0x1400279e0.
void* TutorialImage::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialImage());
}

DEFINE_EXE_TYPE(TutorialImage, "Object::External")
