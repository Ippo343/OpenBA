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

// Vtable pointer (resolved at runtime)
static void* s_vtable_TutorialImage = nullptr;

// ExternalPtr serializer for imageRef field at +0x20 (0x140027ad0)
typedef void (*fn_ExternalPtr_SerializeImage_t)(void* fieldPtr, void* serializer);
static fn_ExternalPtr_SerializeImage_t s_fn_ExternalPtr_SerializeImage = nullptr;

// Dialog::CreateElement dispatch (0x1401528c0)
typedef void (*fn_Dialog_CreateElement_t)(void* dialog, void* retval, void* imageRef);
static fn_Dialog_CreateElement_t s_fn_Dialog_CreateElement = nullptr;

static bool s_resolved = false;

static void EnsureResolved() {
    if (s_resolved) return;
    s_vtable_TutorialImage = (void*)ResolveAddr(Module::Exe, 0x140160a40);

    // ExternalPtr serialize for image resources (direct call target at 0x140027ad0)
    s_fn_ExternalPtr_SerializeImage = reinterpret_cast<fn_ExternalPtr_SerializeImage_t>(
        ResolveAddr(Module::Exe, 0x140027ad0));

    // Dialog::CreateElement — runtime dispatch slot at 0x1401528c0
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401528c0));
        if (dispatch && *dispatch) {
            s_fn_Dialog_CreateElement = reinterpret_cast<fn_Dialog_CreateElement_t>(*dispatch);
        }
    }

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
    uint8_t serializer[24];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Serialize imageWidth at +0x28
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(serializer, (void*)(s + 0x28), nullptr);
    }

    // Serialize imageHeight at +0x2C
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(serializer, (void*)(s + 0x2C), nullptr);
    }

    // Serialize ExternalPtr (imageRef) at +0x20
    if (s_fn_ExternalPtr_SerializeImage) {
        s_fn_ExternalPtr_SerializeImage((void*)(s + 0x20), serializer);
    }

    // Destroy Serializer
    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(serializer);
    }
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
void TutorialImage_Slot9(void* self, void* param1, void* param2) {
    uintptr_t s = (uintptr_t)self;
    EnsureResolved();

    // Read imageRef at +0x20
    void* imageRef = *(void**)(s + 0x20);

    // Call Dialog::CreateElement(dialog=param2, retval=param1, imageRef)
    if (s_fn_Dialog_CreateElement) {
        s_fn_Dialog_CreateElement(param2, param1, imageRef);
    }
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

static void* g_TutorialImage_Vtable[9] = {
    (void*)&TutorialImage_Dtor,            // +0x00 Dtor
    nullptr,                      // +0x08 (base thunk)
    (void*)&TutorialImage_GetTypeId,       // +0x10 GetTypeId
    (void*)&TutorialImage_GetFileVersion,  // +0x18 GetFileVersion
    nullptr,                      // +0x20 (shared base thunk 0x14003a912)
    (void*)&TutorialImage_Serialize,       // +0x28 Serialize
    (void*)&TutorialImage_Cleanup,         // +0x30 Cleanup
    nullptr,                      // +0x38 (base 0x140001640)
    (void*)&TutorialImage_Ctor,              // +0x40 Ctor/CreateInPlace
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
