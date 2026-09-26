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

#include "Tutorial/tutorialimage_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/Dialog.h"         // ENDGAME B.3.6 UI callers (K4)
#include "ui/Element.h"        // ENDGAME B.3.6 UI callers (K4)

// Vtable pointer (resolved at runtime)
static void* s_vtable_TutorialImage = nullptr;

// ExternalPtr serializer for imageRef field at +0x20 (0x140027ad0)
typedef void (*fn_ExternalPtr_SerializeImage_t)(void* fieldPtr, void* serializer);
static fn_ExternalPtr_SerializeImage_t s_fn_ExternalPtr_SerializeImage = nullptr;

static bool s_resolved = false;

static void EnsureResolved() {
    if (s_resolved) return;
    s_vtable_TutorialImage = (void*)ResolveAddr(Module::Exe, 0x140160a40);

    // ExternalPtr serialize for image resources (direct call target at 0x140027ad0)
    s_fn_ExternalPtr_SerializeImage = reinterpret_cast<fn_ExternalPtr_SerializeImage_t>(
        ResolveAddr(Module::Exe, 0x140027ad0));

    s_resolved = true;
}

// ============================================================
// TutorialImage::Dtor (vtable +0x00, 0x140027a80)
// ============================================================
// Scalar destructor: sets vtable, calls External::~External,
// optionally frees memory.
void* TutorialImage_Dtor(void* self, uint32_t flags) {
    EnsureResolved();
    uintptr_t s = (uintptr_t)self;

    // Set vtable to TutorialImage
    *(void**)s = s_vtable_TutorialImage;

    // Call External::~External base destructor
    if (fn_External_Dtor) {
        fn_External_Dtor(self);
    }

    // If bit 0 set, free memory
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// TutorialImage::Serialize (vtable +0x28, 0x1400e0330)
// ============================================================
// Creates Serializer on stack, serializes imageWidth(+0x28),
// imageHeight(+0x2C), then ExternalPtr at +0x20.
void TutorialImage_Serialize(void* self, void* stream) {
    EnsureResolved();
    uintptr_t s = (uintptr_t)self;

    // Create Serializer on stack (24 bytes), version=0
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize imageWidth at +0x28
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);   // B.3.4 bridge

    // Serialize imageHeight at +0x2C
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x2C), nullptr);   // B.3.4 bridge

    // Serialize ExternalPtr (imageRef) at +0x20
    if (s_fn_ExternalPtr_SerializeImage) {
        s_fn_ExternalPtr_SerializeImage((void*)(s + 0x20), serializer);
    }

    // Destroy Serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// TutorialImage::Cleanup (vtable +0x30, 0x140027a30)
// ============================================================
// Calls Dtor(self, 0) to run destructor without freeing,
// then re-initializes: calls External::External, sets vtable,
// zeroes imageRef/imageWidth/imageHeight.
void TutorialImage_Cleanup(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Call scalar dtor through vtable slot 0 with flags=0 (no free)
    void** vtbl = *(void***)s;
    if (vtbl && vtbl[0]) {
        typedef void* (*fn_ScalarDtor_t)(void*, uint32_t);
        ((fn_ScalarDtor_t)vtbl[0])(self, 0);
    }

    // Re-initialize External base
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable back to TutorialImage
    EnsureResolved();
    *(void**)s = s_vtable_TutorialImage;

    // Zero the image fields
    *(uint64_t*)(s + 0x20) = 0;  // imageRef
    *(uint64_t*)(s + 0x28) = 0;  // imageWidth + imageHeight
}

// ============================================================
// TutorialImage::Ctor (vtable +0x40, 0x1400279e0)
// ============================================================
// Factory constructor: if self is null, returns null.
// Otherwise calls External::External, sets vtable, zeroes fields.
void* TutorialImage_Ctor(void* self) {
    if (self == nullptr) {
        return nullptr;
    }

    EnsureResolved();
    uintptr_t s = (uintptr_t)self;

    // Call External::External base constructor
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable to TutorialImage
    *(void**)s = s_vtable_TutorialImage;

    // Zero the image fields
    *(uint64_t*)(s + 0x20) = 0;  // imageRef = nullptr
    *(uint64_t*)(s + 0x28) = 0;  // imageWidth = 0, imageHeight = 0

    return self;
}

// ============================================================
// TutorialImage::Slot9 (vtable +0x48, 0x1400e0300)
// ============================================================
// Creates a UI element for displaying the image.
// Calls Dialog::CreateElement(dialog, retval, self->imageRef).
// Parameters: self=RCX, retval=RDX, dialog=R8
// Returns retval (RDX).
// Returns retIBP, exactly as retail does (every TutorialImage*::Slot9 ends
// `return param_2`). This is NOT cosmetic: TutWave_RefreshStepUI feeds the
// return value straight to TableElement::SetChild as the child pointer, so a
// void return leaves whatever the last call left in RAX and the table gets a
// garbage child, which faults later inside UI::UserInterface::Update.
void* TutorialImage_Slot9(void* self, void* param1, void* param2) {
    uintptr_t s = (uintptr_t)self;
    EnsureResolved();

    // Read imageRef at +0x20
    void* imageRef = *(void**)(s + 0x20);

    // Call Dialog::CreateElement(dialog=param2, retval=param1, imageRef)
    ::new (param1) Object::InternalPtr<UI::Element>(
        reinterpret_cast<const UI::Dialog*>(param2)->CreateElement(  // B.3.6 bridge
            Object::ExternalPtr<UI::ElementDescription>(
                reinterpret_cast<UI::ElementDescription*>(imageRef))));
    return param1;   // retail returns the RETBUF (its param_2), which is our param1
}

// ============================================================
// Standalone factory + vtable — External-resource family (factory cascade).
// Vtable @ 0x140160a40 (9 slots, Object::External base), size 0x30, parent "Object::External".
// GetFileVersion (+0x18) = 0x1000. MusicList pattern (musiclist.cpp).
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TutorialImage_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialImage");
    return c;
}
static int TutorialImage_GetFileVersion(void*) { return 0x1000; }

static void* g_TutorialImage_Vtable[10] = {
    (void*)&TutorialImage_Dtor,            // +0x00 Dtor
    nullptr,                      // +0x08 (base thunk)
    (void*)&TutorialImage_GetTypeId,       // +0x10 GetTypeId
    (void*)&TutorialImage_GetFileVersion,  // +0x18 GetFileVersion
    nullptr,                      // +0x20 (shared base thunk 0x14003a912)
    (void*)&TutorialImage_Serialize,       // +0x28 Serialize
    (void*)&TutorialImage_Cleanup,         // +0x30 Cleanup
    nullptr,                      // +0x38 (base 0x140001640)
    (void*)&TutorialImage_Ctor,   // +0x40 Ctor/CreateInPlace (0x1400279e0)
    (void*)&TutorialImage_Slot9,   // +0x48 BuildElement (0x1400e0300)
};

extern "C" void RegisterTutorialImageVtable() {
    RegisterExeAddr(0x140160a40, (uintptr_t)g_TutorialImage_Vtable);
}
extern "C" void* TutorialImage_CreateInPlace(void* mem) {
    return TutorialImage_Ctor(mem);
}
extern "C" void* TutorialImage_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    return p ? TutorialImage_Ctor(p) : nullptr;
}
REGISTER_TYPE(TutorialImage, "Object::External", &TutorialImage_Factory, &TutorialImage_CreateInPlace);
