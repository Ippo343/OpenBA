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

#include "Tutorial/tutorialimageinputkey_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// hotkeytooltipui.cpp - the existing byte-faithful port of exe 0x140099c20.
namespace Game { void Hotkey_BuildKeyName(void* retval, int device, int keyIndex); }

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
// ============================================================
// Creates a UI element from imageRef, finds a TextElement child,
// looks up the bound key name from KeyMap using inputId at +0x30,
// and sets the display text on the TextElement.
//
// Params (Win64 ABI):
//   self = TutorialImageInputKey*
//   retIBP = InternalBasePtr* (return buffer, also output)
//   dialog = Dialog* (parent dialog)
void TutorialImageInputKey_Slot9(void* self, void* retIBP, void* dialog) {
    EnsureFuncs();
    auto p = reinterpret_cast<uint8_t*>(self);

    // 1. Create UI element from imageRef at +0x20
    if (fn_Dialog_CreateElement)
        fn_Dialog_CreateElement(dialog, retIBP, *(void**)(p + 0x20));

    // 2. CheckPtr on the returned IBP
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(retIBP);

    // 3. FindElementType — find TextElement child
    void* element = *(void**)retIBP;
    uint8_t foundIBP[16] = {};
    if (fn_Element_FindElementType && g_TypeId_TextElement)
        fn_Element_FindElementType(element, foundIBP, *g_TypeId_TextElement);

    // 4. CheckPtr on found element
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(foundIBP);

    // 5. IsDerivedFrom check — verify it's a TextElement
    void* foundObj = *(void**)foundIBP;
    void* textElement = nullptr;
    if (fn_IsDerivedFrom && g_TypeId_TextElement && foundObj) {
        if (fn_IsDerivedFrom(foundObj, *g_TypeId_TextElement))
            textElement = foundObj;
    }

    // 6. Wrap in InternalBasePtr
    void* textRef = nullptr;
    if (fn_InternalBasePtr_Ctor)
        fn_InternalBasePtr_Ctor(&textRef, textElement);

    // 7. Get actual KeyMap object (triple deref from exref)
    void* keymap = nullptr;
    if (g_pKeyMapObjectExref && *g_pKeyMapObjectExref) {
        void* step2 = *(void**)*g_pKeyMapObjectExref;
        if (step2) keymap = *(void**)step2;
    }

    // 8. Get device type and input index for inputId at +0x30
    int inputId = *(int*)(p + 0x30);
    int deviceType = 0;
    if (fn_KeyMap_GetDeviceForId && keymap)
        deviceType = fn_KeyMap_GetDeviceForId(keymap, inputId, 0);

    int inputIndex = 0;
    if (fn_KeyMap_GetInputForId && keymap)
        inputIndex = fn_KeyMap_GetInputForId(keymap, inputId, 0);

    // 9. Get key name as UI::String
    uint8_t uiStr[16] = {};
    // LANE1 row 36: 0x140099c20 is unregistered, so fn_GetInputKeyName was
    // permanently null and every tutorial key name rendered blank. The function is
    // already ported byte-for-byte as Hotkey_BuildKeyName (hotkeytooltipui.cpp);
    // retail's param_1 is the KeyMap container and that port looks up the same
    // *g_pKeyMapSingleton, so this is the identical call, not a fourth copy.
    Game::Hotkey_BuildKeyName(uiStr, deviceType, inputIndex);

    // 10. Set text on the TextElement
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(&textRef);
    if (fn_TextElement_SetText && textRef)
        fn_TextElement_SetText(textRef, uiStr);
}

// ============================================================
// Standalone factory + vtable — External-resource family (factory cascade).
// Vtable @ 0x140160b20 (9 slots, Object::External base), size 0x38, parent "TutorialImage".
// GetFileVersion (+0x18) = 0x1001. MusicList pattern (musiclist.cpp).
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TutorialImageInputKey_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialImageInputKey");
    return c;
}
static int TutorialImageInputKey_GetFileVersion(void*) { return 0x1001; }

static void* g_TutorialImageInputKey_Vtable[9] = {
    (void*)&TutorialImageInputKey_Dtor,            // +0x00 Dtor
    nullptr,                      // +0x08 (base thunk)
    (void*)&TutorialImageInputKey_GetTypeId,       // +0x10 GetTypeId
    (void*)&TutorialImageInputKey_GetFileVersion,  // +0x18 GetFileVersion
    nullptr,                      // +0x20 (shared base thunk 0x14003a912)
    (void*)&TutorialImageInputKey_Serialize,       // +0x28 Serialize
    (void*)&TutorialImageInputKey_Cleanup,         // +0x30 Cleanup
    nullptr,                      // +0x38 (base 0x140001640)
    (void*)&TutorialImageInputKey_Ctor,              // +0x40 Ctor/CreateInPlace
};

extern "C" void RegisterTutorialImageInputKeyVtable() {
    RegisterExeAddr(0x140160b20, (uintptr_t)g_TutorialImageInputKey_Vtable);
}
extern "C" void* TutorialImageInputKey_CreateInPlace(void* mem) {
    return TutorialImageInputKey_Ctor(mem);
}
extern "C" void* TutorialImageInputKey_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    return p ? TutorialImageInputKey_Ctor(p) : nullptr;
}
REGISTER_TYPE(TutorialImageInputKey, "TutorialImage", &TutorialImageInputKey_Factory, &TutorialImageInputKey_CreateInPlace);
