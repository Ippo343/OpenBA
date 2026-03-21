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
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Tutorial/tutorialimageinputkey_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Vtable pointer (resolved once)
// ============================================================
static void* s_vtable_TutorialImageInputKey = nullptr;
static void* s_vtable_TutorialImage = nullptr;

static void EnsureVtables() {
    if (!s_vtable_TutorialImageInputKey) {
        s_vtable_TutorialImageInputKey = (void*)ResolveAddr(Module::Exe, TutorialImageInputKeyAddr::Vtable);
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

static void EnsureFuncs() {
    if (!s_fn_External_Ctor) {
        // External::External ctor from dispatch slot
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151e98));
        if (dispatch && *dispatch)
            s_fn_External_Ctor = reinterpret_cast<fn_External_Ctor_t>(*dispatch);
    }
    if (!s_fn_ExternalPtr_SerializeImage) {
        s_fn_ExternalPtr_SerializeImage = reinterpret_cast<fn_ExternalPtr_SerializeImage_t>(
            ResolveAddr(Module::Exe, 0x140027ad0));
    }
}

// ============================================================
// Slot0: Dtor (0x140027fa0)
// ============================================================
void* TutorialImageInputKey_Dtor(void* self, uint32_t flags) {
    EnsureVtables();
    auto p = reinterpret_cast<uint8_t*>(self);

    // Set own vtable (then immediately overwrite with base -- matches original)
    *(void**)p = s_vtable_TutorialImageInputKey;
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
// Slot5: Serialize (0x1400e07f0)
// ============================================================
void TutorialImageInputKey_Serialize(void* self, void* stream) {
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

    // Second serializer block: serialize +0x30 (inputId) via stream vtable +0x98
    uint8_t serializerBuf2[32] = {};
    if (g_fn_Serializer_Ctor)
        g_fn_Serializer_Ctor(serializerBuf2, stream, 0);

    // Get the internal stream pointer from the serializer (at offset +0x08 in serializer)
    void* innerStream = *(void**)(serializerBuf2 + 0x08);
    if (innerStream) {
        // Call virtual method at vtable +0x98 on the stream: Serialize(stream, &field, nullptr)
        void** streamVtable = *(void***)innerStream;
        typedef void (*fn_StreamSerialize_t)(void* stream, void* field, void* name);
        fn_StreamSerialize_t streamSerialize = reinterpret_cast<fn_StreamSerialize_t>(
            streamVtable[0x98 / 8]);
        if (streamSerialize)
            streamSerialize(innerStream, p + 0x30, nullptr);
    }

    if (g_fn_Serializer_Dtor)
        g_fn_Serializer_Dtor(serializerBuf2);
}

// ============================================================
// Slot6: Cleanup (0x140027f50)
// ============================================================
void* TutorialImageInputKey_Cleanup(void* self) {
    EnsureVtables();
    EnsureFuncs();
    auto p = reinterpret_cast<uint8_t*>(self);

    // Call own Dtor with flags=0 via vtable slot 0 (scalar dtor)
    void** vtable = *(void***)self;
    typedef void* (*fn_ScalarDtor_t)(void*, uint32_t);
    fn_ScalarDtor_t scalarDtor = reinterpret_cast<fn_ScalarDtor_t>(vtable[0]);
    scalarDtor(self, 0);

    // Reinitialize: call External ctor
    if (s_fn_External_Ctor)
        s_fn_External_Ctor(self);

    // Set base vtable, zero base fields
    *(void**)p = s_vtable_TutorialImage;
    *(uint64_t*)(p + 0x20) = 0;
    *(uint64_t*)(p + 0x28) = 0;

    // Set own vtable
    *(void**)p = s_vtable_TutorialImageInputKey;

    return self;
}

// ============================================================
// Slot8: Ctor/Init (0x140027f00)
// ============================================================
void* TutorialImageInputKey_Ctor(void* self) {
    EnsureVtables();
    EnsureFuncs();

    if (!self)
        return nullptr;

    auto p = reinterpret_cast<uint8_t*>(self);

    // Call External::External ctor
    if (s_fn_External_Ctor)
        s_fn_External_Ctor(self);

    // Set base vtable
    *(void**)p = s_vtable_TutorialImage;

    // Zero base fields
    *(uint64_t*)(p + 0x20) = 0;
    *(uint64_t*)(p + 0x28) = 0;

    // Set own vtable
    *(void**)p = s_vtable_TutorialImageInputKey;

    return self;
}

// ============================================================
// Slot9: Slot9/Update (0x1400e06e0)
// Complex UI operation -- calls via trampoline for now.
// Uses: Dialog::CreateElement, Element::FindElementType,
//       IsDerivedFrom, InternalBasePtr_Ctor, KeyMap::GetDeviceForId,
//       KeyMap::GetInputForId, FUN_140099c20, TextElement::SetText
// ============================================================
// Resolved to original function for passthrough
static void* s_fn_Slot9_Original = nullptr;

void TutorialImageInputKey_Slot9(void* self, void* param1, void* param2) {
    if (!s_fn_Slot9_Original) {
        s_fn_Slot9_Original = (void*)ResolveAddr(Module::Exe, 0x1400e06e0);
    }
    // Complex UI logic with many engine dependencies -- delegate to original
    typedef void (*fn_Slot9_t)(void* self, void* param1, void* param2);
    if (s_fn_Slot9_Original)
        reinterpret_cast<fn_Slot9_t>(s_fn_Slot9_Original)(self, param1, param2);
}
