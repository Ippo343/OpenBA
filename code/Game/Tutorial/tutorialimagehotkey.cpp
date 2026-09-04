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

#include "Tutorial/tutorialimagehotkey_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// hotkeytooltipui.cpp - the existing byte-faithful port of exe 0x140099c20.
namespace Game { void Hotkey_BuildKeyName(void* retval, int device, int keyIndex); }

// ============================================================
// Local function pointers resolved on first use
// ============================================================

// TutorialImage External ref serialize helper (0x140027ad0)
// Serializes/deserializes an External pointer with factory lookup
typedef void (*fn_ExternalRef_Serialize_t)(void* externalPtr, void* serializer);
static fn_ExternalRef_Serialize_t s_fn_ExternalRef_Serialize = nullptr;

// FUN_140027de0 — resolves hotkey string to External ref via ExternalFactory
typedef void (*fn_ResolveHotkeyRef_t)(void* stringField);
static fn_ResolveHotkeyRef_t s_fn_ResolveHotkeyRef = nullptr;

static bool s_funcsResolved = false;

static void EnsureFuncsResolved() {
    if (s_funcsResolved) return;
    s_funcsResolved = true;

    s_fn_ExternalRef_Serialize = (fn_ExternalRef_Serialize_t)
        ResolveAddr(Module::Exe, 0x140027ad0);
    s_fn_ResolveHotkeyRef = (fn_ResolveHotkeyRef_t)
        ResolveAddr(Module::Exe, 0x140027de0);
}

// ============================================================
// TutorialImageHotKey::Dtor (vtable +0x00, 0x140027d70)
// ============================================================
// Scalar deleting destructor.
// 1. Set vtable to TutorialImageHotKey
// 2. Destroy String at +0x30
// 3. Set vtable to TutorialImage (base)
// 4. Call External::~External
// 5. If (flags & 1), free memory
void* TutorialImageHotKey_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Set vtable to TutorialImageHotKey
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialImageHotKeyAddr::Vtable);
    *(uintptr_t*)s = vtable;

    // Destroy String at +0x30
    if (fn_String_Dtor) {
        fn_String_Dtor((void*)(s + 0x30));
    }

    // Set vtable to TutorialImage (base)
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialImageAddr::Vtable);
    *(uintptr_t*)s = baseVtable;

    // Call External::~External
    if (fn_External_Dtor) {
        fn_External_Dtor(self);
    }

    // Conditional free
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// TutorialImageHotKey::Serialize (vtable +0x28, 0x1400e0540)
// ============================================================
// Two-phase serialization:
// Phase 1: Serialize base TutorialImage fields (int +0x28, int +0x2c, external ref +0x20)
// Phase 2: Serialize HotKey string +0x30, resolve external ref on load
void TutorialImageHotKey_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;
    EnsureFuncsResolved();

    // Phase 1: Base TutorialImage fields
    uint8_t ser1[32];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(ser1, stream, 0);
    }

    // Serialize int at +0x28
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(ser1, (void*)(s + 0x28), nullptr);
    }

    // Serialize int at +0x2c
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(ser1, (void*)(s + 0x2c), nullptr);
    }

    // Serialize external ref at +0x20
    if (s_fn_ExternalRef_Serialize) {
        s_fn_ExternalRef_Serialize((void*)(s + 0x20), ser1);
    }

    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(ser1);
    }

    // Phase 2: HotKey-specific fields
    uint8_t ser2[32];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(ser2, stream, 0);
    }

    // Serialize String at +0x30
    if (g_fn_String_Serialize) {
        g_fn_String_Serialize((void*)(s + 0x30), ser2);
    }

    // If loading (version == 0), resolve the hotkey string to an External ref
    if (g_fn_Serializer_GetVersion) {
        int version = g_fn_Serializer_GetVersion(ser2);
        if (version == 0) {
            if (s_fn_ResolveHotkeyRef) {
                s_fn_ResolveHotkeyRef((void*)(s + 0x30));
            }
        }
    }

    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(ser2);
    }
}

// ============================================================
// TutorialImageHotKey::Cleanup (vtable +0x30, 0x140027d50)
// ============================================================
// Calls dtor(self, 0) via vtable, then re-initializes via Ctor.
void TutorialImageHotKey_Cleanup(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Call ScalarDtor(self, 0) via vtable — destroys fields
    uintptr_t* vtable = *(uintptr_t**)s;
    typedef void* (*DtorFn)(void*, uint32_t);
    DtorFn dtor = (DtorFn)vtable[0];
    dtor(self, 0);

    // Re-initialize
    TutorialImageHotKey_Ctor(self);
}

// ============================================================
// TutorialImageHotKey::Ctor (real at 0x1400e0680)
// ============================================================
// Called by factory after allocation and by Cleanup for re-init.
// 1. Call External::External base init
// 2. Set vtable to TutorialImage, zero base fields
// 3. Set vtable to TutorialImageHotKey, zero derived fields
void* TutorialImageHotKey_Ctor(void* self) {
    if (self == nullptr) {
        return nullptr;
    }

    uintptr_t s = (uintptr_t)self;

    // Call External::External base init
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable to TutorialImage (base), zero base fields
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialImageAddr::Vtable);
    *(uintptr_t*)s = baseVtable;
    *(uint64_t*)(s + 0x20) = 0;  // imageRef = null
    *(uint64_t*)(s + 0x28) = 0;  // field28 + field2c = 0

    // Set vtable to TutorialImageHotKey, init derived fields
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialImageHotKeyAddr::Vtable);
    *(uintptr_t*)s = myVtable;
    *(uint64_t*)(s + 0x30) = 0;       // hotkeyName = empty string
    *(int32_t*)(s + 0x38) = -1;       // field38 = -1 sentinel
    *(uint64_t*)(s + 0x40) = 0;       // field40 = null

    return self;
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
void* TutorialImageHotKey_Slot9(void* self, void* retIBP, void* dialog) {
    EnsureFuncsResolved();
    uintptr_t s = (uintptr_t)self;

    // 1. Create UI element from imageRef at +0x20
    if (fn_Dialog_CreateElement)
        fn_Dialog_CreateElement(dialog, retIBP, *(void**)(s + 0x20));

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

    // 7. Resolve hotkey ref from string at +0x30
    if (s_fn_ResolveHotkeyRef)
        s_fn_ResolveHotkeyRef((void*)(s + 0x30));

    // 8. Get actual KeyMap object (triple deref from exref)
    void* keymap = nullptr;
    if (g_pKeyMapObjectExref && *g_pKeyMapObjectExref) {
        void* step2 = *(void**)*g_pKeyMapObjectExref;
        if (step2) keymap = *(void**)step2;
    }

    // 9. Read key ID from resolved ref at field40->+0x40
    void* resolvedRef = *(void**)(s + 0x40);
    int keyId = 0;
    if (resolvedRef)
        keyId = *(int*)((uintptr_t)resolvedRef + 0x40);

    // 10. Get device type for this key
    int deviceType = 0;
    if (fn_KeyMap_GetDeviceForId && keymap)
        deviceType = fn_KeyMap_GetDeviceForId(keymap, keyId, 0);

    // 11. Resolve hotkey ref again (matches original)
    if (s_fn_ResolveHotkeyRef)
        s_fn_ResolveHotkeyRef((void*)(s + 0x30));

    // 12. Get input index for this key
    int inputIndex = 0;
    if (fn_KeyMap_GetInputForId && keymap)
        inputIndex = fn_KeyMap_GetInputForId(keymap, keyId, 0);

    // 13. Get key name as UI::String
    uint8_t uiStr[16] = {};
    // LANE1 row 36: 0x140099c20 is unregistered, so fn_GetInputKeyName was
    // permanently null and every tutorial key name rendered blank. The function is
    // already ported byte-for-byte as Hotkey_BuildKeyName (hotkeytooltipui.cpp);
    // retail's param_1 is the KeyMap container and that port looks up the same
    // *g_pKeyMapSingleton, so this is the identical call, not a fourth copy.
    Game::Hotkey_BuildKeyName(uiStr, deviceType, inputIndex);

    // 14. Set text on the TextElement
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(&textRef);
    if (fn_TextElement_SetText && textRef)
        fn_TextElement_SetText(textRef, uiStr);

    return retIBP;
}

// ============================================================
// Standalone factory + vtable — External-resource family (factory cascade).
// Vtable @ 0x140160ab0 (9 slots, Object::External base), size 0x48, parent "TutorialImage".
// GetFileVersion (+0x18) = 0x1001. MusicList pattern (musiclist.cpp).
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TutorialImageHotKey_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialImageHotKey");
    return c;
}
static int TutorialImageHotKey_GetFileVersion(void*) { return 0x1001; }

static void* g_TutorialImageHotKey_Vtable[10] = {
    (void*)&TutorialImageHotKey_Dtor,            // +0x00 Dtor
    nullptr,                      // +0x08 (base thunk)
    (void*)&TutorialImageHotKey_GetTypeId,       // +0x10 GetTypeId
    (void*)&TutorialImageHotKey_GetFileVersion,  // +0x18 GetFileVersion
    nullptr,                      // +0x20 (shared base thunk 0x14003a912)
    (void*)&TutorialImageHotKey_Serialize,       // +0x28 Serialize
    (void*)&TutorialImageHotKey_Cleanup,         // +0x30 Cleanup
    nullptr,                      // +0x38 (base 0x140001640)
    (void*)&TutorialImageHotKey_Ctor,   // +0x40 Ctor/CreateInPlace (0x140027d40)
    (void*)&TutorialImageHotKey_Slot9,   // +0x48 BuildElement (0x1400e0420)
};

extern "C" void RegisterTutorialImageHotKeyVtable() {
    RegisterExeAddr(0x140160ab0, (uintptr_t)g_TutorialImageHotKey_Vtable);
}
extern "C" void* TutorialImageHotKey_CreateInPlace(void* mem) {
    return TutorialImageHotKey_Ctor(mem);
}
extern "C" void* TutorialImageHotKey_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    return p ? TutorialImageHotKey_Ctor(p) : nullptr;
}
REGISTER_TYPE(TutorialImageHotKey, "TutorialImage", &TutorialImageHotKey_Factory, &TutorialImageHotKey_CreateInPlace);
