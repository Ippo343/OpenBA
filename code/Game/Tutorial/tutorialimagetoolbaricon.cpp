// code/Game/Tutorial/tutorialimagetoolbaricon.cpp
// Clean reimplementation of TutorialImageToolbarIcon vtable methods.
//
// TutorialImageToolbarIcon (0x48 bytes) -- toolbar icon image display.
// Vtable: 0x140160b90, extends TutorialImage.
//
// Fields (from Ghidra decompilation):
//   +0x00: vtable ptr
//   +0x08: External base (0x18 bytes)
//   +0x20: ExternalPtr (image resource ref)
//   +0x28: int32 (serialized)
//   +0x2C: int32 (serialized)
//   +0x30: System::String (toolbar name, serialized)
//   +0x38: int32 string tag / ExternalFactory version (-1 init)
//   +0x40: void* resolved Toolbar ptr (cached)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Tutorial/tutorialimagetoolbaricon_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Vtable pointers (resolved once)
// ============================================================
static void* s_vtable_TutorialImageToolbarIcon = nullptr;
static void* s_vtable_TutorialImage = nullptr;

static void EnsureVtables() {
    if (!s_vtable_TutorialImageToolbarIcon) {
        s_vtable_TutorialImageToolbarIcon = (void*)ResolveAddr(Module::Exe, TutorialImageToolbarIconAddr::Vtable);
        s_vtable_TutorialImage = (void*)ResolveAddr(Module::Exe, TutorialImageAddr::Vtable);
    }
}

// ============================================================
// Local function pointers (resolved once)
// ============================================================

// External::External() ctor -- dispatch slot 0x140151e98
typedef void (*fn_External_Ctor_t)(void* self);
static fn_External_Ctor_t s_fn_External_Ctor = nullptr;

// TutorialImage_sub_7ad0 -- ExternalPtr serialize helper (exe function)
typedef void (*fn_ExternalPtr_SerializeImage_t)(void* externalPtrField, void* serializer);
static fn_ExternalPtr_SerializeImage_t s_fn_ExternalPtr_SerializeImage = nullptr;

// System::Filename::Filename() -- dispatch slot 0x140151d40
typedef void* (*fn_Filename_Ctor_local_t)(void* self);
static fn_Filename_Ctor_local_t s_fn_Filename_Ctor = nullptr;

// System::Filename::~Filename() -- dispatch slot 0x1401529e8
typedef void (*fn_Filename_Dtor_local_t)(void* self);
static fn_Filename_Dtor_local_t s_fn_Filename_Dtor = nullptr;

// FUN_140027de0 -- resolves toolbar from String at +0x30
typedef void (*fn_ResolveToolbar_t)(void* stringField);
static fn_ResolveToolbar_t s_fn_ResolveToolbar = nullptr;

static void EnsureFuncs() {
    if (!s_fn_External_Ctor) {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151e98));
        if (dispatch && *dispatch)
            s_fn_External_Ctor = reinterpret_cast<fn_External_Ctor_t>(*dispatch);
    }
    if (!s_fn_ExternalPtr_SerializeImage) {
        s_fn_ExternalPtr_SerializeImage = reinterpret_cast<fn_ExternalPtr_SerializeImage_t>(
            ResolveAddr(Module::Exe, 0x140027ad0));
    }
    if (!s_fn_Filename_Ctor) {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151d40));
        if (dispatch && *dispatch)
            s_fn_Filename_Ctor = reinterpret_cast<fn_Filename_Ctor_local_t>(*dispatch);
    }
    if (!s_fn_Filename_Dtor) {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401529e8));
        if (dispatch && *dispatch)
            s_fn_Filename_Dtor = reinterpret_cast<fn_Filename_Dtor_local_t>(*dispatch);
    }
    if (!s_fn_ResolveToolbar) {
        s_fn_ResolveToolbar = reinterpret_cast<fn_ResolveToolbar_t>(
            ResolveAddr(Module::Exe, 0x140027de0));
    }
}

// ============================================================
// Slot0: Dtor (0x140028090)
// ============================================================
void* TutorialImageToolbarIcon_Dtor(void* self, uint32_t flags) {
    EnsureVtables();
    auto p = reinterpret_cast<uint8_t*>(self);

    // Set own vtable
    *(void**)p = s_vtable_TutorialImageToolbarIcon;

    // Destroy String at +0x30 (dispatch slot 0x140152af0 = String::~String)
    if (fn_String_Dtor)
        fn_String_Dtor(p + 0x30);

    // Set base vtable
    *(void**)p = s_vtable_TutorialImage;

    // Call External::~External
    if (fn_External_Dtor)
        fn_External_Dtor(self);

    // Conditional free
    if (flags & 1) {
        if (fn_MemFreeStatic)
            fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// Slot5: Serialize (0x1400e0af0)
// ============================================================
void TutorialImageToolbarIcon_Serialize(void* self, void* stream) {
    EnsureFuncs();
    auto p = reinterpret_cast<uint8_t*>(self);

    // First serializer block: serialize +0x28, +0x2C as ints, then ExternalPtr at +0x20
    uint8_t serializerBuf[32] = {};
    if (g_fn_Serializer_Ctor)
        g_fn_Serializer_Ctor(serializerBuf, stream, 0);

    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(serializerBuf, p + 0x28, nullptr);
        g_fn_Serialize_Int(serializerBuf, p + 0x2C, nullptr);
    }

    if (s_fn_ExternalPtr_SerializeImage)
        s_fn_ExternalPtr_SerializeImage(p + 0x20, serializerBuf);

    if (g_fn_Serializer_Dtor)
        g_fn_Serializer_Dtor(serializerBuf);

    // Second serializer block: Filename ctor, String::Serialize, conditional toolbar resolve
    uint8_t serializerBuf2[32] = {};
    if (g_fn_Serializer_Ctor)
        g_fn_Serializer_Ctor(serializerBuf2, stream, 0);

    // Filename ctor (stack local, used as temp)
    uint8_t filenameBuf[8] = {};
    if (s_fn_Filename_Ctor)
        s_fn_Filename_Ctor(filenameBuf);

    // String::Serialize on +0x30
    if (g_fn_String_Serialize)
        g_fn_String_Serialize(p + 0x30, serializerBuf2);

    // If loading (serializer stream mode == 0), resolve toolbar
    void* innerStream = *(void**)(serializerBuf2 + 0x00);
    if (innerStream) {
        int streamMode = *(int*)(reinterpret_cast<uint8_t*>(innerStream) + 0x08);
        if (streamMode == 0) {
            // Resolve toolbar from the string
            if (s_fn_ResolveToolbar)
                s_fn_ResolveToolbar(p + 0x30);
        }
    }

    // Filename dtor
    if (s_fn_Filename_Dtor)
        s_fn_Filename_Dtor(filenameBuf);

    if (g_fn_Serializer_Dtor)
        g_fn_Serializer_Dtor(serializerBuf2);
}

// ============================================================
// Slot6: Cleanup (0x140028070)
// ============================================================
void TutorialImageToolbarIcon_Cleanup(void* self) {
    // Call own Dtor with flags=0 via vtable slot 0
    void** vtable = *(void***)self;
    typedef void* (*fn_ScalarDtor_t)(void*, uint32_t);
    fn_ScalarDtor_t scalarDtor = reinterpret_cast<fn_ScalarDtor_t>(vtable[0]);
    scalarDtor(self, 0);

    // Reinitialize via real Ctor
    TutorialImageToolbarIcon_Ctor(self);
}

// ============================================================
// Slot8: Ctor/Init (real: 0x1400e0c30)
// ============================================================
void* TutorialImageToolbarIcon_Ctor(void* self) {
    EnsureVtables();
    EnsureFuncs();

    auto p = reinterpret_cast<uint8_t*>(self);

    // Call External::External ctor
    if (s_fn_External_Ctor)
        s_fn_External_Ctor(self);

    // Set base vtable, zero base fields
    *(void**)p = s_vtable_TutorialImage;
    *(uint64_t*)(p + 0x20) = 0;
    *(uint64_t*)(p + 0x28) = 0;

    // Set own vtable
    *(void**)p = s_vtable_TutorialImageToolbarIcon;

    // Initialize String at +0x30 (zero), tag at +0x38 (-1), toolbar ref at +0x40 (null)
    *(uint64_t*)(p + 0x30) = 0;
    *(int32_t*)(p + 0x38) = -1;
    *(uint64_t*)(p + 0x40) = 0;

    return self;
}

// ============================================================
// Slot9: Slot9/Update (0x1400e0940)
// Complex UI operation -- calls via original for now.
// Uses: Dialog::CreateElement, Element::FindElementType,
//       IsDerivedFrom, InternalBasePtr_Ctor, FUN_140027de0,
//       fn_Sprite_Ctor_Name, ImageElement::SetSprite,
//       toolbar component iteration
// ============================================================
static void* s_fn_Slot9_Original = nullptr;

void TutorialImageToolbarIcon_Slot9(void* self, void* param1, void* param2) {
    if (!s_fn_Slot9_Original) {
        s_fn_Slot9_Original = (void*)ResolveAddr(Module::Exe, 0x1400e0940);
    }
    // Complex UI logic with many engine dependencies -- delegate to original
    typedef void (*fn_Slot9_t)(void* self, void* param1, void* param2);
    if (s_fn_Slot9_Original)
        reinterpret_cast<fn_Slot9_t>(s_fn_Slot9_Original)(self, param1, param2);
}
